/*
 * Jim - zeromq command
 *
 * Copyright 2014 Florian Schäfer <florian.schaefer@gmail.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE JIM TCL PROJECT ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * JIM TCL PROJECT OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * The views and conclusions contained in the software and documentation
 * are those of the authors and should not be interpreted as representing
 * official policies, either expressed or implied, of the Jim Tcl Project.
 */

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
#include <unistd.h>
#include <sys/types.h>

#include <czmq.h>

#include "jimautoconf.h"
#include <jim.h>

static zctx_t *ctx = NULL;
static int ctx_refc = 0;

static void
zeromq_delete_context(Jim_Interp *interp, void *data)
{
    JIM_NOTUSED(interp);

    if (ctx) {
        ctx_refc--;

        if (ctx_refc == 0) {
            zctx_destroy(&ctx);
            ctx = NULL;
        }
    }
}

static void
zeromq_delete_socket(Jim_Interp *interp, void *data)
{
    JIM_NOTUSED(interp);

    void *socket = data;
    zsocket_destroy(ctx, socket);
}

static int
zeromq_socket_handler(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    void *socket = Jim_CmdPrivData(interp);

    int rc = 0;
    int option;

    const char * const options[] = {
        "bind", "close", "connect", "disconnect", "interrupted", "sockopt", "receive", "send", NULL
    };

    enum {
        OPT_BIND, OPT_CLOSE, OPT_CONNECT, OPT_DISCONNECT, OPT_INTERRUPTED, OPT_SOCKOPT, OPT_RECEIVE, OPT_SEND
    };

    if (argc < 2) {
        Jim_WrongNumArgs(interp, 1, argv, "method ?args ...?");
        return JIM_ERR;
    }

    if (Jim_GetEnum(interp, argv[1], options, &option, "zeromq.socket method", JIM_ERRMSG | JIM_ENUM_ABBREV) != JIM_OK)
        return JIM_ERR;

    if (option == OPT_BIND || option == OPT_CONNECT) {

        if (argc != 3) {
            Jim_WrongNumArgs(interp, 2, argv, "endpoint");
            return JIM_ERR;
        }

        const char *endpoint;
        endpoint = Jim_String(argv[2]);

        if (option == OPT_CONNECT) {
            rc = zsocket_connect(socket, endpoint);

            if (rc != 0) {
                Jim_SetResultString(interp, "Failed to connect to socket", -1);
                return JIM_ERR;
            }
        }
        else {
            rc = zsocket_bind(socket, endpoint);

            if (rc == -1) {
                Jim_SetResultString(interp, "Failed to bind socket", -1);
                return JIM_ERR;
            }

            Jim_SetResultInt(interp, rc);
        }

    }
    else if (option == OPT_CLOSE) {

        if (argc != 2) {
            Jim_WrongNumArgs(interp, 2, argv, "");
            return JIM_ERR;
        }

        Jim_DeleteCommand(interp, Jim_String(argv[0]));

    }
    else if (option == OPT_DISCONNECT) {

        if (argc != 3) {
            Jim_WrongNumArgs(interp, 2, argv, "endpoint");
            return JIM_ERR;
        }

        const char *endpoint;
        endpoint = Jim_String(argv[2]);

        rc = zsocket_disconnect(socket, endpoint);

        if (rc != 0) {
            Jim_SetResultString(interp, "Failed to diconnect from socket", -1);
            return JIM_ERR;
        }

    }
    else if (option == OPT_INTERRUPTED) {

        if (argc != 2) {
            Jim_WrongNumArgs(interp, 2, argv, "");
            return JIM_ERR;
        }

        Jim_SetResult(interp, Jim_NewIntObj(interp, zctx_interrupted));
        zctx_interrupted = 0;

    }
    else if (option == OPT_SOCKOPT) {

        if (argc < 3 || argc > 4) {
            Jim_WrongNumArgs(interp, 2, argv, "option ?value?");
            return JIM_ERR;
        }

        if (Jim_CompareStringImmediate(interp, argv[2], "BACKLOG")) {

            if (argc == 4) {

                long backlog;

                if (Jim_GetLong(interp, argv[3], &backlog) != JIM_OK) {
                    return JIM_ERR;
                }

                zsocket_set_backlog(socket, (int)backlog);

            }
            else {
                Jim_SetResult(interp, Jim_NewIntObj(interp, zsocket_backlog(socket)));
            }

        }
        else if (Jim_CompareStringImmediate(interp, argv[2], "IDENTITY")) {

            const char* identity = "";

            if (argc == 4) {
                if (zsocket_type(socket) == ZMQ_REQ
                &&  zsocket_type(socket) == ZMQ_REP
                &&  zsocket_type(socket) == ZMQ_DEALER
                &&  zsocket_type(socket) == ZMQ_ROUTER) {
                    identity = Jim_String(argv[3]);
                    zsocket_set_identity(socket, identity);
                }
            }
            else {
                identity = zsocket_identity(socket);
                Jim_SetResultString(interp, identity, -1);
                free((void*)identity);
            }

        }
        else if (Jim_CompareStringImmediate(interp, argv[2], "LINGER")) {

            if (argc == 4) {

                long linger;

                if (Jim_GetLong(interp, argv[3], &linger) != JIM_OK) {
                    return JIM_ERR;
                }

                zsocket_set_linger(socket, (int)linger);

            }
            else {
                Jim_SetResult(interp, Jim_NewIntObj(interp, zsocket_linger(socket)));
            }

        }
        else if (Jim_CompareStringImmediate(interp, argv[2], "RCVHWM")) {

            if (argc == 4) {

                long rcvhwm;

                if (Jim_GetLong(interp, argv[3], &rcvhwm) != JIM_OK) {
                    return JIM_ERR;
                }

                zsocket_set_rcvhwm(socket, (int)rcvhwm);

            }
            else {
                Jim_SetResult(interp, Jim_NewIntObj(interp, zsocket_rcvhwm(socket)));
            }

        }
        else if (Jim_CompareStringImmediate(interp, argv[2], "SNDHWM")) {

            if (argc == 4) {

                long sndhwm;

                if (Jim_GetLong(interp, argv[3], &sndhwm) != JIM_OK) {
                    return JIM_ERR;
                }

                zsocket_set_sndhwm(socket, (int)sndhwm);

            }
            else {
                Jim_SetResult(interp, Jim_NewIntObj(interp, zsocket_sndhwm(socket)));
            }

        }
        else if (Jim_CompareStringImmediate(interp, argv[2], "RCVTIMEO")) {

            if (argc == 4) {

                long rcvtimeo;

                if (Jim_GetLong(interp, argv[3], &rcvtimeo) != JIM_OK) {
                    return JIM_ERR;
                }

                zsocket_set_rcvtimeo(socket, (int)rcvtimeo);

            }
            else {
                Jim_SetResult(interp, Jim_NewIntObj(interp, zsocket_rcvtimeo(socket)));
            }

        }
        else if (Jim_CompareStringImmediate(interp, argv[2], "SNDTIMEO")) {

            if (argc == 4) {

                long sndtimeo;

                if (Jim_GetLong(interp, argv[3], &sndtimeo) != JIM_OK) {
                    return JIM_ERR;
                }

                zsocket_set_sndtimeo(socket, (int)sndtimeo);

            }
            else {
                Jim_SetResult(interp, Jim_NewIntObj(interp, zsocket_sndtimeo(socket)));
            }

        }
        else if (Jim_CompareStringImmediate(interp, argv[2], "SUBSCRIBE")) {

            const char* subscribe = "";

            if (argc == 4) {
                subscribe = Jim_String(argv[3]);
                zsocket_set_subscribe(socket, subscribe);
            }

        }
        else if (Jim_CompareStringImmediate(interp, argv[2], "UNSUBSCRIBE")) {

            const char* unsubscribe = "";

            if (argc == 4) {
                unsubscribe = Jim_String(argv[3]);
                zsocket_set_unsubscribe(socket, unsubscribe);
            }

        }
        else {
            Jim_WrongNumArgs(interp, 2, argv, "option=BACKLOG|IDENTITY|LINGER|RCVHWM|SNDHWM|SNDTIMEO|RCVTIMEO|SUBSCRIBE|UNSUBSCRIBE ?value?");
            return JIM_ERR;
        }

    }
    else if (option == OPT_RECEIVE) {

        if ((argc == 3 && !Jim_CompareStringImmediate(interp, argv[2], "-nowait")) || argc > 3) {
            Jim_WrongNumArgs(interp, 2, argv, "?-nowait?");
            return JIM_ERR;
        }

        int nowait = 0;
        const char *message = NULL;

        if (argc == 3) {
            nowait = 1;
        }

        if (nowait) {
            if (zsocket_poll(socket, 50)) {
                message = zstr_recv_nowait(socket);
            }
        }
        else {
            message = zstr_recv(socket);
        }

        if (message) {
            Jim_SetResultString(interp, message, -1);
            free((void*)message);
        }
        else {
            Jim_SetEmptyResult(interp);
        }

    }
    else if (option == OPT_SEND) {

        if (argc != 3) {
            Jim_WrongNumArgs(interp, 2, argv, "message");
            return JIM_ERR;
        }

        const char *message;
        message = Jim_String(argv[2]);
        rc = zstr_send(socket, "%s", message);

        if (rc != 0) {
            Jim_SetResultString(interp, "Failed to send message", -1);
            return JIM_ERR;
        }

    }

    return JIM_OK;
}

