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

#include <stdint.h>
#include <openssl/evp.h>

#define SM3_DIGEST_LENGTH 32

#define NULL_POINTER_EXCEPTION              "java/lang/NullPointerException"
#define ARRAY_INDEX_OUT_OF_BOUNDS_EXCEPTION "java/lang/ArrayIndexOutOfBoundsException"
#define ILLEGAL_STATE_EXCEPTION             "java/lang/IllegalStateException"
#define OUT_OF_MEMORY_ERROR                 "java/lang/OutOfMemoryError"

static void sun_throw(JNIEnv *env, const char *exceptionName, const char *message) {
    jclass clazz = (*env)->FindClass(env, exceptionName);
    if (clazz != NULL) {
        (*env)->ThrowNew(env, clazz, message);
    }
}

/* ----------------------------------------------------------------------- */
/* SM3 streaming interface                                                   */
/* ----------------------------------------------------------------------- */

/*
 * Allocates a new EVP_MD_CTX, initialises it for SM3, and returns an opaque
 * pointer cast to jlong. The caller owns the context and must eventually pass
 * it to sm3Free.
 */
JNIEXPORT jlong JNICALL
Java_sun_security_provider_NativeSun_sm3Init(JNIEnv *env, jclass clazz) {
    const EVP_MD *md = EVP_sm3();
    if (md == NULL) {
        sun_throw(env, ILLEGAL_STATE_EXCEPTION, "SM3 not available in OpenSSL");
        return 0;
    }
    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    if (ctx == NULL) {
        sun_throw(env, OUT_OF_MEMORY_ERROR, "Failed to allocate SM3 context");
        return 0;
    }
    if (1 != EVP_DigestInit_ex(ctx, md, NULL)) {
        EVP_MD_CTX_free(ctx);
        sun_throw(env, ILLEGAL_STATE_EXCEPTION, "SM3 init failed");
        return 0;
    }
    return (jlong)(intptr_t)ctx;
}

JNIEXPORT void JNICALL
Java_sun_security_provider_NativeSun_sm3Update(
        JNIEnv *env, jclass clazz, jlong ctxHandle,
        jbyteArray in, jint inOfs, jint inLen) {
    EVP_MD_CTX *ctx = (EVP_MD_CTX *)(intptr_t)ctxHandle;
    if (ctx == NULL) {
        sun_throw(env, ILLEGAL_STATE_EXCEPTION, "SM3 context is null");
        return;
    }
    if (in == NULL) {
        sun_throw(env, NULL_POINTER_EXCEPTION, "Input array is null");
        return;
    }
    if (inLen == 0) {
        return;
    }
    if (inOfs < 0 || inLen < 0) {
        sun_throw(env, ARRAY_INDEX_OUT_OF_BOUNDS_EXCEPTION,
                  "Invalid input offset or length");
        return;
    }
    jsize alen = (*env)->GetArrayLength(env, in);
    if (inLen > alen - inOfs) {
        sun_throw(env, ARRAY_INDEX_OUT_OF_BOUNDS_EXCEPTION,
                  "Input offset+length exceeds array length");
        return;
    }
    jbyte *inBytes = (*env)->GetByteArrayElements(env, in, NULL);
    if (inBytes == NULL) {
        return; /* OOME already pending */
    }
    int ok = EVP_DigestUpdate(ctx, (unsigned char *)(inBytes + inOfs),
                              (size_t)inLen);
    (*env)->ReleaseByteArrayElements(env, in, inBytes, JNI_ABORT);
    if (!ok) {
        sun_throw(env, ILLEGAL_STATE_EXCEPTION, "SM3 update failed");
    }
}

/*
 * Writes the 32-byte SM3 digest into out[outOfs..outOfs+31] and reinitialises
 * the context so it can be reused for another message without a new sm3Init.
 */
