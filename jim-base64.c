/*
 * Jim - base64 command
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

#include <stdlib.h>
#include <string.h>
#include <jim-subcmd.h>

static char base64_alphabet[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz"
    "0123456789+/";

#define XX -1

static int base64_index[256] = {
    XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX,
    XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX,
    XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,62, XX,XX,XX,63,
    52,53,54,55, 56,57,58,59, 60,61,XX,XX, XX,XX,XX,XX,
    XX, 0, 1, 2,  3, 4, 5, 6,  7, 8, 9,10, 11,12,13,14,
    15,16,17,18, 19,20,21,22, 23,24,25,XX, XX,XX,XX,XX,
    XX,26,27,28, 29,30,31,32, 33,34,35,36, 37,38,39,40,
    41,42,43,44, 45,46,47,48, 49,50,51,XX, XX,XX,XX,XX,
    XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX,
    XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX,
    XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX,
    XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX,
    XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX,
    XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX,
    XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX,
    XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX,
};

char*
base64_encode(const void *data, size_t size)
{
    char *s;
    char *p;
    int c;
    unsigned const char *q;

    if (data == NULL) return NULL;
    if (size == 0) return NULL;

    p = s = calloc(size * 4 / 3 + 4, sizeof(char));

    if (p == NULL)
        return NULL;

    q = (const unsigned char *) data;

    int i;
    for (i = 0; i < size;) {
        c = q[i++];
        c *= 256;
        if (i < size)
            c += q[i];

        i++;

        c *= 256;
        if (i < size)
            c += q[i];

        i++;

        p[0] = base64_alphabet[(c & 0x00fc0000) >> 18];
        p[1] = base64_alphabet[(c & 0x0003f000) >> 12];
        p[2] = base64_alphabet[(c & 0x00000fc0) >> 6];
        p[3] = base64_alphabet[(c & 0x0000003f) >> 0];

        if (i > size)
            p[3] = '=';

        if (i > size + 1)
            p[2] = '=';

        p += 4;
    }

    *p = 0;

    return s;
}

#define DECODE_ERROR 0xffffffff

static unsigned int
base64_decode_token(const char *token)
{

    int val = 0;
    int marker = 0;

    if (strlen(token) < 4)
        return DECODE_ERROR;

    int i;
    for (i = 0; i < 4; i++) {
        val *= 64;
        if (token[i] == '=') {
            marker++;
        } else if (marker > 0) {
            return DECODE_ERROR;
        } else {
            val += base64_index[(int) token[i]];
        }
    }

    if (marker > 2)
        return DECODE_ERROR;

    return (marker << 24) | val;
}

void*
base64_decode(const char *str)
{
    void *data;
    const char *p;
    unsigned char *q;

    if (str == NULL) return NULL;

    q = data = calloc(strlen(str) / 4 * 3, sizeof(char));

    for (p = str; *p && (*p == '=' || strchr(base64_alphabet, *p)); p += 4) {
        unsigned int val = base64_decode_token(p);
        unsigned int marker = (val >> 24) & 0xff;

        if (val == DECODE_ERROR)
            return NULL;

        *q++ = (val >> 16) & 0xff;

        if (marker < 2)
            *q++ = (val >> 8) & 0xff;

        if (marker < 1)
            *q++ = val & 0xff;
    }

    return data;
}

static int base64_cmd_encode(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    const char *data = Jim_String(argv[0]);
    int len = Jim_Length(argv[0]);

    char *b64 = base64_encode(data, len);

    if (b64 == NULL) {
        Jim_SetResultString(interp, "Failed to encode data", -1);
        return JIM_ERR;
    }

    Jim_SetResultString(interp, b64, -1);
    return JIM_OK;
}

static int base64_cmd_decode(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    void *data = base64_decode(Jim_String(argv[0]));

    if (data == NULL) {
        Jim_SetResultString(interp, "Failed to decode data", -1);
        return JIM_ERR;
    }

    Jim_SetResultString(interp, data, -1);
    return JIM_OK;
}

static const jim_subcmd_type array_command_table[] = {
        {       "encode",
                "data",
                base64_cmd_encode,
                1,
                1,
                /* Description: base64 encode */
        },
        {       "decode",
                "base64String",
                base64_cmd_decode,
                1,
                1,
                /* Description: base64 decode */
        },
        {       NULL
        }
};

int
Jim_base64Init(Jim_Interp *interp)
{
    if (Jim_PackageProvide(interp, "base64", "0.1", JIM_ERRMSG))
        return JIM_ERR;

    Jim_CreateCommand(interp, "base64", Jim_SubCmdProc, (void *)array_command_table, NULL);
    return JIM_OK;
}