static int
zeromq_socket_new_cmd(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    if (argc != 2) {
        goto wrong_args;
    }

    int type = ZMQ_PUSH;

    if (Jim_CompareStringImmediate(interp, argv[1], "PUSH")) {
        type = ZMQ_PUSH;
    }
    else if (Jim_CompareStringImmediate(interp, argv[1], "PULL")) {
        type = ZMQ_PULL;
    }
    else if (Jim_CompareStringImmediate(interp, argv[1], "REP")) {
        type = ZMQ_REP;
    }
    else if (Jim_CompareStringImmediate(interp, argv[1], "REQ")) {
        type = ZMQ_REQ;
    }
    else if (Jim_CompareStringImmediate(interp, argv[1], "PUB")) {
        type = ZMQ_PUB;
    }
    else if (Jim_CompareStringImmediate(interp, argv[1], "SUB")) {
        type = ZMQ_SUB;
    }
    else if (Jim_CompareStringImmediate(interp, argv[1], "PAIR")) {
        type = ZMQ_PAIR;
    }
    else if (Jim_CompareStringImmediate(interp, argv[1], "STREAM")) {
        type = ZMQ_STREAM;
    }
    else {
        goto wrong_args;
    }

    if (ctx == NULL) {
        ctx = zctx_new();
    }

    if (ctx == NULL) {
        goto zmq_error;
    }

    ctx_refc++;

    void *socket = zsocket_new(ctx, type);
    if (socket == NULL) {
        goto zmq_error;
    }

    char buffer[60];
    snprintf(buffer, sizeof(buffer), "zeromq.socket%ld", Jim_GetId(interp));
    Jim_CreateCommand(interp, buffer, zeromq_socket_handler, socket, zeromq_delete_socket);

    Jim_SetResult(interp, Jim_MakeGlobalNamespaceName(interp, Jim_NewStringObj(interp, buffer, -1)));
    return JIM_OK;

wrong_args:

    Jim_WrongNumArgs(interp, 1, argv, "type=PUSH|PULL|REP|REQ|PUB|SUB|PAIR|STREAM");
    return JIM_ERR;

zmq_error:

    zeromq_delete_context(interp, (void*)ctx);

    Jim_SetResultFormatted(interp, "error %d: %s\n", errno, zmq_strerror(errno));
    return JIM_ERR;
}

