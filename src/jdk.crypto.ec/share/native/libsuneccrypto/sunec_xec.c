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
#include <openssl/evp.h>

#include "sunec_util.h"

JNIEXPORT void JNICALL Java_sun_security_ec_NativeEC_xdhComputePubKey
  (JNIEnv *env, jclass clazz, jint curveNID,
   jbyteArray privKeyIn, jbyteArray pubKeyOut) {
    EVP_PKEY *pkey = NULL;
    jbyte* priv_key_bytes = NULL;
    jbyte* pub_key_bytes = NULL;

    int evp_type = 0;
    int key_len = 0;

    switch (curveNID) {
        case NID_X25519:
            evp_type = EVP_PKEY_X25519;
            key_len = 32;
            break;
        case NID_X448:
            evp_type = EVP_PKEY_X448;
            key_len = 56;
            break;
        default:
            sunec_throw(env, INVALID_ALGO_PARAM_EXCEPTION, "Unsupported XDH curve");
            return;
    }

    jsize priv_key_len = (*env)->GetArrayLength(env, privKeyIn);
    jsize pub_key_len = (*env)->GetArrayLength(env, pubKeyOut);
    if (priv_key_len != key_len || pub_key_len != key_len) {
        sunec_throw(env, INVALID_KEY_EXCEPTION,
                evp_type == EVP_PKEY_X25519 ? "X25519 requires 32-byte buffer"
                                            : "X448 requires 56-byte buffer");
        goto cleanup;
    }

    priv_key_bytes = (*env)->GetPrimitiveArrayCritical(env, privKeyIn, NULL);
    if (!priv_key_bytes) {
        sunec_throw(env, ILLEGAL_STATE_EXCEPTION, "Access private key buffer failed");
        goto cleanup;
    }

    pkey = EVP_PKEY_new_raw_private_key(evp_type, NULL, (const unsigned char*)priv_key_bytes, priv_key_len);
    if (!pkey) {
        sunec_throw(env, INVALID_KEY_EXCEPTION, "Invalid XDH private key");
        goto cleanup;
    }
    (*env)->ReleasePrimitiveArrayCritical(env, privKeyIn, priv_key_bytes, JNI_ABORT);
    priv_key_bytes = NULL;

    pub_key_bytes = (*env)->GetPrimitiveArrayCritical(env, pubKeyOut, NULL);
    if (!pub_key_bytes) {
        sunec_throw(env, ILLEGAL_STATE_EXCEPTION, "Access public key buffer failed");
        goto cleanup;
    }

    size_t real_pub_key_len = (size_t)key_len;
    if (!EVP_PKEY_get_raw_public_key(pkey, (unsigned char*)pub_key_bytes, &real_pub_key_len)
        || real_pub_key_len != (size_t)pub_key_len) {
        sunec_throw(env, INVALID_KEY_EXCEPTION, "Public key derivation failed");
        goto cleanup;
    }
    (*env)->ReleasePrimitiveArrayCritical(env, pubKeyOut, pub_key_bytes, 0);
    pub_key_bytes = NULL;

cleanup:
    if (priv_key_bytes) {
        (*env)->ReleasePrimitiveArrayCritical(env, privKeyIn, priv_key_bytes, JNI_ABORT);
    }
    if (pub_key_bytes) {
        (*env)->ReleasePrimitiveArrayCritical(env, pubKeyOut, pub_key_bytes, 0);
    }
    if (pkey) {
        EVP_PKEY_free(pkey);
    }
}
