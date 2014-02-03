/*
 * Jim - totp / hotp command for One-Time Password generation
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

/*
 * Copyright (c) 2011 Red Hat, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <sys/types.h>
#include <sys/param.h>
#include <stdint.h>
#include <ctype.h>
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <openssl/evp.h>

#include <jim.h>

#define MAX_HMAC_SIZE	(512 / 8)
#define MAX_BLOCK_SIZE	(512 / 8)

struct thotp_blob {
	unsigned char *data;
	size_t length;
};

enum thotp_alg {
	thotp_alg_sha1,
	thotp_alg_sha224,
	thotp_alg_sha256,
	thotp_alg_sha384,
	thotp_alg_sha512,
};

static int
thotp_digest_base(unsigned char *data1, size_t data1_size, unsigned char pad,
		  unsigned char *data2_value, size_t data2_size,
		  enum thotp_alg algorithm,
		  unsigned char *out, size_t space, size_t *space_used)
{
	size_t hmac_size, block_size;
	unsigned char block[MAX_BLOCK_SIZE];
	EVP_MD_CTX ctx;
	const EVP_MD *digest;
	unsigned int s;

	memset(&ctx, 0, sizeof(ctx));
	switch (algorithm) {
	case thotp_alg_sha1:
		digest = EVP_sha1();
		break;
	case thotp_alg_sha224:
		digest = EVP_sha224();
		break;
	case thotp_alg_sha256:
		digest = EVP_sha256();
		break;
	case thotp_alg_sha384:
		digest = EVP_sha384();
		break;
	case thotp_alg_sha512:
		digest = EVP_sha512();
		break;
	default:
		return ENOSYS;
	};

	hmac_size = EVP_MD_size(digest);
	if (hmac_size > space) {
		return ENOSPC;
	}
	block_size = EVP_MD_block_size(digest);
	if (data1_size > 0) {
		if (data1_size < block_size) {
			memcpy(block, data1, data1_size);
			memset(block + data1_size, 0, block_size - data1_size);
		} else {
			if ((EVP_DigestInit(&ctx, digest) != 1) ||
			    (EVP_DigestUpdate(&ctx, data1, data1_size) != 1) ||
			    (EVP_DigestFinal(&ctx, block, &s) != 1)) {
				return ENOSYS;
			}
			if (s < block_size) {
				memset(block + s, 0, block_size - s);
			}
		}
		for (s = 0; s < block_size; s++) {
			block[s] ^= pad;
		}
	}

	if ((EVP_DigestInit(&ctx, digest) != 1) ||
	    ((block_size > 0) &&
	     (EVP_DigestUpdate(&ctx, block, block_size) != 1)) ||
	    (EVP_DigestUpdate(&ctx, data2_value, data2_size) != 1) ||
	    (EVP_DigestFinal(&ctx, out, &s) != 1)) {
		return ENOSYS;
	}
	*space_used = s;
	return 0;
}

static int
thotp_hmac_base(struct thotp_blob *key,
		unsigned char *counter_value, size_t counter_size,
		enum thotp_alg algorithm,
		unsigned char *out, size_t space, size_t *space_used)
{
	unsigned char buf2[space];
	int result;

	result = thotp_digest_base(key->data, key->length, 0x36,
				   counter_value, counter_size,
				   algorithm, buf2, space, space_used);
	if (result == 0) {
		result = thotp_digest_base(key->data, key->length, 0x5c,
					   buf2, *space_used,
					   algorithm, out, space, space_used);
	}

	return result;
}

int
thotp_hmac(struct thotp_blob *key, uint64_t counter, enum thotp_alg algorithm,
	   unsigned char *out, size_t space, size_t *space_used)
{
	unsigned char counter_value[64 / 8];
	int i;

	for (i = 0; i < 8; i++) {
		counter_value[i] = (counter >> (64 - ((i + 1) * 8))) & 0xff;
	}
	return thotp_hmac_base(key, counter_value, sizeof(counter_value),
			       algorithm, out, space, space_used);
}

static int
thotp_truncate(unsigned char *hmac, size_t hmac_size, int digits, char *data)
{
	unsigned int offset;
	uint32_t bin_code, multiplier;
	int i;

	if (hmac_size < 1) {
		return ENOSYS;
	}

	offset = hmac[hmac_size - 1] & 0x0f;
	bin_code = ((hmac[offset] & 0x7f) << 24) |
		   ((hmac[offset + 1] & 0xff) << 16) |
		   ((hmac[offset + 2] & 0xff) <<  8) |
		   (hmac[offset + 3] & 0xff);

	for (i = 0, multiplier = 1; i < digits; i++) {
		multiplier *= 10;
	}
	snprintf(data, digits + 1, "%0*lu",
		 digits, (long) (bin_code % multiplier));

	return 0;
}

int
thotp_hotp(struct thotp_blob *key,
	   enum thotp_alg algorithm, uint64_t counter, int digits,
	   char **code)
{
	unsigned char hmac[MAX_HMAC_SIZE];
	size_t hmac_size;
	int result;

	*code = NULL;

	result = thotp_hmac(key, counter, algorithm,
			    hmac, sizeof(hmac), &hmac_size);
	if (result != 0) {
		return result;
	}

	*code = Jim_Alloc(digits + 1);
	if (*code == NULL) {
		Jim_Free(*code);
		*code = NULL;
		return ENOMEM;
	}
	memset(*code, '\0', digits + 1);

	result = thotp_truncate(hmac, hmac_size, digits, *code);
	if (result != 0)  {
		Jim_Free(*code);
		*code = NULL;
		return result;
	}

	return 0;
}

int
thotp_totp(struct thotp_blob *key,
	   enum thotp_alg algorithm, uint64_t step, time_t when, int digits,
	   char **code)
{
	return thotp_hotp(key, algorithm, (when / step), digits, code);
}

static const char b32alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";

int
thotp_base32_parse(const char *base32, struct thotp_blob **blob)
{
	unsigned char buf[5];
	unsigned int counter, v;
	const char *p, *q;

	*blob = Jim_Alloc(sizeof(**blob));
	if (*blob == NULL) {
		return ENOMEM;
	}
	(*blob)->data = Jim_Alloc((howmany(strlen(base32), 8) * 5) + 5 + 1);
	if ((*blob)->data == NULL) {
		Jim_Free(*blob);
		*blob = NULL;
		return ENOMEM;
	}
	for (p = base32, counter = 0, (*blob)->length = 0; *p != '\0'; p++) {
		if (*p == '=') {
			break;
		}
		if ((q = strchr(b32alphabet, toupper(*p))) == NULL) {
			continue;
		}
		v = (q - b32alphabet) & 0x1f;
		if ((counter % 8) == 0) {
			memset(buf, '\0', sizeof(buf));
		}
		switch (counter % 8) {
		case 0:
			buf[0] &= 0x07;
			buf[0] |= (v << 3);
			break;
		case 1:
			buf[0] &= 0xf8;
			buf[0] |= (v >> 2);
			buf[1] &= 0x3f;
			buf[1] |= (v << 6);
			break;
		case 2:
			buf[1] &= 0xc0;
			buf[1] |= (v << 1);
			break;
		case 3:
			buf[1] &= 0xfe;
			buf[1] |= (v >> 4);
			buf[2] &= 0x0f;
			buf[2] |= (v << 4);
			break;
		case 4:
			buf[2] &= 0xf0;
			buf[2] |= (v >> 1);
			buf[3] &= 0x7f;
			buf[3] |= (v << 7);
			break;
		case 5:
			buf[3] &= 0x80;
			buf[3] |= (v << 2);
			break;
		case 6:
			buf[3] &= 0xfc;
			buf[3] |= v >> 3;
			buf[4] &= 0x1f;
			buf[4] |= v << 5;
			break;
		case 7:
			buf[4] &= 0xe0;
			buf[4] |= v;
			break;
		}
		counter++;
		if ((counter % 8) == 0) {
			memcpy((*blob)->data + (*blob)->length, buf, 5);
			(*blob)->length += 5;
		}
	}
	if ((counter % 8) != 0) {
		v = ((counter % 8) * 5) / 8;
		memcpy((*blob)->data + (*blob)->length, buf, v);
		(*blob)->length += v;
	}
	(*blob)->data[(*blob)->length] = '\0';
	return 0;
}

int
thotp_base32_unparse(struct thotp_blob *blob, char **base32)
{
	size_t length;
	unsigned int i, j;
	unsigned char v[8];

	*base32 = Jim_Alloc((howmany(blob->length, 5) * 8) + 1);
	if (*base32 == NULL) {
		return ENOMEM;
	}
	for (i = 0, length = 0; i < blob->length; i++) {
		if ((i % 5) == 0) {
			memset(v, 0, sizeof(v));
		}
		switch (i % 5) {
		case 0:
			v[0] |= (blob->data[i]) >> 3;
			v[1] |= (blob->data[i] & 0x07) << 2;
			break;
		case 1:
			v[1] |= (blob->data[i] & 0xc0) >> 6;
			v[2] |= (blob->data[i] & 0x3e) >> 1;
			v[3] |= (blob->data[i] & 0x01) << 4;
			break;
		case 2:
			v[3] |= (blob->data[i] & 0xf0) >> 4;
			v[4] |= (blob->data[i] & 0x0f) << 1;
			break;
		case 3:
			v[4] |= (blob->data[i] & 0x80) >> 7;
			v[5] |= (blob->data[i] & 0x7c) >> 2;
			v[6] |= (blob->data[i] & 0x03) << 3;
			break;
		case 4:
			v[6] |= (blob->data[i] & 0xe0) >> 5;
			v[7] |= (blob->data[i] & 0x1f);
			break;
		}
		if ((i % 5) == 4) {
			for (j = 0; j < 8; j++) {
				(*base32)[length++] = b32alphabet[v[j]];
			}
		}
	}
	if ((i % 5) != 0) {
		for (j = 0; j < 8; j++) {
			if (j < howmany((i % 5) * 8, 5)) {
				(*base32)[length++] = b32alphabet[v[j]];
			} else {
				(*base32)[length++] = '=';
			}
		}
	}
	(*base32)[length] = '\0';

	return 0;
}

static int
Hotp_Cmd(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    struct thotp_blob *key = NULL;
	char *code = NULL, *p;
	unsigned long long counter;

    int rc = JIM_OK;

    if (argc != 3) {
        Jim_WrongNumArgs(interp, 1, argv, "key counter");
        rc = JIM_ERR;
        goto free;
    }

	if (thotp_base32_parse(Jim_String(argv[1]), &key) != 0) {
        Jim_SetResultString(interp, "Failed to parse key", -1);
        rc = JIM_ERR;
        goto free;
    }

    counter = strtoull(Jim_String(argv[2]), &p, 10);
	if ((p == NULL) || (*p != '\0')) {
        Jim_SetResultString(interp, "Failed to parse counter", -1);
        rc = JIM_ERR;
        goto free;
    }

    if (thotp_hotp(key, thotp_alg_sha1, (uint64_t) counter, 6, &code) != 0) {
        Jim_SetResultString(interp, "Failed to calculate HOTP", -1);
        return JIM_ERR;
    }

    Jim_SetResultString(interp, code, -1);

free:

    if (key && key->data) {
        Jim_Free(key->data);
        Jim_Free(key);
    }

    if (code)
        Jim_Free(code);

    return rc;
}

static int
Totp_Cmd(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
	struct thotp_blob *key = NULL;
	char *code = NULL;

    int rc = JIM_OK;

    if (argc != 2) {
        Jim_WrongNumArgs(interp, 1, argv, "key");
        rc = JIM_ERR;
        goto free;
    }

	if (thotp_base32_parse(Jim_String(argv[1]), &key) != 0) {
        Jim_SetResultString(interp, "Failed to parse key", -1);
        rc = JIM_ERR;
        goto free;
    }

	if (thotp_totp(key, thotp_alg_sha1, 30, time(NULL), 6, &code) != 0) {
        Jim_SetResultString(interp, "Failed to calculate TOTP", -1);
        rc = JIM_ERR;
        goto free;
    }

    Jim_SetResultString(interp, code, -1);

free:

    if (key && key->data) {
        Jim_Free(key->data);
        Jim_Free(key);
    }

    if (code)
        Jim_Free(code);

    return rc;
}

int
Jim_thotpInit(Jim_Interp *interp)
{
    if (Jim_PackageProvide(interp, "thotp", "0.1", JIM_ERRMSG))
        return JIM_ERR;

    Jim_CreateCommand(interp, "hotp", Hotp_Cmd, NULL, NULL);
    Jim_CreateCommand(interp, "totp", Totp_Cmd, NULL, NULL);

    return JIM_OK;
}