static void
zeromq_delete_beacon(Jim_Interp *interp, void *data)
{
    JIM_NOTUSED(interp);

    zbeacon_t *beacon = (zbeacon_t *)data;
    zbeacon_destroy(&beacon);
}

static int
zeromq_beacon_handler(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    zbeacon_t *beacon = Jim_CmdPrivData(interp);

    int option;

    const char * const options[] = {
        "close", "hostname", "interval", "noecho", "publish", "silence", "subscribe", "unsubscribe", "socket", NULL
    };

    enum {
        OPT_CLOSE, OPT_HOSTNAME, OPT_INTERVAL, OPT_NOECHO, OPT_PUBLISH, OPT_SILENCE, OPT_SUBSCRIBE, OPT_UNSUBSCRIBE, OPT_SOCKET
    };

    if (argc < 2) {
        Jim_WrongNumArgs(interp, 1, argv, "method ?args ...?");
        return JIM_ERR;
    }

    if (Jim_GetEnum(interp, argv[1], options, &option, "zeromq.beacon method", JIM_ERRMSG | JIM_ENUM_ABBREV) != JIM_OK)
        return JIM_ERR;

    if (option == OPT_CLOSE) {

        if (argc != 2) {
            Jim_WrongNumArgs(interp, 2, argv, "");
            return JIM_ERR;
        }

        Jim_DeleteCommand(interp, Jim_String(argv[0]));

    }
    else if (option == OPT_HOSTNAME) {

        if (argc != 2) {
            Jim_WrongNumArgs(interp, 2, argv, "");
            return JIM_ERR;
        }

        const char *hostname;
        hostname = zbeacon_hostname(beacon);

        Jim_SetResultString(interp, hostname, -1);

    }
    else if (option == OPT_INTERVAL) {

        if (argc != 3) {
            Jim_WrongNumArgs(interp, 2, argv, "milliseconds");
            return JIM_ERR;
        }

        long interval;

        if (Jim_GetLong(interp, argv[2], &interval) != JIM_OK) {
            return JIM_ERR;
        }

        zbeacon_set_interval(beacon, (int)interval);

    }
    else if (option == OPT_NOECHO) {

        if (argc != 2) {
            Jim_WrongNumArgs(interp, 2, argv, "");
            return JIM_ERR;
        }

        zbeacon_noecho(beacon);

    }
    else if (option == OPT_PUBLISH) {

        if (argc != 3) {
            Jim_WrongNumArgs(interp, 2, argv, "message");
            return JIM_ERR;
        }

        const char *message;
        message = Jim_String(argv[2]);

        zbeacon_publish(beacon, (byte*)message, strlen(message));

    }
    else if (option == OPT_SILENCE) {

        if (argc != 2) {
            Jim_WrongNumArgs(interp, 2, argv, "");
            return JIM_ERR;
        }

        zbeacon_silence(beacon);

    }
    else if (option == OPT_SUBSCRIBE) {

        if (argc < 2 || argc > 3) {
            Jim_WrongNumArgs(interp, 2, argv, "filter");
            return JIM_ERR;
        }

        const char *filter = 0;
        int filter_len = 0;

        if (argc == 3) {
            filter = Jim_String(argv[2]);
            filter_len = strlen(filter);
        }

        zbeacon_subscribe(beacon, (byte*)filter, filter_len);

    }
    else if (option == OPT_UNSUBSCRIBE) {

        if (argc != 2) {
            Jim_WrongNumArgs(interp, 2, argv, "");
            return JIM_ERR;
        }

        zbeacon_unsubscribe(beacon);

    }
    else if (option == OPT_SOCKET) {

        if (argc != 2) {
            Jim_WrongNumArgs(interp, 2, argv, "");
            return JIM_ERR;
        }

        void *socket = zbeacon_socket(beacon);

        char buffer[60];
        snprintf(buffer, sizeof(buffer), "zeromq.socket%ld", Jim_GetId(interp));
        Jim_CreateCommand(interp, buffer, zeromq_socket_handler, socket, NULL);

        Jim_SetResult(interp, Jim_MakeGlobalNamespaceName(interp, Jim_NewStringObj(interp, buffer, -1)));

    }

    return JIM_OK;
}

