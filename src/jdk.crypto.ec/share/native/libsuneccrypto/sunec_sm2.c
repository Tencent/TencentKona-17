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

#include <math.h>
#include <stdbool.h>

#include <openssl/ec.h>
#include <openssl/err.h>
#include <openssl/rand.h>

#include "sunec_util.h"

JNIEXPORT jbyteArray JNICALL Java_sun_security_ec_NativeSunEC_sm2Encrypt(
  JNIEnv *env, jclass clazz, jbyteArray pubKey,
  jbyteArray plaintext, jint plaintextOffset, jint plaintextLength) {
    EVP_PKEY *pkey = NULL;
    EVP_PKEY_CTX *ctx = NULL;
    size_t ciphertext_len;
    unsigned char *ciphertext_buf = NULL;
    jbyteArray ciphertext = NULL;
    jbyte *pub_key_bytes = NULL;
    jbyte *plaintext_bytes = NULL;
    EC_KEY *ec_key = NULL;
    EC_POINT *pub_point = NULL;

    pub_key_bytes = (*env)->GetByteArrayElements(env, pubKey, NULL);
    plaintext_bytes = (*env)->GetByteArrayElements(env, plaintext, NULL);
    if (!pub_key_bytes || !plaintext_bytes) {
        sunec_throw(env, NULL_POINTER_EXCEPTION, "Failed to access array elements");
        goto cleanup;
    }

    ec_key = EC_KEY_new_by_curve_name(NID_sm2);
    if (!ec_key) {
        sunec_throw(env, PROVIDER_EXCEPTION, "Failed to create SM2 key");
        goto cleanup;
    }

    pub_point = EC_POINT_new(EC_KEY_get0_group(ec_key));
    if (!pub_point) {
        sunec_throw(env, PROVIDER_EXCEPTION, "Failed to create EC point");
        goto cleanup;
    }

    if (!EC_POINT_oct2point(EC_KEY_get0_group(ec_key), pub_point,
            (unsigned char*)pub_key_bytes, 65, NULL)) {
        sunec_throw(env, INVALID_KEY_EXCEPTION, "Invalid public key format");
        goto cleanup;
    }

    if (!EC_KEY_set_public_key(ec_key, pub_point)) {
        sunec_throw(env, PROVIDER_EXCEPTION, "Failed to set public key");
        goto cleanup;
    }

    pkey = EVP_PKEY_new();
    if (!pkey) {
        sunec_throw(env, PROVIDER_EXCEPTION, "Failed to create EVP_PKEY");
        goto cleanup;
    }

    if (!EVP_PKEY_set1_EC_KEY(pkey, ec_key)) {
        sunec_throw(env, PROVIDER_EXCEPTION, "Failed to assign EC key to EVP_PKEY");
        goto cleanup;
    }

    ctx = EVP_PKEY_CTX_new(pkey, NULL);
    if (!ctx) {
        sunec_throw(env, PROVIDER_EXCEPTION, "Failed to create EVP_PKEY_CTX");
        goto cleanup;
    }

    if (!EVP_PKEY_encrypt_init(ctx)) {
        sunec_throw(env, PROVIDER_EXCEPTION, "Encryption initialization failed");
        goto cleanup;
    }

    if (!EVP_PKEY_encrypt(ctx, NULL, &ciphertext_len,
            (unsigned char*)plaintext_bytes + plaintextOffset, plaintextLength)) {
        sunec_throw(env, PROVIDER_EXCEPTION, "Failed to determine ciphertext length");
        goto cleanup;
    }

    ciphertext_buf = OPENSSL_malloc(ciphertext_len);
    if (!ciphertext_buf) {
        sunec_throw(env, PROVIDER_EXCEPTION, "Memory allocation failed");
        goto cleanup;
    }

    if (!EVP_PKEY_encrypt(ctx, ciphertext_buf, &ciphertext_len,
            (unsigned char*)plaintext_bytes + plaintextOffset, plaintextLength)) {
        sunec_throw(env, PROVIDER_EXCEPTION, "Encryption failed");
        goto cleanup;
    }

    ciphertext = (*env)->NewByteArray(env, ciphertext_len);
    if (ciphertext) {
        (*env)->SetByteArrayRegion(env, ciphertext, 0, ciphertext_len, (jbyte*)ciphertext_buf);
    } else {
        sunec_throw(env, PROVIDER_EXCEPTION, "Failed to create byte array");
    }

cleanup:
    if (ciphertext_buf) {
        OPENSSL_free(ciphertext_buf);
    }
    if (pub_point) {
        EC_POINT_free(pub_point);
    }
    if (ec_key) {
        EC_KEY_free(ec_key);
    }
    if (ctx) {
        EVP_PKEY_CTX_free(ctx);
    }
    if (pkey) {
        EVP_PKEY_free(pkey);
    }
    if (pub_key_bytes) {
        (*env)->ReleaseByteArrayElements(env, pubKey, pub_key_bytes, JNI_ABORT);
    }
    if (plaintext_bytes) {
        (*env)->ReleaseByteArrayElements(env, plaintext, plaintext_bytes, JNI_ABORT);
    }

    return ciphertext;
}