JNIEXPORT void JNICALL
Java_sun_security_provider_NativeSun_sm3DoFinal(
        JNIEnv *env, jclass clazz, jlong ctxHandle,
        jbyteArray out, jint outOfs) {
    EVP_MD_CTX *ctx = (EVP_MD_CTX *)(intptr_t)ctxHandle;
    if (ctx == NULL) {
        sun_throw(env, ILLEGAL_STATE_EXCEPTION, "SM3 context is null");
        return;
    }
    if (out == NULL) {
        sun_throw(env, NULL_POINTER_EXCEPTION, "Output array is null");
        return;
    }
    if (outOfs < 0) {
        sun_throw(env, ARRAY_INDEX_OUT_OF_BOUNDS_EXCEPTION,
                  "Output offset is negative");
        return;
    }
    jsize outLen = (*env)->GetArrayLength(env, out);
    if (outOfs > outLen - SM3_DIGEST_LENGTH) {
        sun_throw(env, ARRAY_INDEX_OUT_OF_BOUNDS_EXCEPTION,
                  "Output buffer too small for SM3 digest");
        return;
    }
    jbyte *outBytes = (*env)->GetByteArrayElements(env, out, NULL);
    if (outBytes == NULL) {
        return;
    }
    unsigned int digestLen = 0;
    int ok = EVP_DigestFinal_ex(ctx,
                                (unsigned char *)(outBytes + outOfs), &digestLen);
    (*env)->ReleaseByteArrayElements(env, out, outBytes, ok ? 0 : JNI_ABORT);
    if (!ok) {
        sun_throw(env, ILLEGAL_STATE_EXCEPTION, "SM3 final failed");
        return;
    }
    if (digestLen != SM3_DIGEST_LENGTH) {
        sun_throw(env, ILLEGAL_STATE_EXCEPTION,
                  "SM3 produced unexpected digest length");
        /* Best-effort reinit: the primary exception is already pending, so a
         * failure here is silently ignored rather than overwriting it with a
         * secondary exception. Use EVP_sm3() explicitly to maximise the chance
         * of success after an abnormal finalisation. */
        EVP_DigestInit_ex(ctx, EVP_sm3(), NULL);
        return;
    }
    /* Reinitialise for reuse. EVP_DigestInit_ex with a NULL type argument
     * reuses the current digest (SM3), equivalent to a fresh sm3Init on the
     * same already-allocated context. */
    if (1 != EVP_DigestInit_ex(ctx, NULL, NULL)) {
        sun_throw(env, ILLEGAL_STATE_EXCEPTION, "SM3 context reinit failed");
    }
}

/*
 * Reinitialises ctx to its post-init state without producing output. Used by
 * SM3EngineNative.reset() to abort a partially-computed hash.
 */
JNIEXPORT void JNICALL
Java_sun_security_provider_NativeSun_sm3Reinit(
        JNIEnv *env, jclass clazz, jlong ctxHandle) {
    EVP_MD_CTX *ctx = (EVP_MD_CTX *)(intptr_t)ctxHandle;
    if (ctx == NULL) {
        return;
    }
    if (1 != EVP_DigestInit_ex(ctx, NULL, NULL)) {
        sun_throw(env, ILLEGAL_STATE_EXCEPTION, "SM3 context reinit failed");
    }
}

/*
 * Duplicates the state of src into a fresh EVP_MD_CTX and returns the new
 * handle. Used by SM3EngineNative.clone() to snapshot mid-computation state.
 */
JNIEXPORT jlong JNICALL
Java_sun_security_provider_NativeSun_sm3Copy(
        JNIEnv *env, jclass clazz, jlong ctxHandle) {
    EVP_MD_CTX *src = (EVP_MD_CTX *)(intptr_t)ctxHandle;
    if (src == NULL) {
        sun_throw(env, ILLEGAL_STATE_EXCEPTION, "Source SM3 context is null");
        return 0;
    }
    EVP_MD_CTX *dst = EVP_MD_CTX_new();
    if (dst == NULL) {
        sun_throw(env, OUT_OF_MEMORY_ERROR, "Failed to allocate SM3 context");
        return 0;
    }
    if (1 != EVP_MD_CTX_copy_ex(dst, src)) {
        EVP_MD_CTX_free(dst);
        sun_throw(env, ILLEGAL_STATE_EXCEPTION, "Failed to copy SM3 context");
        return 0;
    }
    return (jlong)(intptr_t)dst;
}

/* Releases the EVP_MD_CTX. Safe to call with 0. */
JNIEXPORT void JNICALL
Java_sun_security_provider_NativeSun_sm3Free(
        JNIEnv *env, jclass clazz, jlong ctxHandle) {
    EVP_MD_CTX *ctx = (EVP_MD_CTX *)(intptr_t)ctxHandle;
    if (ctx != NULL) {
        EVP_MD_CTX_free(ctx);
    }
}