static int
zeromq_beacon_new_cmd(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    if (argc < 2 || argc > 3) {
        goto wrong_args;
    }

    long port;

    if (Jim_GetLong(interp, argv[1], &port) != JIM_OK) {
        goto wrong_args;
    }

    if (argc == 3) {
        const char* interface = Jim_String(argv[2]);
        Jim_IncrRefCount(argv[2]);
        zsys_set_interface((char*)interface);
    }

#ifdef HAVE_GETEUID
    if (port < 1024 && geteuid() != 0) {
        Jim_SetResultFormatted(interp, "error: %s\n", "user does not have permission to use privileged ports");
        return JIM_ERR;
    }
#endif

    zbeacon_t *beacon = zbeacon_new(port);

    if (beacon == NULL) {
        goto zmq_error;
    }

    char buffer[60];
    snprintf(buffer, sizeof(buffer), "zeromq.beacon%ld", Jim_GetId(interp));
    Jim_CreateCommand(interp, buffer, zeromq_beacon_handler, beacon, zeromq_delete_beacon);

    Jim_SetResult(interp, Jim_MakeGlobalNamespaceName(interp, Jim_NewStringObj(interp, buffer, -1)));
    return JIM_OK;

wrong_args:

    Jim_WrongNumArgs(interp, 1, argv, "port ?interface?");
    return JIM_ERR;

zmq_error:

    Jim_SetResultFormatted(interp, "error %d: %s\n", errno, zmq_strerror(errno));
    return JIM_ERR;
}

int
Jim_zeromqInit(Jim_Interp *interp)
{
    if (Jim_PackageProvide(interp, "zeromq", "0.1", JIM_ERRMSG))
        return JIM_ERR;

    Jim_CreateCommand(interp, "zeromq.socket.new", zeromq_socket_new_cmd, NULL, zeromq_delete_context);
    Jim_CreateCommand(interp, "zeromq.beacon.new", zeromq_beacon_new_cmd, NULL, NULL);

    return JIM_OK;
}