JNIEXPORT jbyteArray JNICALL Java_sun_security_ec_NativeSunEC_sm2Decrypt(
  JNIEnv *env, jclass clazz, jbyteArray privKey,
  jbyteArray ciphertext, jint ciphertextOffset, jint ciphertextLength) {
    EVP_PKEY *pkey = NULL;
    EVP_PKEY_CTX *ctx = NULL;
    size_t plaintext_len;
    unsigned char *plaintext_buf = NULL;
    jbyteArray plaintext = NULL;
    jbyte *priv_key_bytes = NULL;
    jbyte *ciphertext_bytes = NULL;
    EC_KEY *ec_key = NULL;
    BIGNUM *priv_bn = NULL;

    ec_key = EC_KEY_new_by_curve_name(NID_sm2);
    if (!ec_key) {
        sunec_throw(env, PROVIDER_EXCEPTION, "Failed to create SM2 key");
        goto cleanup;
    }

    priv_key_bytes = (*env)->GetByteArrayElements(env, privKey, NULL);
    ciphertext_bytes = (*env)->GetByteArrayElements(env, ciphertext, NULL);
    if (!priv_key_bytes || !ciphertext_bytes) {
        sunec_throw(env, NULL_POINTER_EXCEPTION, "Failed to access array elements");
        goto cleanup;
    }

    int priv_key_len = (*env)->GetArrayLength(env, privKey);
    priv_bn = BN_bin2bn((unsigned char*)priv_key_bytes, priv_key_len, NULL);
    if (!priv_bn) {
        sunec_throw(env, PROVIDER_EXCEPTION, "Failed to create BIGNUM from private key");
        goto cleanup;
    }

    if (!EC_KEY_set_private_key(ec_key, priv_bn)) {
        sunec_throw(env, INVALID_KEY_EXCEPTION, "Failed to set private key");
        goto cleanup;
    }

    pkey = EVP_PKEY_new();
    if (!pkey) {
        sunec_throw(env, PROVIDER_EXCEPTION, "Failed to create EVP_PKEY");
        goto cleanup;
    }

    if (!EVP_PKEY_set1_EC_KEY(pkey, ec_key)) {
        sunec_throw(env, PROVIDER_EXCEPTION, "Failed to assign EC key to EVP_PKEY");
        goto cleanup;
    }

    ctx = EVP_PKEY_CTX_new(pkey, NULL);
    if (!ctx) {
        sunec_throw(env, PROVIDER_EXCEPTION, "Failed to create EVP_PKEY_CTX");
        goto cleanup;
    }

    if (!EVP_PKEY_decrypt_init(ctx)) {
        sunec_throw(env, PROVIDER_EXCEPTION, "Decryption initialization failed");
        goto cleanup;
    }

    if (!EVP_PKEY_decrypt(ctx, NULL, &plaintext_len,
            (unsigned char*)ciphertext_bytes + ciphertextOffset, ciphertextLength)) {
        sunec_throw(env, PROVIDER_EXCEPTION, "Failed to determine plaintext length");
        goto cleanup;
    }

    plaintext_buf = OPENSSL_malloc(plaintext_len);
    if (!plaintext_buf) {
        sunec_throw(env, PROVIDER_EXCEPTION, "Memory allocation failed");
        goto cleanup;
    }

    if (!EVP_PKEY_decrypt(ctx, plaintext_buf, &plaintext_len,
            (unsigned char*)ciphertext_bytes + ciphertextOffset, ciphertextLength)) {
        sunec_throw(env, PROVIDER_EXCEPTION, "Decryption failed");
        goto cleanup;
    }

    plaintext = (*env)->NewByteArray(env, plaintext_len);
    if (plaintext) {
        (*env)->SetByteArrayRegion(env, plaintext, 0, plaintext_len, (jbyte*)plaintext_buf);
    } else {
        sunec_throw(env, PROVIDER_EXCEPTION, "Failed to create byte array");
    }

cleanup:
    if (plaintext_buf) {
        OPENSSL_free(plaintext_buf);
    }
    if (priv_bn) {
        BN_free(priv_bn);
    }
    if (ec_key) {
        EC_KEY_free(ec_key);
    }
    if (ctx) {
        EVP_PKEY_CTX_free(ctx);
    }
    if (pkey) {
        EVP_PKEY_free(pkey);
    }
    if (priv_key_bytes) {
        (*env)->ReleaseByteArrayElements(env, privKey, priv_key_bytes, JNI_ABORT);
    }
    if (ciphertext_bytes) {
        (*env)->ReleaseByteArrayElements(env, ciphertext, ciphertext_bytes, JNI_ABORT);
    }

    return plaintext;
}

