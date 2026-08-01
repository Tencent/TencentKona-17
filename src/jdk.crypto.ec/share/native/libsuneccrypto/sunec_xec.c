/*
 * Copyright (C) 2025, 2026, Tencent. All rights reserved.
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

JNIEXPORT void JNICALL Java_sun_security_ec_NativeSunEC_xdhComputePubKey
  (JNIEnv *env, jclass clazz, jint curveNID,
   jbyteArray privKeyIn, jbyteArray pubKeyOut) {
    EVP_PKEY *pkey = NULL;
    uint8_t *priv_key_buf = NULL;
    uint8_t *pub_key_buf = NULL;

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

    if (privKeyIn == NULL || pubKeyOut == NULL) {
        sunec_throw(env, NULL_POINTER_EXCEPTION,
                "Private key input and public key output must not be null");
        goto cleanup;
    }

    jsize priv_key_len = (*env)->GetArrayLength(env, privKeyIn);
    jsize pub_key_len = (*env)->GetArrayLength(env, pubKeyOut);
    if (priv_key_len != key_len || pub_key_len != key_len) {
        sunec_throw(env, INVALID_KEY_EXCEPTION,
                evp_type == EVP_PKEY_X25519 ? "X25519 requires 32-byte buffer"
                                            : "X448 requires 56-byte buffer");
        goto cleanup;
    }

    // Copy the private key into a native buffer (scrubbed with
    // OPENSSL_clear_free) rather than pinning the Java array via
    // GetPrimitiveArrayCritical, which would expose the caller's key to
    // in-place zeroing.
    priv_key_buf = OPENSSL_malloc(priv_key_len);
    if (!priv_key_buf) {
        sunec_throw(env, ILLEGAL_STATE_EXCEPTION, "Memory allocation failed");
        goto cleanup;
    }
    (*env)->GetByteArrayRegion(env, privKeyIn, 0, priv_key_len, (jbyte*)priv_key_buf);
    if ((*env)->ExceptionCheck(env)) {
        goto cleanup;
    }

    pkey = EVP_PKEY_new_raw_private_key(evp_type, NULL, priv_key_buf, priv_key_len);
    if (!pkey) {
        sunec_throw(env, INVALID_KEY_EXCEPTION, "Invalid XDH private key");
        goto cleanup;
    }

    pub_key_buf = OPENSSL_malloc(pub_key_len);
    if (!pub_key_buf) {
        sunec_throw(env, ILLEGAL_STATE_EXCEPTION, "Memory allocation failed");
        goto cleanup;
    }

    size_t real_pub_key_len = (size_t)key_len;
    if (!EVP_PKEY_get_raw_public_key(pkey, pub_key_buf, &real_pub_key_len)
        || real_pub_key_len != (size_t)pub_key_len) {
        sunec_throw(env, INVALID_KEY_EXCEPTION, "Public key derivation failed");
        goto cleanup;
    }
    (*env)->SetByteArrayRegion(env, pubKeyOut, 0, pub_key_len, (jbyte*)pub_key_buf);

cleanup:
    if (priv_key_buf) {
        OPENSSL_clear_free(priv_key_buf, priv_key_len);
    }
    if (pub_key_buf) {
        OPENSSL_free(pub_key_buf);
    }
    if (pkey) {
        EVP_PKEY_free(pkey);
    }
}

JNIEXPORT void JNICALL Java_sun_security_ec_NativeSunEC_xdhDeriveKey
  (JNIEnv *env, jclass clazz, jint curveNID,
   jbyteArray privKey, jbyteArray peerPubKey, jbyteArray sharedKeyOut) {
    EVP_PKEY *priv_key = NULL;
    EVP_PKEY *peer_pub_key = NULL;
    EVP_PKEY_CTX *ctx = NULL;
    uint8_t *priv_key_data_buf = NULL;
    jbyte *pub_key_data = NULL;
    uint8_t *shared_key_buf = NULL;
    int shared_key_out_len = 0;

    int key_type;
    int key_len;
    switch (curveNID) {
        case NID_X25519:
            key_type = EVP_PKEY_X25519;
            key_len = 32;
            break;
        case NID_X448:
            key_type = EVP_PKEY_X448;
            key_len = 56;
            break;
        default:
            sunec_throw(env, INVALID_ALGO_PARAM_EXCEPTION, "Unsupported curve");
            return;
    }

    if (privKey == NULL || peerPubKey == NULL || sharedKeyOut == NULL) {
        sunec_throw(env, NULL_POINTER_EXCEPTION,
                "Private key, peer public key and shared key output must not be null");
        return;
    }

    if ((*env)->GetArrayLength(env, privKey) != key_len ||
        (*env)->GetArrayLength(env, peerPubKey) != key_len) {
        sunec_throw(env, INVALID_KEY_EXCEPTION, "Invalid key length");
        return;
    }

    if ((*env)->GetArrayLength(env, sharedKeyOut) != key_len) {
        sunec_throw(env, ILLEGAL_STATE_EXCEPTION, "Shared key buffer size mismatch");
        return;
    }

    // Copy the private key into a native buffer (scrubbed with
    // OPENSSL_clear_free) rather than cleansing the JNI array, which may alias
    // the Java heap. The peer public key is not sensitive.
    priv_key_data_buf = OPENSSL_malloc(key_len);
    if (!priv_key_data_buf) {
        sunec_throw(env, ILLEGAL_STATE_EXCEPTION, "Memory allocation failed");
        goto cleanup;
    }
    (*env)->GetByteArrayRegion(env, privKey, 0, key_len, (jbyte*)priv_key_data_buf);
    if ((*env)->ExceptionCheck(env)) {
        goto cleanup;
    }
    pub_key_data = (*env)->GetByteArrayElements(env, peerPubKey, NULL);
    if (!pub_key_data) {
        sunec_throw(env, ILLEGAL_STATE_EXCEPTION, "Failed to access key buffer");
        goto cleanup;
    }

    priv_key = EVP_PKEY_new_raw_private_key(key_type, NULL, priv_key_data_buf, key_len);
    peer_pub_key = EVP_PKEY_new_raw_public_key(key_type, NULL, (unsigned char *)pub_key_data, key_len);
    if (!priv_key || !peer_pub_key) {
        sunec_throw(env, INVALID_KEY_EXCEPTION, "Invalid key");
        goto cleanup;
    }

    ctx = EVP_PKEY_CTX_new(priv_key, NULL);
    if (!ctx || !EVP_PKEY_derive_init(ctx)) {
        sunec_throw(env, ILLEGAL_STATE_EXCEPTION, "Derivation initialization failed");
        goto cleanup;
    }

    if (!EVP_PKEY_derive_set_peer(ctx, peer_pub_key)) {
        sunec_throw(env, ILLEGAL_STATE_EXCEPTION, "Failed to set peer public key");
        goto cleanup;
    }

    size_t shared_key_len = 0;
    if (!EVP_PKEY_derive(ctx, NULL, &shared_key_len)) {
        sunec_throw(env, ILLEGAL_STATE_EXCEPTION, "Failed to get the derived key length");
        goto cleanup;
    }

    // Derive into a native buffer, copy the result out to the Java array, then
    // scrub the native buffer so the shared secret does not linger in freed
    // native memory.
    shared_key_out_len = (int)shared_key_len;
    shared_key_buf = OPENSSL_malloc(shared_key_len);
    if (!shared_key_buf) {
        sunec_throw(env, ILLEGAL_STATE_EXCEPTION, "Memory allocation failed");
        goto cleanup;
    }

    if (!EVP_PKEY_derive(ctx, shared_key_buf, &shared_key_len)) {
        sunec_throw(env, INVALID_KEY_EXCEPTION, "Key derivation failed");
        goto cleanup;
    }
    if (shared_key_len != (size_t)key_len) {
        sunec_throw(env, INVALID_KEY_EXCEPTION, "Unexpected derived key length");
        goto cleanup;
    }
    (*env)->SetByteArrayRegion(env, sharedKeyOut, 0, (jsize)shared_key_len, (jbyte*)shared_key_buf);

cleanup:
    if (shared_key_buf) {
        OPENSSL_clear_free(shared_key_buf, shared_key_out_len);
    }
    if (pub_key_data) {
        (*env)->ReleaseByteArrayElements(env, peerPubKey, pub_key_data, JNI_ABORT);
    }
    if (priv_key_data_buf) {
        OPENSSL_clear_free(priv_key_data_buf, key_len);
    }
    EVP_PKEY_CTX_free(ctx);
    EVP_PKEY_free(priv_key);
    EVP_PKEY_free(peer_pub_key);
}
