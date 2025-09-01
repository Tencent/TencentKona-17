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

#include <openssl/evp.h>

#define SM3_DIGEST_LENGTH 32

#define INDEX_OUT_OF_BOUNDS_EXCEPTION "java/lang/IndexOutOfBoundsException"
#define ILLEGAL_STATE_EXCEPTION "java/lang/IllegalStateException"
#define INVALID_ALGO_PARAM_EXCEPTION "java/security/InvalidAlgorithmParameterException"
#define NULL_POINTER_EXCEPTION "java/lang/NullPointerException"

void sun_throw(JNIEnv *env, const char *exceptionName, const char *message) {
    jclass exceptionClazz = (*env)->FindClass(env, exceptionName);
    if (exceptionClazz != NULL) {
        (*env)->ThrowNew(env, exceptionClazz, message);
    }
}

JNIEXPORT void JNICALL Java_sun_security_provider_NativeSun_sm3Digest(
  JNIEnv *env, jclass clazz, jbyteArray message, jbyteArray out, jint outOffset) {
    if (message == NULL || out == NULL) {
        sun_throw(env, NULL_POINTER_EXCEPTION, "Input/output arrays must not be null");
        return;
    }

    jsize message_len = (*env)->GetArrayLength(env, message);
    jsize out_len = (*env)->GetArrayLength(env, out);

    if (outOffset < 0 || outOffset + SM3_DIGEST_LENGTH > out_len) {
        sun_throw(env, INDEX_OUT_OF_BOUNDS_EXCEPTION, "Output buffer too small");
        return;
    }

    const EVP_MD *sm3 = EVP_sm3();
    if (!sm3) {
        sun_throw(env, INVALID_ALGO_PARAM_EXCEPTION, "SM3 algorithm not available");
        return;
    }

    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    if (!ctx) {
        sun_throw(env, ILLEGAL_STATE_EXCEPTION, "Failed to create SM3 context");
        return;
    }

    if (1 != EVP_DigestInit_ex(ctx, sm3, NULL)) {
        EVP_MD_CTX_free(ctx);
        sun_throw(env, ILLEGAL_STATE_EXCEPTION, "SM3 initialization failed");
        return;
    }

    jbyte *message_bytes = (*env)->GetByteArrayElements(env, message, NULL);
    if (!message_bytes) {
        EVP_MD_CTX_free(ctx);
        sun_throw(env, ILLEGAL_STATE_EXCEPTION, "Failed to access message array");
        return;
    }

    if (1 != EVP_DigestUpdate(ctx, message_bytes, message_len)) {
        (*env)->ReleaseByteArrayElements(env, message, message_bytes, JNI_ABORT);
        EVP_MD_CTX_free(ctx);
        sun_throw(env, ILLEGAL_STATE_EXCEPTION, "SM3 update failed");
        return;
    }

    (*env)->ReleaseByteArrayElements(env, message, message_bytes, JNI_ABORT);

    jbyte *out_bytes = (*env)->GetByteArrayElements(env, out, NULL);
    if (!out_bytes) {
        EVP_MD_CTX_free(ctx);
        sun_throw(env, ILLEGAL_STATE_EXCEPTION, "Failed to access output array");
        return;
    }

    unsigned int digest_len = 0;
    if (!EVP_DigestFinal_ex(ctx, (unsigned char*)(out_bytes + outOffset), &digest_len)) {
        (*env)->ReleaseByteArrayElements(env, out, out_bytes, JNI_ABORT);
        EVP_MD_CTX_free(ctx);
        sun_throw(env, ILLEGAL_STATE_EXCEPTION, "SM3 finalization failed");
        return;
    }

    if (digest_len != SM3_DIGEST_LENGTH) {
        (*env)->ReleaseByteArrayElements(env, out, out_bytes, JNI_ABORT);
        EVP_MD_CTX_free(ctx);
        sun_throw(env, ILLEGAL_STATE_EXCEPTION, "Invalid SM3 digest length");
        return;
    }

    (*env)->ReleaseByteArrayElements(env, out, out_bytes, 0);

    EVP_MD_CTX_free(ctx);
}