JNIEXPORT jbyteArray JNICALL Java_sun_security_ec_NativeSunEC_sm2Sign(
  JNIEnv *env, jclass clazz, jbyteArray privKey, jbyteArray pubKey,
  jbyteArray id, jbyteArray message) {
    EVP_PKEY *pkey = NULL;
    EVP_PKEY_CTX *pctx = NULL;
    EVP_MD_CTX *mctx = NULL;
    EC_KEY *ec_key = NULL;
    BIGNUM *priv_bn = NULL;
    EC_POINT *pub_point = NULL;
    size_t sig_len = 0;
    unsigned char *sig_buf = NULL;
    jbyteArray sig = NULL;

    jbyte *priv_key_bytes = NULL;
    jbyte *pub_key_bytes = NULL;
    jbyte *id_bytes = NULL;
    jbyte *msg_bytes = NULL;

    priv_key_bytes = (*env)->GetByteArrayElements(env, privKey, NULL);
    id_bytes = (*env)->GetByteArrayElements(env, id, NULL);
    msg_bytes = (*env)->GetByteArrayElements(env, message, NULL);

    if (!priv_key_bytes || !id_bytes || !msg_bytes) {
        sunec_throw(env, NULL_POINTER_EXCEPTION, "Private key, ID or message cannot be null");
        goto cleanup;
    }

    jsize priv_key_len = (*env)->GetArrayLength(env, privKey);
    jsize id_len = (*env)->GetArrayLength(env, id);
    jsize msg_len = (*env)->GetArrayLength(env, message);

    ec_key = EC_KEY_new_by_curve_name(NID_sm2);
    if (!ec_key) {
        sunec_throw(env, PROVIDER_EXCEPTION, "Failed to create SM2 key");
        goto cleanup;
    }

    priv_bn = BN_bin2bn((unsigned char*)priv_key_bytes, priv_key_len, NULL);
    if (!priv_bn) {
        sunec_throw(env, PROVIDER_EXCEPTION, "Failed to create private key BIGNUM");
        goto cleanup;
    }

    if (!EC_KEY_set_private_key(ec_key, priv_bn)) {
        sunec_throw(env, SIGNATURE_EXCEPTION, "Failed to set private key");
        goto cleanup;
    }

    if (pubKey != NULL) {
        pub_key_bytes = (*env)->GetByteArrayElements(env, pubKey, NULL);
        if (!pub_key_bytes) {
            sunec_throw(env, SIGNATURE_EXCEPTION, "Failed to access pubKey array");
            goto cleanup;
        }

        jsize pub_key_len = (*env)->GetArrayLength(env, pubKey);

        pub_point = EC_POINT_new(EC_KEY_get0_group(ec_key));
        if (!pub_point) {
            sunec_throw(env, PROVIDER_EXCEPTION, "Failed to create EC point");
            goto cleanup;
        }

        if (!EC_POINT_oct2point(EC_KEY_get0_group(ec_key), pub_point,
                (unsigned char*)pub_key_bytes, pub_key_len, NULL)) {
            sunec_throw(env, SIGNATURE_EXCEPTION, "Invalid public key format");
            goto cleanup;
        }
    } else {
        const EC_GROUP *group = EC_KEY_get0_group(ec_key);
        pub_point = EC_POINT_new(group);
        if (!pub_point) {
            sunec_throw(env, PROVIDER_EXCEPTION, "Failed to create EC point");
            goto cleanup;
        }

        if (!EC_POINT_mul(group, pub_point, priv_bn, NULL, NULL, NULL)) {
            sunec_throw(env, PROVIDER_EXCEPTION, "Failed to generate public key");
            goto cleanup;
        }
    }

    if (!EC_KEY_set_public_key(ec_key, pub_point)) {
        sunec_throw(env, PROVIDER_EXCEPTION, "Failed to set public key");
        goto cleanup;
    }

    pkey = EVP_PKEY_new();
    if (!pkey) {
        sunec_throw(env, PROVIDER_EXCEPTION, "Failed to create EVP_PKEY");
        goto cleanup;
    }

    if (!EVP_PKEY_set1_EC_KEY(pkey, ec_key)) {
        sunec_throw(env, PROVIDER_EXCEPTION, "Failed to assign EC key to EVP_PKEY");
        goto cleanup;
    }

    pctx = EVP_PKEY_CTX_new(pkey, NULL);
    if (!pctx) {
        sunec_throw(env, PROVIDER_EXCEPTION, "Failed to create EVP_PKEY_CTX");
        goto cleanup;
    }

    if (id_len > 0) {
        if (!EVP_PKEY_CTX_set1_id(pctx, (unsigned char*)id_bytes, id_len)) {
            sunec_throw(env, SIGNATURE_EXCEPTION, "Failed to set SM2 ID");
            goto cleanup;
        }
    }

    mctx = EVP_MD_CTX_new();
    if (!mctx) {
        sunec_throw(env, PROVIDER_EXCEPTION, "Failed to create EVP_MD_CTX");
        goto cleanup;
    }

    EVP_MD_CTX_set_pkey_ctx(mctx, pctx);

    if (!EVP_DigestSignInit(mctx, NULL, EVP_sm3(), NULL, pkey)) {
        sunec_throw(env, PROVIDER_EXCEPTION, "Sign initialization failed");
        goto cleanup;
    }

    if (!EVP_DigestSign(mctx, NULL, &sig_len, (unsigned char*)msg_bytes, msg_len)) {
        sunec_throw(env, SIGNATURE_EXCEPTION, "Failed to get signature length");
        goto cleanup;
    }

    sig_buf = OPENSSL_malloc(sig_len);
    if (!sig_buf) {
        sunec_throw(env, PROVIDER_EXCEPTION, "Memory allocation failed");
        goto cleanup;
    }

    if (!EVP_DigestSign(mctx, sig_buf, &sig_len, (unsigned char*)msg_bytes, msg_len)) {
        sunec_throw(env, SIGNATURE_EXCEPTION, "Signing failed");
        goto cleanup;
    }

    sig = (*env)->NewByteArray(env, sig_len);
    if (sig) {
        (*env)->SetByteArrayRegion(env, sig, 0, sig_len, (jbyte*)sig_buf);
    } else {
        sunec_throw(env, PROVIDER_EXCEPTION, "Failed to create result array");
    }

cleanup:
    if (sig_buf) {
        OPENSSL_free(sig_buf);
    }
    if (mctx) {
        EVP_MD_CTX_free(mctx);
    }
    if (pctx) {
        EVP_PKEY_CTX_free(pctx);
    }
    if (pkey) {
        EVP_PKEY_free(pkey);
    }
    if (ec_key) {
        EC_KEY_free(ec_key);
    }
    if (priv_bn) {
        BN_free(priv_bn);
    }
    if (pub_point) {
        EC_POINT_free(pub_point);
    }

    if (priv_key_bytes) {
        (*env)->ReleaseByteArrayElements(env, privKey, priv_key_bytes, JNI_ABORT);
    }
    if (pub_key_bytes) {
        (*env)->ReleaseByteArrayElements(env, pubKey, pub_key_bytes, JNI_ABORT);
    }
    if (id_bytes) {
        (*env)->ReleaseByteArrayElements(env, id, id_bytes, JNI_ABORT);
    }
    if (msg_bytes) {
        (*env)->ReleaseByteArrayElements(env, message, msg_bytes, JNI_ABORT);
    }

    return sig;
}

