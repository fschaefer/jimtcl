/*
 * Jim - hex command
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

char *
hex_encode(const char *bin, int len)
{
    int i, idx;
    char hex_char[] = "0123456789ABCDEF";
    char *hex = Jim_Alloc(len * 2 + 1);

    for (i=0, idx=0; i<len; i++)
    {
        hex[idx++] = hex_char[(bin[i] & 0xf0) >> 4];
        hex[idx++] = hex_char[bin[i] & 0x0f];
    }

    hex[idx] = '\0';
    return hex;
}

void *
hex_decode(const char *hex)
{
    int i, idx;
    char bytes[2];
    int len = strlen(hex) / 2;
    char *bin = Jim_Alloc(len + 1);

    for (i=0, idx=0; hex[i]; i++)
    {
        if (hex[i & 0x01] == 0)
            return NULL; // hex length != even

        if (hex[i] >= '0' && hex[i] <= '9')
            bytes[i & 0x01] = hex[i] - '0';
        else if (hex[i] >= 'A' && hex[i] <= 'F')
            bytes[i & 0x01] = hex[i] - 'A' + 10;
        else if (hex[i] >= 'a' && hex[i] <= 'f')
            bytes[i & 0x01] = hex[i] - 'a' + 10;

        if (i & 0x01) {
            bin[idx++] = (bytes[0] << 4) | bytes[1];
        }
    }

    bin[idx++] = '\0';

    return bin;
}

static int
hex_cmd_encode(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    const char *data = Jim_String(argv[0]);
    int len = Jim_Length(argv[0]);

    char *b64 = hex_encode(data, len);

    if (b64 == NULL) {
        Jim_SetResultString(interp, "Failed to encode data", -1);
        return JIM_ERR;
    }

    Jim_SetResultString(interp, b64, -1);
    Jim_Free(b64);

    return JIM_OK;
}

static int
hex_cmd_decode(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    void *data = hex_decode(Jim_String(argv[0]));

    if (data == NULL) {
        Jim_SetResultString(interp, "Failed to decode data", -1);
        return JIM_ERR;
    }

    Jim_SetResultString(interp, data, -1);
    Jim_Free(data);

    return JIM_OK;
}

static const jim_subcmd_type array_command_table[] = {
    {
        "encode",
        "data",
        hex_cmd_encode,
        1,
        1,
        /* Description: hex encode */
    },
    {
        "decode",
        "hexString",
        hex_cmd_decode,
        1,
        1,
        /* Description: hex decode */
    },
    {
        NULL
    }
};

int
Jim_hexInit(Jim_Interp *interp)
{
    if (Jim_PackageProvide(interp, "hex", "0.1", JIM_ERRMSG))
        return JIM_ERR;

    Jim_CreateCommand(interp, "hex", Jim_SubCmdProc, (void *)array_command_table, NULL);
    return JIM_OK;
}
