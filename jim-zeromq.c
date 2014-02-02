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

#include <czmq.h>

#include <jim.h>

#define SEND_BUFFER_SIZE 8192

static zctx_t *ctx = NULL;

static void
JimZeromqDelContextProc(Jim_Interp *interp, void *privData)
{
    JIM_NOTUSED(interp);
	zctx_destroy(&ctx);
}

static void
JimZeromqDelSocketProc(Jim_Interp *interp, void *privData)
{
    void *socket = privData;
    JIM_NOTUSED(interp);
	zsocket_destroy(ctx, socket);
}

static int
JimZeromqHandlerCommand(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    void *socket = Jim_CmdPrivData(interp);

    int option;
    static const char * const options[] = {
        "close", "send", "interrupted", "receive", NULL
    };
    enum
    { OPT_CLOSE, OPT_SEND, OPT_INTERRUPTED, OPT_RECEIVE };

    if (argc < 2) {
        Jim_WrongNumArgs(interp, 1, argv, "method ?args ...?");
        return JIM_ERR;
    }
    if (Jim_GetEnum(interp, argv[1], options, &option, "zeromq method", JIM_ERRMSG) != JIM_OK)
        return JIM_ERR;
    /* CLOSE */
    if (option == OPT_CLOSE) {
        if (argc != 2) {
            Jim_WrongNumArgs(interp, 2, argv, "");
            return JIM_ERR;
        }
        Jim_DeleteCommand(interp, Jim_String(argv[0]));
    }
    else if (option == OPT_SEND) {
        if (argc != 3) {
            Jim_WrongNumArgs(interp, 2, argv, "message");
            return JIM_ERR;
        }

        const char *message = Jim_String(argv[2]);
	    zstr_send(socket, message);
    }
    else if (option == OPT_INTERRUPTED) {
        if (argc != 2) {
            Jim_WrongNumArgs(interp, 2, argv, "");
            return JIM_ERR;
        }

        Jim_SetResult(interp, Jim_NewIntObj(interp, zctx_interrupted));

        zctx_interrupted = 0;

    }
    else if (option == OPT_RECEIVE) {
        if (argc != 2) {
            Jim_WrongNumArgs(interp, 2, argv, "");
            return JIM_ERR;
        }

        const char *message = NULL;

        //if (zsocket_poll(socket, 200))
        message = zstr_recv(socket);

        if (message)
            Jim_SetResultString(interp, message, -1);
        else
            Jim_SetEmptyResult(interp);
    }
    return JIM_OK;
}

static int
Zeromq_Cmd(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{

	const char *arg_type = "PUSH";
	int type = ZMQ_PUSH;
	const char *subscribe = "";
	const char *endpoint = NULL;
	int bind = 0;

    if (argc < 2 || (argc == 2 && Jim_CompareStringImmediate(interp, argv[1], "-bind"))) {
        goto wrong_args;
    }

    if (Jim_CompareStringImmediate(interp, argv[1], "-bind")) {
        bind = 1;
        arg_type = Jim_String(argv[2]);
    }
    else {
        arg_type = Jim_String(argv[1]);
    }

    if (!strcasecmp(arg_type, "push")) {
        type = ZMQ_PUSH;
    }
    else if (!strcasecmp(arg_type, "pull")) {
        type = ZMQ_PULL;
    }
    else if (!strcasecmp(arg_type, "req")) {
        type = ZMQ_REQ;
    }
    else if (!strcasecmp(arg_type, "rep")) {
        type = ZMQ_REP;
    }
    else if (!strcasecmp(arg_type, "pub")) {
        type = ZMQ_PUB;
    }
    else if (!strcasecmp(arg_type, "sub")) {
        type = ZMQ_SUB;
    }
    else {
        goto wrong_args;
    }

    if (type == ZMQ_SUB && argc == 4) {
        subscribe = Jim_String(argv[argc-2]);
    }

    endpoint = Jim_String(argv[argc-1]);

    if (ctx == NULL)
        ctx = zctx_new();

	if (ctx == NULL) {
	    goto zmq_error;
	}

	void *socket = zsocket_new(ctx, type);
	if (socket == NULL) {
	    goto zmq_error;
    }

	if (type == ZMQ_SUB)
        zsocket_set_subscribe(socket, subscribe);

	if (bind) {
		zsocket_bind(socket, endpoint);
	} else {
	    zsocket_connect(socket, endpoint);
    }

    char buf[60];
    snprintf(buf, sizeof(buf), "zeromq.socket%ld", Jim_GetId(interp));
    Jim_CreateCommand(interp, buf, JimZeromqHandlerCommand, socket, JimZeromqDelSocketProc);

    Jim_SetResult(interp, Jim_MakeGlobalNamespaceName(interp, Jim_NewStringObj(interp, buf, -1)));
    return JIM_OK;

wrong_args:
    Jim_WrongNumArgs(interp, 1, argv, "?-bind? type ?subscribe? endpoint");
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

    Jim_CreateCommand(interp, "zeromq.open", Zeromq_Cmd, NULL, JimZeromqDelContextProc);
    return JIM_OK;
}