JNIEXPORT jboolean JNICALL Java_sun_security_ec_NativeSunEC_sm2Verify(
  JNIEnv *env, jclass clazz, jbyteArray pubKey,
  jbyteArray id, jbyteArray message, jbyteArray signature) {
    EVP_PKEY *pkey = NULL;
    EVP_PKEY_CTX *pctx = NULL;
    EVP_MD_CTX *mctx = NULL;
    EC_KEY *ec_key = NULL;
    EC_POINT *pub_point = NULL;
    jboolean verified = JNI_FALSE;

    jbyte *pub_key_bytes = NULL;
    jbyte *id_bytes = NULL;
    jbyte *msg_bytes = NULL;
    jbyte *sig_bytes = NULL;

    pub_key_bytes = (*env)->GetByteArrayElements(env, pubKey, NULL);
    id_bytes = (*env)->GetByteArrayElements(env, id, NULL);
    msg_bytes = (*env)->GetByteArrayElements(env, message, NULL);
    sig_bytes = (*env)->GetByteArrayElements(env, signature, NULL);

    if (!pub_key_bytes || !id_bytes || !msg_bytes || !sig_bytes) {
        sunec_throw(env, NULL_POINTER_EXCEPTION, "Failed to access array elements");
        goto cleanup;
    }

    jsize pub_key_len = (*env)->GetArrayLength(env, pubKey);
    jsize id_len = (*env)->GetArrayLength(env, id);
    jsize msg_len = (*env)->GetArrayLength(env, message);
    jsize sig_len = (*env)->GetArrayLength(env, signature);

    ec_key = EC_KEY_new_by_curve_name(NID_sm2);
    if (!ec_key) {
        sunec_throw(env, PROVIDER_EXCEPTION, "Failed to create SM2 key");
        goto cleanup;
    }

    pub_point = EC_POINT_new(EC_KEY_get0_group(ec_key));
    if (!pub_point) {
        sunec_throw(env, PROVIDER_EXCEPTION, "Failed to create EC point");
        goto cleanup;
    }

    if (!EC_POINT_oct2point(EC_KEY_get0_group(ec_key), pub_point,
            (unsigned char*)pub_key_bytes, pub_key_len, NULL)) {
        sunec_throw(env, SIGNATURE_EXCEPTION, "Invalid public key format");
        goto cleanup;
    }

    if (!EC_KEY_set_public_key(ec_key, pub_point)) {
        sunec_throw(env, SIGNATURE_EXCEPTION, "Failed to set public key");
        goto cleanup;
    }

    pkey = EVP_PKEY_new();
    if (!pkey) {
        sunec_throw(env, PROVIDER_EXCEPTION, "Failed to create EVP_PKEY");
        goto cleanup;
    }

    if (!EVP_PKEY_set1_EC_KEY(pkey, ec_key)) {
        sunec_throw(env, PROVIDER_EXCEPTION, "Failed to assign EC key to EVP_PKEY");
        goto cleanup;
    }

    pctx = EVP_PKEY_CTX_new(pkey, NULL);
    if (!pctx) {
        sunec_throw(env, PROVIDER_EXCEPTION, "Failed to create EVP_PKEY_CTX");
        goto cleanup;
    }

    if (id_len > 0) {
        if (!EVP_PKEY_CTX_set1_id(pctx, (unsigned char*)id_bytes, id_len)) {
            sunec_throw(env, SIGNATURE_EXCEPTION, "Failed to set SM2 ID");
            goto cleanup;
        }
    }

    mctx = EVP_MD_CTX_new();
    if (!mctx) {
        sunec_throw(env, PROVIDER_EXCEPTION, "Failed to create EVP_MD_CTX");
        goto cleanup;
    }

    EVP_MD_CTX_set_pkey_ctx(mctx, pctx);

    if (!EVP_DigestVerifyInit(mctx, NULL, EVP_sm3(), NULL, pkey)) {
        sunec_throw(env, SIGNATURE_EXCEPTION, "Verify initialization failed");
        goto cleanup;
    }

    int verify_val = EVP_DigestVerify(mctx, (unsigned char*)sig_bytes, sig_len,
            (unsigned char*)msg_bytes, msg_len);
    if (verify_val == 1) {
        verified = JNI_TRUE;
    } else if (verify_val == 0) {
        verified = JNI_FALSE;
    } else {
        sunec_throw(env, SIGNATURE_EXCEPTION, "Error during verification");
        goto cleanup;
    }

cleanup:
    if (mctx) {
        EVP_MD_CTX_free(mctx);
    }
    if (pctx) {
        EVP_PKEY_CTX_free(pctx);
    }
    if (pkey) {
        EVP_PKEY_free(pkey);
    }
    if (ec_key) {
        EC_KEY_free(ec_key);
    }
    if (pub_point) {
        EC_POINT_free(pub_point);
    }

    if (pub_key_bytes) {
        (*env)->ReleaseByteArrayElements(env, pubKey, pub_key_bytes, JNI_ABORT);
    }
    if (id_bytes) {
        (*env)->ReleaseByteArrayElements(env, id, id_bytes, JNI_ABORT);
    }
    if (msg_bytes) {
        (*env)->ReleaseByteArrayElements(env, message, msg_bytes, JNI_ABORT);
    }
    if (sig_bytes) {
        (*env)->ReleaseByteArrayElements(env, signature, sig_bytes, JNI_ABORT);
    }

    return verified;
}

typedef struct {
    const uint8_t* id;
    size_t id_len;
} SM2_ID;

typedef struct {
    const uint8_t* field;
    size_t field_len;

    const uint8_t* order;
    size_t order_len;

    const uint8_t* a;
    size_t a_len;

    const uint8_t* b;
    size_t b_len;

    const uint8_t* gen_x;
    size_t gen_x_len;

    const uint8_t* gen_y;
    size_t gen_y_len;
} SM2_CURVE;

typedef struct {
    EVP_MD_CTX* sm3_ctx;
    BN_CTX* bn_ctx;
} SM2_KEYEX_CTX;

typedef struct {
    BIGNUM* pri_key;
    EC_POINT* pub_key;
    BIGNUM* e_pri_key;
    uint8_t* id;
    size_t id_len;

    EC_POINT* peer_pub_key;
    EC_POINT* peer_e_pub_key;
    uint8_t* peer_id;
    size_t peer_id_len;
} SM2_KEYEX_PARAMS;

static const uint8_t SM2_DEFAULT_ID[] = {
        0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38,
        0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38
};

const SM2_ID* sm2_default_id() {
    static const SM2_ID instance = {
            .id = SM2_DEFAULT_ID,
            .id_len = sizeof(SM2_DEFAULT_ID)
    };

    return &instance;
}

static const uint8_t FIELD[] = {
        0xFF, 0xFF, 0xFF, 0xFE, 0xFF, 0xFF, 0xFF, 0xFF,
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
        0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00,
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF
};

static const uint8_t ORDER[] = {
        0xFF, 0xFF, 0xFF, 0xFE, 0xFF, 0xFF, 0xFF, 0xFF,
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
        0x72, 0x03, 0xDF, 0x6B, 0x21, 0xC6, 0x05, 0x2B,
        0x53, 0xBB, 0xF4, 0x09, 0x39, 0xD5, 0x41, 0x23
};

static const uint8_t A[] = {
        0xFF, 0xFF, 0xFF, 0xFE, 0xFF, 0xFF, 0xFF, 0xFF,
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
        0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00,
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFC
};

static const uint8_t B[] = {
        0x28, 0xE9, 0xFA, 0x9E, 0x9D, 0x9F, 0x5E, 0x34,
        0x4D, 0x5A, 0x9E, 0x4B, 0xCF, 0x65, 0x09, 0xA7,
        0xF3, 0x97, 0x89, 0xF5, 0x15, 0xAB, 0x8F, 0x92,
        0xDD, 0xBC, 0xBD, 0x41, 0x4D, 0x94, 0x0E, 0x93
};

static const uint8_t GEN_X[] = {
        0x32, 0xC4, 0xAE, 0x2C, 0x1F, 0x19, 0x81, 0x19,
        0x5F, 0x99, 0x04, 0x46, 0x6A, 0x39, 0xC9, 0x94,
        0x8F, 0xE3, 0x0B, 0xBF, 0xF2, 0x66, 0x0B, 0xE1,
        0x71, 0x5A, 0x45, 0x89, 0x33, 0x4C, 0x74, 0xC7
};

