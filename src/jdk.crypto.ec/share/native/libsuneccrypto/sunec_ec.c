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

// Need to use the deprecated lower EC functions
#define OPENSSL_SUPPRESS_DEPRECATED

#include "jni.h"

#include <openssl/ec.h>
#include <openssl/err.h>
#include <openssl/rand.h>

#include "sunec_util.h"

JNIEXPORT void JNICALL Java_sun_security_ec_NativeEC_ecGenKeyPair
  (JNIEnv *env, jclass clazz, jint curveNID, jbyteArray seed,
   jbyteArray privKeyOut, jbyteArray pubKeyOut) {
    EC_KEY *ec_key = EC_KEY_new_by_curve_name(curveNID);
    if (!ec_key) {
        sunec_throw(env, INVALID_ALGO_PARAM_EXCEPTION, "Unsupported EC curve");
        return;
    }

    if (seed != NULL) {
        jsize seed_len = (*env)->GetArrayLength(env, seed);
        jbyte *seed_bytes = (*env)->GetPrimitiveArrayCritical(env, seed, NULL);
        if (!seed_bytes) {
            EC_KEY_free(ec_key);
            sunec_throw(env, ILLEGAL_STATE_EXCEPTION, "Failed to access seed array");
            return;
        }
        RAND_seed(seed_bytes, seed_len);
        (*env)->ReleasePrimitiveArrayCritical(env, seed, seed_bytes, JNI_ABORT);
    }

    if (!EC_KEY_generate_key(ec_key)) {
        EC_KEY_free(ec_key);
        sunec_throw(env, INVALID_KEY_EXCEPTION, "Key pair generation failed");
        return;
    }

    const EC_GROUP *group = EC_KEY_get0_group(ec_key);
    const EC_POINT *pub_point = EC_KEY_get0_public_key(ec_key);
    const BIGNUM *priv_bn = EC_KEY_get0_private_key(ec_key);
    if (!group || !pub_point || !priv_bn) {
        EC_KEY_free(ec_key);
        sunec_throw(env, INVALID_KEY_EXCEPTION, "Invalid key components");
        return;
    }

    jsize priv_key_len = (*env)->GetArrayLength(env, privKeyOut);
    jbyte *priv_key_bytes = (*env)->GetPrimitiveArrayCritical(env, privKeyOut, NULL);
    if (!priv_key_bytes) {
        EC_KEY_free(ec_key);
        sunec_throw(env, ILLEGAL_STATE_EXCEPTION, "Failed to access private key buffer");
        return;
    }

    int encode_priv_key_ok = BN_bn2binpad(
            priv_bn, (uint8_t*)priv_key_bytes, priv_key_len) == priv_key_len;
    (*env)->ReleasePrimitiveArrayCritical(
            env, privKeyOut, priv_key_bytes, encode_priv_key_ok ? 0 : JNI_ABORT);
    if (!encode_priv_key_ok) {
        memset(priv_key_bytes, 0, priv_key_len);
        EC_KEY_free(ec_key);
        sunec_throw(env, ILLEGAL_STATE_EXCEPTION, "Private key encoding failed");
        return;
    }

    jsize pub_key_len = (*env)->GetArrayLength(env, pubKeyOut);
    jbyte* pub_key_bytes = (*env)->GetPrimitiveArrayCritical(env, pubKeyOut, NULL);
    if (!pub_key_bytes) {
        EC_KEY_free(ec_key);
        sunec_throw(env, ILLEGAL_STATE_EXCEPTION, "Failed to access public key buffer");
        return;
    }

    int encode_pub_key_ok = EC_POINT_point2oct(
            group, pub_point, POINT_CONVERSION_UNCOMPRESSED,
            (uint8_t*)pub_key_bytes, pub_key_len, NULL) == (size_t)pub_key_len;
    (*env)->ReleasePrimitiveArrayCritical(
            env, pubKeyOut, pub_key_bytes, encode_pub_key_ok ? 0 : JNI_ABORT);

    EC_KEY_free(ec_key);

    if (!encode_pub_key_ok) {
        sunec_throw(env, ILLEGAL_STATE_EXCEPTION, "Public key encoding failed");
    }
}
