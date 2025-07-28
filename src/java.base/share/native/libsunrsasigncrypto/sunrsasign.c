/*
 * Copyright (C) 2025, THL A29 Limited, a Tencent company. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.
 *
 * This code is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * version 2 for more details (a copy is included in the LICENSE file that
 * accompanied this code).
 *
 * You should have received a copy of the GNU General Public License version
 * 2 along with this work; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "jni.h"

#include <openssl/err.h>
#include <openssl/rand.h>

#include "sunrsasign_util.h"

JNIEXPORT void JNICALL Java_sun_security_rsa_NativeSunRsaSign_rsaModPow
  (JNIEnv *env, jclass clazz, jbyteArray base, jbyteArray exp, jbyteArray mod,
   jbyteArray out) {
    jbyte *base_bytes = NULL, *exp_bytes = NULL, *mod_bytes = NULL, *out_bytes = NULL;
    BIGNUM *bn_base = NULL, *bn_exp = NULL, *bn_mod = NULL, *bn_out = NULL;
    BN_CTX *ctx = NULL;

    const jsize base_len = (*env)->GetArrayLength(env, base);
    const jsize exp_len = (*env)->GetArrayLength(env, exp);
    const jsize mod_len = (*env)->GetArrayLength(env, mod);
    if (base_len <= 0 || exp_len <= 0 || mod_len <= 0) {
        sunrsasign_throw(env, INVALID_ALGO_PARAM_EXCEPTION, "The parameter cannot be empty");
        goto cleanup;
    }

    const jsize out_len = (*env)->GetArrayLength(env, out);
    if (out_len <= 0) {
        sunrsasign_throw(env, INVALID_ALGO_PARAM_EXCEPTION, "The out buffer cannot be empty");
        goto cleanup;
    }

    if (!(base_bytes = (*env)->GetByteArrayElements(env, base, NULL))) {
        sunrsasign_throw(env, ILLEGAL_STATE_EXCEPTION, "Failed to access base");
        goto cleanup;
    }
    if (!(exp_bytes = (*env)->GetByteArrayElements(env, exp, NULL))) {
        sunrsasign_throw(env, ILLEGAL_STATE_EXCEPTION, "Failed to access exponent");
        goto cleanup;
    }
    if (!(mod_bytes = (*env)->GetByteArrayElements(env, mod, NULL))) {
        sunrsasign_throw(env, ILLEGAL_STATE_EXCEPTION, "Failed to access base");
        goto cleanup;
    }
    if (!(out_bytes = (*env)->GetByteArrayElements(env, out, NULL))) {
        sunrsasign_throw(env, ILLEGAL_STATE_EXCEPTION, "Failed to access out buffer");
        goto cleanup;
    }

    if (!(bn_base = BN_bin2bn((const unsigned char *)base_bytes, base_len, NULL))) {
        sunrsasign_throw(env, ILLEGAL_STATE_EXCEPTION, "Failed to create BIGNUM for base");
        goto cleanup;
    }
    if (!(bn_exp = BN_bin2bn((const unsigned char *)exp_bytes, exp_len, NULL))) {
        sunrsasign_throw(env, ILLEGAL_STATE_EXCEPTION, "Failed to create BIGNUM for exponent");
        goto cleanup;
    }
    if (!(bn_mod = BN_bin2bn((const unsigned char *)mod_bytes, mod_len, NULL))) {
        sunrsasign_throw(env, ILLEGAL_STATE_EXCEPTION, "Failed to create BIGNUM for modulus");
        goto cleanup;
    }

    if (!(ctx = BN_CTX_new())) {
        sunrsasign_throw(env, ILLEGAL_STATE_EXCEPTION, "Failed to create BN_CTX");
        goto cleanup;
    }

    if (!(bn_out = BN_new())) {
        sunrsasign_throw(env, ILLEGAL_STATE_EXCEPTION, "Failed to create BIGNUM for out");
        goto cleanup;
    }

    if (!BN_mod_exp(bn_out, bn_base, bn_exp, bn_mod, ctx)) {
        sunrsasign_throw(env, ILLEGAL_STATE_EXCEPTION, "Failed to compute the result");
        goto cleanup;
    }

    int result_len = BN_num_bytes(bn_out);
    if (result_len < 0) {
        sunrsasign_throw(env, ILLEGAL_STATE_EXCEPTION, "Invalid length of the result");
        goto cleanup;
    }

    const int pad_len = out_len - result_len;
    if (pad_len < 0) {
        sunrsasign_throw(env, ILLEGAL_STATE_EXCEPTION, "Out buffer is too small");
        goto cleanup;
    }

    BN_bn2bin(bn_out, (unsigned char *)out_bytes + pad_len);

cleanup:
    if (base_bytes) {
        (*env)->ReleaseByteArrayElements(env, base, base_bytes, JNI_ABORT);
    }
    if (exp_bytes) {
        (*env)->ReleaseByteArrayElements(env, exp, exp_bytes, JNI_ABORT);
    }
    if (mod_bytes) {
        (*env)->ReleaseByteArrayElements(env, mod, mod_bytes, JNI_ABORT);
    }
    if (out_bytes) {
        (*env)->ReleaseByteArrayElements(env, out, out_bytes, 0);
    }

    if (bn_base) {
        BN_clear_free(bn_base);
    }
    if (bn_exp) {
        BN_clear_free(bn_exp);
    }
    if (bn_mod) {
        BN_clear_free(bn_mod);
    }
    if (bn_out) {
        BN_clear_free(bn_out);
    }
    if (ctx) {
        BN_CTX_free(ctx);
    }
}