static const uint8_t GEN_Y[] = {
        0xBC, 0x37, 0x36, 0xA2, 0xF4, 0xF6, 0x77, 0x9C,
        0x59, 0xBD, 0xCE, 0xE3, 0x6B, 0x69, 0x21, 0x53,
        0xD0, 0xA9, 0x87, 0x7C, 0xC6, 0x2A, 0x47, 0x40,
        0x02, 0xDF, 0x32, 0xE5, 0x21, 0x39, 0xF0, 0xA0
};

const SM2_CURVE* sm2_curve() {
    static const SM2_CURVE instance = {
        .field_len = sizeof(FIELD),
        .order = ORDER,
        .order_len = sizeof(ORDER),
        .a = A,
        .a_len = sizeof(A),
        .b = B,
        .b_len = sizeof(B),
        .gen_x = GEN_X,
        .gen_x_len = sizeof(GEN_X),
        .gen_y = GEN_Y,
        .gen_y_len = sizeof(GEN_Y)
    };

    return &instance;
}

const EC_GROUP* sm2_group() {
    static const EC_GROUP* sm2_group = NULL;

    if (sm2_group == NULL) {
        EC_GROUP* group = EC_GROUP_new_by_curve_name(NID_sm2);
        if (group == NULL) {
            return NULL;
        }
        sm2_group = group;
    }

    return sm2_group;
}

EVP_MD_CTX* sm3_create_ctx() {
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (ctx == NULL) {
        return NULL;
    }

    const EVP_MD* md = EVP_sm3();
    if (md == NULL) {
        return NULL;
    }

    if (!EVP_DigestInit_ex(ctx, md, NULL)) {
        return NULL;
    }

    return ctx;
}

int sm3_reset(EVP_MD_CTX* ctx) {
    if (ctx == NULL) {
        return OPENSSL_FAILURE;
    }

    if (!EVP_DigestInit_ex(ctx, NULL, NULL)) {
        return OPENSSL_FAILURE;
    }

    return OPENSSL_SUCCESS;
}

SM2_KEYEX_CTX* sm2_create_keyex_ctx() {
    EVP_MD_CTX* sm3_ctx = sm3_create_ctx();
    if (sm3_ctx == NULL) {
        return NULL;
    }

    BN_CTX* bn_ctx = BN_CTX_new();
    if (bn_ctx == NULL) {
        return NULL;
    }

    SM2_KEYEX_CTX* ctx = OPENSSL_malloc(sizeof(SM2_KEYEX_CTX));
    if (ctx == NULL) {
        return NULL;
    }
    ctx->sm3_ctx = sm3_ctx;
    ctx->bn_ctx = bn_ctx;

    return ctx;
}

void sm2_free_keyex_ctx(SM2_KEYEX_CTX* ctx) {
    if (ctx != NULL) {
        if (ctx->sm3_ctx != NULL) {
            EVP_MD_CTX_free(ctx->sm3_ctx);
            ctx->sm3_ctx = NULL;
        }
        if (ctx->bn_ctx != NULL) {
            BN_CTX_free(ctx->bn_ctx);
            ctx->bn_ctx = NULL;
        }

        OPENSSL_free(ctx);
    }
}

int z(uint8_t* out, SM2_KEYEX_CTX* ctx,
      const uint8_t* id, const size_t id_len,
      const EC_GROUP* group, const EC_POINT* point) {
    const SM2_ID* default_id = sm2_default_id();
    const SM2_CURVE* curve = sm2_curve();

    const uint8_t* id_to_use = id ? id : default_id->id;
    size_t id_bytes_len = id ? id_len : default_id->id_len;
    int id_bits_len = id_bytes_len << 3;

    uint8_t id_len_high = (id_bits_len >> 8) & 0xFF;
    uint8_t id_len_low = id_bits_len & 0xFF;
    if (!EVP_DigestUpdate(ctx->sm3_ctx, &id_len_high, 1) ||
        !EVP_DigestUpdate(ctx->sm3_ctx, &id_len_low, 1) ||
        !EVP_DigestUpdate(ctx->sm3_ctx, id_to_use, id_bytes_len) ||

        !EVP_DigestUpdate(ctx->sm3_ctx, curve->a, curve->a_len) ||
        !EVP_DigestUpdate(ctx->sm3_ctx, curve->b, curve->b_len) ||

        !EVP_DigestUpdate(ctx->sm3_ctx, curve->gen_x, curve->gen_x_len) ||
        !EVP_DigestUpdate(ctx->sm3_ctx, curve->gen_y, curve->gen_y_len)) {
        return OPENSSL_FAILURE;
    }

    int ret = OPENSSL_FAILURE;

    BIGNUM* x_bn = BN_new();
    BIGNUM* y_bn = BN_new();
    if (x_bn == NULL || y_bn == NULL) {
        goto cleanup;
    }

    if (!EC_POINT_get_affine_coordinates(group, point, x_bn, y_bn, ctx->bn_ctx)) {
        goto cleanup;
    }

    uint8_t x_bytes[32] = {0};
    uint8_t y_bytes[32] = {0};
    if (!BN_bn2binpad(x_bn, x_bytes, sizeof(x_bytes)) ||
        !BN_bn2binpad(y_bn, y_bytes, sizeof(y_bytes))) {
        goto cleanup;
    }

    if (!EVP_DigestUpdate(ctx->sm3_ctx, x_bytes, sizeof(x_bytes)) ||
        !EVP_DigestUpdate(ctx->sm3_ctx, y_bytes, sizeof(y_bytes))) {
        goto cleanup;
    }

    unsigned int len = 0;
    if (!EVP_DigestFinal_ex(ctx->sm3_ctx, out, &len) || len != SM3_DIGEST_LEN) {
        ret = OPENSSL_FAILURE;
    }

    if (!sm3_reset(ctx->sm3_ctx)) {
        goto cleanup;
    }

    ret = OPENSSL_SUCCESS;

cleanup:
    if (x_bn) BN_clear_free(x_bn);
    if (y_bn) BN_clear_free(y_bn);

    OPENSSL_cleanse(x_bytes, sizeof(x_bytes));
    OPENSSL_cleanse(y_bytes, sizeof(y_bytes));

    return ret;
}

