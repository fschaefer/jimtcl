/*
 * Jim - aes command
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
#include <stdio.h>
#include <stdlib.h>
#include <openssl/aes.h>
#include <openssl/evp.h>

#include <jim-subcmd.h>

extern char* hex_encode(const char *bin, int len);
extern void* hex_decode(const char *bin);

typedef enum aes_mode_t {
    AES_MODE_ENCRYPTION,
    AES_MODE_DECRYPTION,
    AES_MODE_UNKNOWN
} aes_mode_t;

typedef enum aes_algo_t {
    AES_128_ECB,
    AES_192_ECB,
    AES_256_ECB,
    AES_128_CBC,
    AES_192_CBC,
    AES_256_CBC,
    AES_NONE
} aes_algo_t;

int
aes_init(const char *key_data, int key_data_len, unsigned char *salt, \
    EVP_CIPHER_CTX *ctx, aes_mode_t mode, aes_algo_t algo)
{
    int rounds, ret, def_key_len;
    unsigned char key[EVP_MAX_KEY_LENGTH], iv[EVP_MAX_IV_LENGTH];
    const EVP_CIPHER *type;

    switch (algo) {
    case AES_128_ECB:
        rounds = 1;
        def_key_len = 16;
        type = EVP_aes_128_ecb();
        break;
    case AES_192_ECB:
        rounds = 1;
        def_key_len = 24;
        type = EVP_aes_192_ecb();
        break;
    case AES_256_ECB:
        rounds = 1;
        def_key_len = 32;
        type = EVP_aes_256_ecb();
    case AES_128_CBC:
        rounds = 1;
        def_key_len = 16;
        type = EVP_aes_128_cbc();
        break;
    case AES_192_CBC:
        rounds = 1;
        def_key_len = 24;
        type = EVP_aes_192_cbc();
        break;
    case AES_256_CBC:
        rounds = 1;
        def_key_len = 32;
        type = EVP_aes_256_cbc();
        break;
    default:
        return -1;
    }

    ret = EVP_BytesToKey(type, EVP_md5(), salt, (unsigned char*)key_data, key_data_len, rounds, key, iv);
    if (ret != def_key_len) {
        return -1;
    }

    EVP_CIPHER_CTX_init(ctx);
    switch (mode) {
    case AES_MODE_ENCRYPTION:
        EVP_EncryptInit_ex(ctx, type, NULL, key, iv);
        break;
    case AES_MODE_DECRYPTION:
        EVP_DecryptInit_ex(ctx, type, NULL, key, iv);
        break;
    default:
        break;
    }

    return 0;
}

int
aes_encrypt(EVP_CIPHER_CTX *e, unsigned char *in, int ilen, unsigned char *out, int *olenp)
{
    int olen = *olenp;
    int flen = 0;

    EVP_EncryptUpdate(e, out, &olen, in, ilen);
    EVP_EncryptFinal_ex(e, out + olen, &flen);

    *olenp = olen + flen;
    return 0;
}

int
aes_decrypt(EVP_CIPHER_CTX *e, unsigned char *in, int ilen, unsigned char *out, int *olenp)
{
    int olen = *olenp;
    int flen = 0;

    EVP_DecryptUpdate(e, out, &olen, in, ilen);
    EVP_DecryptFinal_ex(e, out + olen, &flen);

    *olenp = olen + flen;
    return 0;
}

static int
aes_cmd_encrypt(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    if (argc < 2 || argc == 3 || (argc == 4 && !Jim_CompareStringImmediate(interp, argv[0], "-type"))) {
        goto wrong_args;
    }

    aes_algo_t type = AES_256_CBC;

    if (argc == 4) {
        if (Jim_CompareStringImmediate(interp, argv[1], "AES_128_ECB")) {
            type = AES_128_ECB;
        }
        else if (Jim_CompareStringImmediate(interp, argv[1], "AES_192_ECB")) {
            type = AES_192_ECB;
        }
        else if (Jim_CompareStringImmediate(interp, argv[1], "AES_256_ECB")) {
            type = AES_256_ECB;
        }
        else if (Jim_CompareStringImmediate(interp, argv[1], "AES_128_CBC")) {
            type = AES_128_CBC;
        }
        else if (Jim_CompareStringImmediate(interp, argv[1], "AES_192_CBC")) {
            type = AES_192_CBC;
        }
        else if (Jim_CompareStringImmediate(interp, argv[1], "AES_256_CBC")) {
            type = AES_256_CBC;
        }
        else {
            goto wrong_args;
        }
    }

    const char *key = Jim_String(argv[argc-2]);

    char *plaintext = (char*)Jim_String(argv[argc-1]);
    int plaintext_len = strlen(plaintext);

    char *ciphertext = (char *) Jim_Alloc(plaintext_len + AES_BLOCK_SIZE -1);
    int olen = 0;

    int rc = JIM_OK;

    EVP_CIPHER_CTX ctx;

    if (aes_init(key, strlen(key), NULL, &ctx, AES_MODE_ENCRYPTION, type) != 0) {
        Jim_SetResultString(interp, "Failed to encrypt data", -1);
        rc = JIM_ERR;
        goto encrypt_error;
    }

    aes_encrypt(&ctx, (unsigned char*)plaintext, plaintext_len, (unsigned char*)ciphertext, &olen);

    EVP_CIPHER_CTX_cleanup(&ctx);

    if (olen == 0) {
        Jim_SetResultString(interp, "Failed to encrypt data", -1);
        rc = JIM_ERR;
        goto encrypt_error;
    }

    char *hex = hex_encode(ciphertext, olen);
    Jim_SetResultString(interp, hex, -1);
    Jim_Free(hex);

encrypt_error:

    Jim_Free(ciphertext);
    return rc;

wrong_args:

    Jim_WrongNumArgs(interp, 0, argv, "?-type AES_128_ECB|AES_192_ECB|AES_256_ECB|AES_128_CBC|AES_192_CBC|AES_256_CBC? key plaintext");
    return JIM_ERR;
}

static int
aes_cmd_decrypt(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    if (argc < 2 || argc == 3 || (argc == 4 && !Jim_CompareStringImmediate(interp, argv[0], "-type"))) {
        goto wrong_args;
    }

    aes_algo_t type = AES_256_CBC;

    if (argc == 4) {
        if (Jim_CompareStringImmediate(interp, argv[1], "AES_128_ECB")) {
            type = AES_128_ECB;
        }
        else if (Jim_CompareStringImmediate(interp, argv[1], "AES_192_ECB")) {
            type = AES_192_ECB;
        }
        else if (Jim_CompareStringImmediate(interp, argv[1], "AES_256_ECB")) {
            type = AES_256_ECB;
        }
        else if (Jim_CompareStringImmediate(interp, argv[1], "AES_128_CBC")) {
            type = AES_128_CBC;
        }
        else if (Jim_CompareStringImmediate(interp, argv[1], "AES_192_CBC")) {
            type = AES_192_CBC;
        }
        else if (Jim_CompareStringImmediate(interp, argv[1], "AES_256_CBC")) {
            type = AES_256_CBC;
        }
        else {
            goto wrong_args;
        }
    }
    const char *key = Jim_String(argv[argc-2]);

    const char *hex = Jim_String(argv[argc-1]);
    const char *ciphertext = hex_decode(hex);

    int ciphertext_len = strlen(hex) / 2;

    int olen = 0;
    unsigned char *plaintext = Jim_Alloc(ciphertext_len + AES_BLOCK_SIZE);

    int rc = JIM_OK;

    EVP_CIPHER_CTX ctx;

    if (aes_init(key, strlen(key), NULL, &ctx, AES_MODE_DECRYPTION, type) != 0) {
        Jim_SetResultString(interp, "Failed to decrypt data", -1);
        rc = JIM_ERR;
        goto decrypt_error;
    }

    aes_decrypt(&ctx, (unsigned char*)ciphertext, ciphertext_len, plaintext, &olen);

    EVP_CIPHER_CTX_cleanup(&ctx);

    if (olen == 0) {
        Jim_SetResultString(interp, "Failed to decrypt data", -1);
        rc = JIM_ERR;
        goto decrypt_error;
    }

    plaintext[olen] = '\0';

    Jim_SetResultString(interp, (const char*)plaintext, -1);

decrypt_error:

    Jim_Free((void*)ciphertext);
    Jim_Free((void*)plaintext);

    return rc;

wrong_args:

    Jim_WrongNumArgs(interp, 0, argv, "?-type AES_128_ECB|AES_192_ECB|AES_256_ECB|AES_128_CBC|AES_192_CBC|AES_256_CBC? key ciphertext");
    return JIM_ERR;
}

static const jim_subcmd_type array_command_table[] = {
    {
        "encrypt",
        "?-type AES_128_ECB|AES_192_ECB|AES_256_ECB|AES_128_CBC|AES_192_CBC|AES_256_CBC? key plaintext",
        aes_cmd_encrypt,
        2,
        4,
        /* Description: aes encrypt */
    },
    {
        "decrypt",
        "?-type AES_128_ECB|AES_192_ECB|AES_256_ECB|AES_128_CBC|AES_192_CBC|AES_256_CBC? key ciphertext",
        aes_cmd_decrypt,
        2,
        4,
        /* Description: aes decrypt */
    },
    {
        NULL
    }
};

int
Jim_aesInit(Jim_Interp *interp)
{
    if (Jim_PackageProvide(interp, "aes", "0.1", JIM_ERRMSG))
        return JIM_ERR;

    Jim_CreateCommand(interp, "aes", Jim_SubCmdProc, (void *)array_command_table, NULL);
    return JIM_OK;
}

