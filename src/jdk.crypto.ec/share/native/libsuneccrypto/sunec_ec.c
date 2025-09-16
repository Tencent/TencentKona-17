/*
 * Copyright (C) 2025, Tencent. All rights reserved.
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

// Validate the given EC point
int validate_point(const EC_GROUP *group, const EC_POINT *point) {
    if (group == NULL || point == NULL) {
        return -1;
    }

    BN_CTX *bn_ctx = BN_CTX_new();
    if (bn_ctx == NULL) {
        return -1;
    }

    BIGNUM *order = BN_new();
    EC_POINT *product = EC_POINT_new(group);
    if (order == NULL || product == NULL) {
        EC_POINT_free(product);
        BN_clear_free(order);
        BN_CTX_free(bn_ctx);

        return -1;
    }

    int validated = 0;

    // The point must be on the curve, and not the infinity.
    // The order of the point must be checked.
    if (EC_GROUP_get_order(group, order, bn_ctx)
        && EC_POINT_is_on_curve(group, point, bn_ctx)
        && !EC_POINT_is_at_infinity(group, point)
        && EC_POINT_mul(group, product, NULL, point, order, bn_ctx)
        && EC_POINT_is_at_infinity(group, product)) {
        validated = 1;
    }

    EC_POINT_free(product);
    BN_clear_free(order);
    BN_CTX_free(bn_ctx);

    return validated;
}

JNIEXPORT void JNICALL Java_sun_security_ec_NativeSunEC_ecGenKeyPair
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

JNIEXPORT void JNICALL Java_sun_security_ec_NativeSunEC_ecdsaSignDigest
  (JNIEnv *env, jclass clazz, jint curveNID, jbyteArray seed,
   jbyteArray privKey, jbyteArray digest, jbyteArray signatureOut) {
    EC_KEY *ec_key = NULL;
    EC_GROUP *group = NULL;
    BIGNUM *priv_key_bn = NULL;
    ECDSA_SIG *ecdsa_sig = NULL;
    unsigned char *seed_data = NULL;
    unsigned char *priv_key_data = NULL;
    unsigned char *digest_data = NULL;
    unsigned char *sig_out_buf = NULL;

    if (seed != NULL) {
        jsize seed_len = (*env)->GetArrayLength(env, seed);
        seed_data = (unsigned char *) (*env)->GetByteArrayElements(env, seed, NULL);
        if (!seed_data || seed_len < 32) {
            sunec_throw(env, INVALID_ALGO_PARAM_EXCEPTION, "Seed must be at least 32 bytes");
            goto cleanup;
        }
        RAND_seed(seed_data, seed_len);
    }

    group = EC_GROUP_new_by_curve_name(curveNID);
    if (!group) {
        sunec_throw(env, INVALID_ALGO_PARAM_EXCEPTION, "Unsupported curve NID");
        goto cleanup;
    }
    const int order_bits = EC_GROUP_order_bits(group);
    const int key_size = (order_bits + 7) / 8;

    jsize priv_key_len = (*env)->GetArrayLength(env, privKey);
    priv_key_data = (unsigned char *)(*env)->GetByteArrayElements(env, privKey, NULL);
    priv_key_bn = BN_bin2bn(priv_key_data, priv_key_len, NULL);
    if (priv_key_len != key_size || !priv_key_bn || BN_is_zero(priv_key_bn)) {
        sunec_throw(env, INVALID_KEY_EXCEPTION, "Invalid private key");
        goto cleanup;
    }

    jsize sig_out_len = (*env)->GetArrayLength(env, signatureOut);
    if (sig_out_len != key_size * 2) {
        sunec_throw(env, ILLEGAL_STATE_EXCEPTION, "Signature buffer size mismatch");
        goto cleanup;
    }

    ec_key = EC_KEY_new();
    if (!ec_key || !EC_KEY_set_group(ec_key, group) || !EC_KEY_set_private_key(ec_key, priv_key_bn)) {
        sunec_throw(env, INVALID_KEY_EXCEPTION, "EC key initialization failed");
        goto cleanup;
    }

    jsize digest_len = (*env)->GetArrayLength(env, digest);
    digest_data = (unsigned char *)(*env)->GetByteArrayElements(env, digest, NULL);
    if (digest_len != key_size) {
        sunec_throw(env, SIGNATURE_EXCEPTION, "Digest length doesn't match curve size");
        goto cleanup;
    }

    ecdsa_sig = ECDSA_do_sign(digest_data, digest_len, ec_key);
    if (!ecdsa_sig) {
        sunec_throw(env, SIGNATURE_EXCEPTION, "Signature generation failed");
        goto cleanup;
    }

    sig_out_buf = (unsigned char *)(*env)->GetByteArrayElements(env, signatureOut, NULL);

    const BIGNUM *r, *s;
    ECDSA_SIG_get0(ecdsa_sig, &r, &s);

    BN_bn2binpad(r, sig_out_buf, key_size);
    BN_bn2binpad(s, sig_out_buf + key_size, key_size);

cleanup:
    if (sig_out_buf) {
        (*env)->ReleaseByteArrayElements(env, signatureOut, (jbyte *)sig_out_buf, 0);
    }
    if (ec_key) {
        EC_KEY_free(ec_key);
    }
    if (group) {
        EC_GROUP_free(group);
    }
    if (priv_key_bn) {
        BN_clear_free(priv_key_bn);
    }
    if (ecdsa_sig) {
        ECDSA_SIG_free(ecdsa_sig);
    }
    if (seed_data) {
        (*env)->ReleaseByteArrayElements(env, seed, (jbyte *)seed_data, JNI_ABORT);
    }
    if (priv_key_data) {
        (*env)->ReleaseByteArrayElements(env, privKey, (jbyte *)priv_key_data, JNI_ABORT);
    }
    if (digest_data) {
        (*env)->ReleaseByteArrayElements(env, digest, (jbyte *)digest_data, JNI_ABORT);
    }
}

JNIEXPORT jint JNICALL Java_sun_security_ec_NativeSunEC_ecdsaVerifySignedDigest
  (JNIEnv *env, jclass clazz, jint curveNID,
   jbyteArray pubKey, jbyteArray digest, jbyteArray signature) {
    EC_GROUP *group = NULL;
    EC_POINT *pub_point = NULL;
    EC_KEY *ec_key = NULL;
    ECDSA_SIG *ecdsa_sig = NULL;
    BIGNUM *r = NULL, *s = NULL;
    jint result = -1;
    unsigned char *pub_key_bytes = NULL;
    unsigned char *sig_bytes = NULL;
    unsigned char *digest_bytes = NULL;

    group = EC_GROUP_new_by_curve_name(curveNID);
    if (!group) {
        sunec_throw(env, INVALID_ALGO_PARAM_EXCEPTION, "Unsupported curve");
        goto cleanup;
    }

    jsize pub_key_len = (*env)->GetArrayLength(env, pubKey);
    pub_key_bytes = (unsigned char *)(*env)->GetByteArrayElements(env, pubKey, NULL);
    if (!pub_key_bytes || pub_key_len <= 0) {
        sunec_throw(env, INVALID_KEY_EXCEPTION, "Empty public key");
        goto cleanup;
    }

    pub_point = EC_POINT_new(group);
    if (!pub_point || !EC_POINT_oct2point(group, pub_point, pub_key_bytes, pub_key_len, NULL)) {
        sunec_throw(env, INVALID_KEY_EXCEPTION, "EC public point initialization failed");
        goto cleanup;
    }

    ec_key = EC_KEY_new();
    if (!ec_key || !EC_KEY_set_group(ec_key, group) || !EC_KEY_set_public_key(ec_key, pub_point)) {
        sunec_throw(env, INVALID_KEY_EXCEPTION, "EC key initialization failed");
        goto cleanup;
    }

    jsize sig_len = (*env)->GetArrayLength(env, signature);
    sig_bytes = (unsigned char *)(*env)->GetByteArrayElements(env, signature, NULL);
    if (!sig_bytes) {
        sunec_throw(env, SIGNATURE_EXCEPTION, "Invalid signature buffer");
        goto cleanup;
    }

    const int order_bits = EC_GROUP_order_bits(group);
    const int key_size = (order_bits + 7) / 8;
    if (sig_len != key_size * 2) {
        sunec_throw(env, SIGNATURE_EXCEPTION, "Invalid signature length");
        goto cleanup;
    }

    r = BN_bin2bn(sig_bytes, key_size, NULL);
    s = BN_bin2bn(sig_bytes + key_size, key_size, NULL);
    if (!r || !s) {
        sunec_throw(env, SIGNATURE_EXCEPTION, "Signature parsing failed");
        goto cleanup;
    }

    ecdsa_sig = ECDSA_SIG_new();
    if (!ecdsa_sig || !ECDSA_SIG_set0(ecdsa_sig, r, s)) {
        sunec_throw(env, SIGNATURE_EXCEPTION, "Signature structure error");
        goto cleanup;
    }

    jsize digest_len = (*env)->GetArrayLength(env, digest);
    digest_bytes = (unsigned char *)(*env)->GetByteArrayElements(env, digest, NULL);
    if (!digest_bytes || digest_len != key_size) {
        sunec_throw(env, ILLEGAL_STATE_EXCEPTION, "Invalid digest length");
        goto cleanup;
    }

    result = ECDSA_do_verify(digest_bytes, digest_len, ecdsa_sig, ec_key);
    if (result != 1 && result != 0) {
        sunec_throw(env, SIGNATURE_EXCEPTION, "Verification error");
    }

cleanup:
    if (pub_key_bytes) {
        (*env)->ReleaseByteArrayElements(env, pubKey, (jbyte *)pub_key_bytes, JNI_ABORT);
    }
    if (sig_bytes) {
        (*env)->ReleaseByteArrayElements(env, signature, (jbyte *)sig_bytes, JNI_ABORT);
    }
    if (digest_bytes) {
        (*env)->ReleaseByteArrayElements(env, digest, (jbyte *)digest_bytes, JNI_ABORT);
    }
    EC_GROUP_free(group);
    EC_POINT_free(pub_point);
    EC_KEY_free(ec_key);
    ECDSA_SIG_free(ecdsa_sig);

    return result;
}

JNIEXPORT void JNICALL Java_sun_security_ec_NativeSunEC_ecdhDeriveKey
  (JNIEnv *env, jclass clazz, jint curveNID,
   jbyteArray privKey, jbyteArray peerPubKey, jbyteArray sharedKeyOut) {
    EC_KEY *ec_key = NULL;
    EC_GROUP *group = NULL;
    jbyte *priv_key_data = NULL;
    BIGNUM *priv_key_bn = NULL;
    jbyte *peer_pub_key_data = NULL;
    EC_POINT *peer_pub_point = NULL;
    jbyte *shared_key_buf = NULL;

    group = EC_GROUP_new_by_curve_name(curveNID);
    if (!group) {
        sunec_throw(env, INVALID_ALGO_PARAM_EXCEPTION, "Unsupported curve NID");
        goto cleanup;
    }

    int order_bits = EC_GROUP_order_bits(group);
    int key_size = (order_bits + 7) / 8;

    int priv_key_len = (*env)->GetArrayLength(env, privKey);
    if (priv_key_len != key_size) {
        sunec_throw(env, INVALID_KEY_EXCEPTION, "Invalid private key length");
        goto cleanup;
    }

    int peer_pub_key_len = (*env)->GetArrayLength(env, peerPubKey);
    if (peer_pub_key_len != (1 + key_size * 2)) {
        sunec_throw(env, INVALID_KEY_EXCEPTION, "Invalid public key length");
        goto cleanup;
    }

    priv_key_data = (*env)->GetByteArrayElements(env, privKey, NULL);
    priv_key_bn = BN_bin2bn((unsigned char *)priv_key_data, priv_key_len, NULL);
    if (!priv_key_bn) {
        sunec_throw(env, INVALID_KEY_EXCEPTION, "Empty private key");
        goto cleanup;
    }

    ec_key = EC_KEY_new();
    if (!ec_key || !EC_KEY_set_group(ec_key, group)
        || !EC_KEY_set_private_key(ec_key, priv_key_bn)) {
        sunec_throw(env, INVALID_KEY_EXCEPTION, "EC key initialization failed");
        goto cleanup;
    }

    peer_pub_key_data = (*env)->GetByteArrayElements(env, peerPubKey, NULL);
    peer_pub_point = EC_POINT_new(group);
    if (!peer_pub_point
        || !EC_POINT_oct2point(group, peer_pub_point, (unsigned char *)peer_pub_key_data, peer_pub_key_len, NULL)) {
        sunec_throw(env, INVALID_KEY_EXCEPTION, "Public point initialization failed");
        goto cleanup;
    }

    if (!validate_point(group, peer_pub_point)) {
        sunec_throw(env, INVALID_KEY_EXCEPTION, "Invalid public point");
        goto cleanup;
    }

    shared_key_buf = (*env)->GetByteArrayElements(env, sharedKeyOut, NULL);
    if ((*env)->GetArrayLength(env, sharedKeyOut) != key_size) {
        sunec_throw(env, ILLEGAL_STATE_EXCEPTION, "Shared key buffer size mismatch");
        goto cleanup;
    }

    int actual_shared_key_len = ECDH_compute_key(shared_key_buf, key_size, peer_pub_point, ec_key, NULL);
    if (actual_shared_key_len != key_size) {
        sunec_throw(env, INVALID_KEY_EXCEPTION, "Derive key failed");
        goto cleanup;
    }

cleanup:
    if (shared_key_buf) {
        (*env)->ReleaseByteArrayElements(env, sharedKeyOut, (jbyte *)shared_key_buf, 0);
    }
    if (peer_pub_point) {
        EC_POINT_free(peer_pub_point);
    }
    if (ec_key) {
        EC_KEY_free(ec_key);
    }
    if (priv_key_bn) {
        BN_clear_free(priv_key_bn);
    }
    if (group) {
        EC_GROUP_free(group);
    }
    if (peer_pub_key_data) {
        (*env)->ReleaseByteArrayElements(env, peerPubKey, peer_pub_key_data, JNI_ABORT);
    }
    if (priv_key_data) {
        (*env)->ReleaseByteArrayElements(env, privKey, priv_key_data, JNI_ABORT);
    }
}