int kdf(uint8_t* key_out, const int key_len, EVP_MD_CTX* sm3_ctx, const uint8_t* in, size_t in_len) {
    int remainder = key_len % SM3_DIGEST_LEN;
    int count = key_len / SM3_DIGEST_LEN + (remainder == 0 ? 0 : 1);

    for (int i = 1; i <= count; i++) {
        uint8_t digest[SM3_DIGEST_LEN];
        if (!EVP_DigestUpdate(sm3_ctx, in, in_len)) {
            return OPENSSL_FAILURE;
        }

        uint8_t counter[4] = { (i >> 24) & 0xFF, (i >> 16) & 0xFF, (i >> 8) & 0xFF, i & 0xFF };
        if (!EVP_DigestUpdate(sm3_ctx, counter, 4) ||
            !EVP_DigestFinal_ex(sm3_ctx, digest, NULL) ||
            !sm3_reset(sm3_ctx)) {
            return OPENSSL_FAILURE;
        }

        int length = (i == count && remainder != 0) ? remainder : SM3_DIGEST_LEN;
        memcpy(key_out + (i - 1) * SM3_DIGEST_LEN, digest, length);
    }

    return OPENSSL_SUCCESS;
}

void combine(uint8_t* combined_out,
             const uint8_t* vX, const uint8_t* vY,
             const uint8_t* zA, const uint8_t* zB,
             const bool is_initiator) {
    memcpy(combined_out, vX, 32);
    memcpy(combined_out + 32, vY, 32);

    if (is_initiator) {
        memcpy(combined_out + 32 + 32, zA, 32);
        memcpy(combined_out + 32 + 32 + 32, zB, 32);
    } else {
        memcpy(combined_out + 32 + 32, zB, 32);
        memcpy(combined_out + 32 + 32 + 32, zA, 32);
    }
}

int calc_bar(BIGNUM* x, BIGNUM* two_pow_w, BIGNUM* two_pow_w_sub_one) {
    return BN_mask_bits(x, BN_num_bits(two_pow_w_sub_one)) && BN_add(x, two_pow_w, x);
}

int sm2_derive_key(uint8_t* key_out, int key_len,
                   SM2_KEYEX_CTX* ctx, const SM2_KEYEX_PARAMS* params,
                   bool is_initiator) {
    const EC_GROUP* group = sm2_group();
    if (group == NULL) {
        return OPENSSL_FAILURE;
    }

    BIGNUM* order = BN_new();
    BIGNUM* order_minus_one = BN_new();
    BIGNUM* two_pow_w = BN_new();
    BIGNUM* two_pow_w_sub_one = BN_new();
    BIGNUM* x1 = BN_new();
    BIGNUM* tA = BN_new();
    BIGNUM* x2 = BN_new();
    BIGNUM* cofactor = BN_new();
    BIGNUM* vX_bn = BN_new();
    BIGNUM* vY_bn = BN_new();
    EC_POINT* rA_p = NULL;
    EC_POINT* interim_p = NULL;
    EC_POINT* u_p = NULL;
    uint8_t* zA = NULL;
    uint8_t* zB = NULL;
    uint8_t* combined = NULL;
    int ret = OPENSSL_FAILURE;

    if (order == NULL || order_minus_one == NULL || two_pow_w == NULL || two_pow_w_sub_one == NULL ||
        x1 == NULL || tA == NULL || x2 == NULL || cofactor == NULL || vX_bn == NULL || vY_bn == NULL) {
        goto cleanup;
    }

    if (!EC_GROUP_get_order(group, order, ctx->bn_ctx)) {
        goto cleanup;
    }

    if (!BN_sub(order_minus_one, order, BN_value_one())) {
        goto cleanup;
    }

    int bit_length = BN_num_bits(order_minus_one);
    int w = (int)ceil((double)bit_length / 2) - 1;

    if (!BN_lshift(two_pow_w, BN_value_one(), w) ||
        !BN_sub(two_pow_w_sub_one, two_pow_w, BN_value_one())) {
        goto cleanup;
    }

    const BIGNUM* rA = params->e_pri_key;

    rA_p = EC_POINT_new(group);
    if (rA_p == NULL || !EC_POINT_mul(group, rA_p, rA, NULL, NULL, ctx->bn_ctx)) {
        goto cleanup;
    }

    if (!EC_POINT_get_affine_coordinates(group, rA_p, x1, NULL, ctx->bn_ctx) ||
        !calc_bar(x1, two_pow_w, two_pow_w_sub_one)) {
        goto cleanup;
    }

    if (!BN_mul(x1, x1, rA, ctx->bn_ctx) ||
        !BN_add(x1, x1, params->pri_key) ||
        !BN_mod(tA, x1, order, ctx->bn_ctx)) {
        goto cleanup;
    }

    if (!EC_POINT_get_affine_coordinates(group, params->peer_e_pub_key, x2, NULL, ctx->bn_ctx) ||
        !calc_bar(x2, two_pow_w, two_pow_w_sub_one)) {
        goto cleanup;
    }

    interim_p = EC_POINT_new(group);
    if (interim_p == NULL || !EC_POINT_mul(group, interim_p, NULL, params->peer_e_pub_key, x2, ctx->bn_ctx) ||
        !EC_POINT_add(group, interim_p, interim_p, params->peer_pub_key, ctx->bn_ctx)) {
        goto cleanup;
    }

    if (!EC_GROUP_get_cofactor(group, cofactor, ctx->bn_ctx)) {
        goto cleanup;
    }

    u_p = EC_POINT_new(group);
    if (u_p == NULL ||
        !BN_mul(tA, tA, cofactor, ctx->bn_ctx) ||
        !EC_POINT_mul(group, u_p, NULL, interim_p, tA, ctx->bn_ctx)) {
        goto cleanup;
    }

    if (EC_POINT_is_at_infinity(group, u_p)) {
        goto cleanup;
    }

    if (!EC_POINT_get_affine_coordinates(group, u_p, vX_bn, vY_bn, ctx->bn_ctx)) {
        goto cleanup;
    }

    int vX_len = BN_num_bytes(vX_bn);
    int vY_len = BN_num_bytes(vY_bn);
    if (vX_len > 32 || vY_len > 32) {
        goto cleanup;
    }

    uint8_t vX[32] = {0};
    uint8_t vY[32] = {0};
    if (!BN_bn2bin(vX_bn, vX + (32 - vX_len)) ||
        !BN_bn2bin(vY_bn, vY + (32 - vY_len))) {
        goto cleanup;
    }

    zA = OPENSSL_malloc(32);
    zB = OPENSSL_malloc(32);
    if (zA == NULL || zB == NULL ||
        !z(zA, ctx, params->id, params->id_len, group, params->pub_key) ||
        !z(zB, ctx, params->peer_id, params->peer_id_len, group, params->peer_pub_key)) {
        goto cleanup;
    }

    combined = OPENSSL_malloc(128);
    if (combined == NULL) {
        goto cleanup;
    }
    combine(combined, vX, vY, zA, zB, is_initiator);

    if (!kdf(key_out, key_len, ctx->sm3_ctx, combined, 128)) {
        goto cleanup;
    }

    ret = OPENSSL_SUCCESS;

cleanup:
    BN_free(order);
    BN_free(order_minus_one);
    BN_free(two_pow_w);
    BN_free(two_pow_w_sub_one);
    BN_free(x1);
    BN_free(tA);
    BN_free(x2);
    BN_free(cofactor);
    BN_free(vX_bn);
    BN_free(vY_bn);
    EC_POINT_free(rA_p);
    EC_POINT_free(interim_p);
    EC_POINT_free(u_p);
    OPENSSL_free(zA);
    OPENSSL_free(zB);
    OPENSSL_free(combined);

    return ret;
}

BIGNUM* sm2_pri_key(const uint8_t* pri_key_bytes) {
    BIGNUM* pri_key = BN_new();
    if (pri_key == NULL) {
        return NULL;
    }

    if (BN_bin2bn(pri_key_bytes, 32, pri_key) == NULL) {
        return NULL;
    }

    return pri_key;
}

EC_POINT* sm2_pub_key(const uint8_t* pub_key_bytes, const size_t pub_key_len) {
    const EC_GROUP* group = sm2_group();

    EC_POINT* pub_key = EC_POINT_new(group);
    if (pub_key == NULL) {
        return NULL;
    }

    if(!EC_POINT_oct2point(group, pub_key, pub_key_bytes, pub_key_len, NULL)) {
        return NULL;
    }

    return pub_key;
}

int sm2_check_point_order(const EC_GROUP* group, const EC_POINT *point) {
    BIGNUM* order = BN_new();
    if (order == NULL) {
        return OPENSSL_FAILURE;
    }

    if (!EC_GROUP_get_order(group, order, NULL)) {
        BN_free(order);

        return OPENSSL_FAILURE;
    }

    EC_POINT* product = EC_POINT_new(group);
    if (product == NULL) {
        BN_free(order);

        return OPENSSL_FAILURE;
    }

    if (!EC_POINT_mul(group, product, NULL, point, order, NULL)) {
        BN_free(order);
        EC_POINT_free(product);

        return OPENSSL_FAILURE;
    }

    if (!EC_POINT_is_at_infinity(group, product)) {
        BN_free(order);
        EC_POINT_free(product);

        return OPENSSL_FAILURE;
    }

    BN_free(order);
    EC_POINT_free(product);

    return OPENSSL_SUCCESS;
}

int sm2_validate_point(EC_POINT *point) {
    return EC_POINT_is_on_curve(sm2_group(), point, NULL) &&
           sm2_check_point_order(sm2_group(), point);
}

JNIEXPORT jbyteArray JNICALL Java_sun_security_ec_NativeSunEC_sm2DeriveKey(
  JNIEnv* env, jclass classObj,
  jbyteArray priKey, jbyteArray pubKey, jbyteArray ePriKey, jbyteArray id,
  jbyteArray peerPubKey, jbyteArray peerEPubKey, jbyteArray peerId,
  jboolean isInitiator, jint sharedKeyLength) {
    SM2_KEYEX_CTX* ctx = sm2_create_keyex_ctx();
    if (ctx == NULL) {
        sunec_throw(env, PROVIDER_EXCEPTION, "Failed to create SM2 key exchange context");
        return NULL;
    }

    jbyte* pri_key_bytes = NULL;
    jbyte* pub_key_bytes = NULL;
    jbyte* e_pri_key_bytes = NULL;
    jbyte* id_bytes = NULL;
    jbyte* peer_pub_key_bytes = NULL;
    jbyte* peer_e_pub_key_bytes = NULL;
    jbyte* peer_id_bytes = NULL;
    BIGNUM* pri_key = NULL;
    EC_POINT* pub_key = NULL;
    BIGNUM* e_pri_key = NULL;
    EC_POINT* peer_pub_key = NULL;
    EC_POINT* peer_e_pub_key = NULL;
    SM2_KEYEX_PARAMS* params = NULL;
    uint8_t* shared_key_buf = NULL;
    jbyteArray shared_key_bytes = NULL;

    int pri_key_len = (*env)->GetArrayLength(env, priKey);
    if (pri_key_len != SM2_PRI_KEY_LEN) {
        sunec_throw(env, ILLEGAL_STATE_EXCEPTION, "Wrong SM2 private key length");
        goto cleanup;
    }
    pri_key_bytes = (*env)->GetByteArrayElements(env, priKey, NULL);
    if (pri_key_bytes == NULL) {
        sunec_throw(env, ILLEGAL_STATE_EXCEPTION, "Failed to get SM2 private key");
        goto cleanup;
    }

    int pub_key_len = (*env)->GetArrayLength(env, pubKey);
    if (pub_key_len != SM2_PUB_KEY_LEN) {
        sunec_throw(env, ILLEGAL_STATE_EXCEPTION, "Wrong SM2 public key length");
        goto cleanup;
    }
    pub_key_bytes = (*env)->GetByteArrayElements(env, pubKey, NULL);
    if (pub_key_bytes == NULL) {
        sunec_throw(env, ILLEGAL_STATE_EXCEPTION, "Failed to get SM2 public key");
        goto cleanup;
    }

    int e_pri_key_len = (*env)->GetArrayLength(env, ePriKey);
    if (e_pri_key_len != SM2_PRI_KEY_LEN) {
        sunec_throw(env, ILLEGAL_STATE_EXCEPTION, "Wrong SM2 ephemeral private key length");
        goto cleanup;
    }
    e_pri_key_bytes = (*env)->GetByteArrayElements(env, ePriKey, NULL);
    if (e_pri_key_bytes == NULL) {
        sunec_throw(env, ILLEGAL_STATE_EXCEPTION, "Failed to get SM2 ephemeral private key");
        goto cleanup;
    }

    int id_len = (*env)->GetArrayLength(env, id);
    if (id_len <= 0) {
        sunec_throw(env, ILLEGAL_STATE_EXCEPTION, "Invalid ID length");
        goto cleanup;
    }
    id_bytes = (*env)->GetByteArrayElements(env, id, NULL);
    if (id_bytes == NULL) {
        sunec_throw(env, ILLEGAL_STATE_EXCEPTION, "Failed to get ID");
        goto cleanup;
    }

    int peer_pub_key_len = (*env)->GetArrayLength(env, peerPubKey);
    if (peer_pub_key_len != SM2_PUB_KEY_LEN) {
        sunec_throw(env, ILLEGAL_STATE_EXCEPTION, "Wrong peer SM2 public key length");
        goto cleanup;
    }
    peer_pub_key_bytes = (*env)->GetByteArrayElements(env, peerPubKey, NULL);
    if (peer_pub_key_bytes == NULL) {
        sunec_throw(env, ILLEGAL_STATE_EXCEPTION, "Failed to get peer SM2 public key");
        goto cleanup;
    }

    int peer_e_pub_key_len = (*env)->GetArrayLength(env, peerEPubKey);
    if (peer_e_pub_key_len != SM2_PUB_KEY_LEN) {
        sunec_throw(env, ILLEGAL_STATE_EXCEPTION, "Wrong peer SM2 ephemeral public key length");
        goto cleanup;
    }
    peer_e_pub_key_bytes = (*env)->GetByteArrayElements(env, peerEPubKey, NULL);
    if (peer_e_pub_key_bytes == NULL) {
        sunec_throw(env, ILLEGAL_STATE_EXCEPTION, "Failed to get peer SM2 ephemeral public key");
        goto cleanup;
    }

    int peer_id_len = (*env)->GetArrayLength(env, peerId);
    if (peer_id_len <= 0) {
        sunec_throw(env, ILLEGAL_STATE_EXCEPTION, "Invalid peer ID length");
        goto cleanup;
    }
    peer_id_bytes = (*env)->GetByteArrayElements(env, peerId, NULL);
    if (peer_id_bytes == NULL) {
        sunec_throw(env, ILLEGAL_STATE_EXCEPTION, "Failed to get peer ID");
        goto cleanup;
    }

    bool is_initiator = (bool)isInitiator;

    int shared_key_len = (int)sharedKeyLength;
    if (shared_key_len <= 0) {
        sunec_throw(env, ILLEGAL_STATE_EXCEPTION, "Invalid shared key length");
        goto cleanup;
    }

    pri_key = sm2_pri_key((const uint8_t *)pri_key_bytes);
    if (pri_key == NULL) {
        sunec_throw(env, PROVIDER_EXCEPTION, "Failed to create BIGNUM from private key");
        goto cleanup;
    }

    pub_key = sm2_pub_key((const uint8_t *)pub_key_bytes, pub_key_len);
    if (pub_key == NULL) {
        sunec_throw(env, PROVIDER_EXCEPTION, "Failed to create EC point from public key");
        goto cleanup;
    }

    e_pri_key = sm2_pri_key((const uint8_t *)e_pri_key_bytes);
    if (e_pri_key == NULL) {
        sunec_throw(env, PROVIDER_EXCEPTION, "Failed to create BIGNUM from ephemeral private key");
        goto cleanup;
    }

    peer_pub_key = sm2_pub_key((const uint8_t *)peer_pub_key_bytes, peer_pub_key_len);
    if (peer_pub_key == NULL) {
        sunec_throw(env, PROVIDER_EXCEPTION, "Failed to create EC point from peer public key");
        goto cleanup;
    }

    peer_e_pub_key = sm2_pub_key((const uint8_t *)peer_e_pub_key_bytes, peer_e_pub_key_len);
    if (peer_e_pub_key == NULL) {
        sunec_throw(env, PROVIDER_EXCEPTION, "Failed to create EC point from peer ephemeral public key");
        goto cleanup;
    }

    params = OPENSSL_malloc(sizeof(SM2_KEYEX_PARAMS));
    if (params == NULL) {
        sunec_throw(env, ILLEGAL_STATE_EXCEPTION, "Memory allocation failed");
        goto cleanup;
    }
    params->pri_key = pri_key;
    params->pub_key = pub_key;
    params->e_pri_key = e_pri_key;
    params->id = (uint8_t*)id_bytes;
    params->id_len = id_len;
    params->peer_pub_key = peer_pub_key;
    params->peer_e_pub_key = peer_e_pub_key;
    params->peer_id = (uint8_t*)peer_id_bytes;
    params->peer_id_len = peer_id_len;

    shared_key_buf = OPENSSL_malloc(shared_key_len);
    if (shared_key_buf == NULL) {
        sunec_throw(env, ILLEGAL_STATE_EXCEPTION, "Memory allocation failed");
        goto cleanup;
    }

    if (!sm2_derive_key(shared_key_buf, shared_key_len, ctx, params, is_initiator)) {
        sunec_throw(env, SIGNATURE_EXCEPTION, "Key derivation failed");
        goto cleanup;
    }

    shared_key_bytes = (*env)->NewByteArray(env, shared_key_len);
    if (shared_key_bytes == NULL) {
        sunec_throw(env, ILLEGAL_STATE_EXCEPTION, "Failed to create shared key");
        goto cleanup;
    }
    (*env)->SetByteArrayRegion(env, shared_key_bytes, 0, shared_key_len, (jbyte*)shared_key_buf);

cleanup:
    if (pri_key_bytes != NULL) {
        (*env)->ReleaseByteArrayElements(env, priKey, pri_key_bytes, JNI_ABORT);
    }
    if (pub_key_bytes != NULL) {
        (*env)->ReleaseByteArrayElements(env, pubKey, pub_key_bytes, JNI_ABORT);
    }
    if (e_pri_key_bytes != NULL) {
        (*env)->ReleaseByteArrayElements(env, ePriKey, e_pri_key_bytes, JNI_ABORT);
    }
    if (id_bytes != NULL) {
        (*env)->ReleaseByteArrayElements(env, id, id_bytes, JNI_ABORT);
    }
    if (peer_pub_key_bytes != NULL) {
        (*env)->ReleaseByteArrayElements(env, peerPubKey, peer_pub_key_bytes, JNI_ABORT);
    }
    if (peer_e_pub_key_bytes != NULL) {
        (*env)->ReleaseByteArrayElements(env, peerEPubKey, peer_e_pub_key_bytes, JNI_ABORT);
    }
    if (peer_id_bytes != NULL) {
        (*env)->ReleaseByteArrayElements(env, peerId, peer_id_bytes, JNI_ABORT);
    }
    BN_free(pri_key);
    EC_POINT_free(pub_key);
    BN_free(e_pri_key);
    EC_POINT_free(peer_pub_key);
    EC_POINT_free(peer_e_pub_key);
    OPENSSL_free(params);
    OPENSSL_free(shared_key_buf);
    sm2_free_keyex_ctx(ctx);

    return shared_key_bytes;
}
