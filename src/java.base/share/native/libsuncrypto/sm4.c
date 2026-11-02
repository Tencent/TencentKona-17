/*
 * Copyright (C) 2026, Tencent. All rights reserved.
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
#include <stdlib.h>
#include <string.h>
#include <openssl/crypto.h>
#include <openssl/evp.h>

/* Keep in sync with com.sun.crypto.provider.NativeSM4. */
#define SM4_MODE_ECB 1
#define SM4_MODE_CBC 2
#define SM4_MODE_CTR 3

#define SM4_BLOCK_SIZE 16

#define ILLEGAL_STATE_EXCEPTION      "java/lang/IllegalStateException"
#define ILLEGAL_BLOCK_SIZE_EXCEPTION "javax/crypto/IllegalBlockSizeException"
#define BAD_PADDING_EXCEPTION        "javax/crypto/BadPaddingException"
#define AEAD_BAD_TAG_EXCEPTION       "javax/crypto/AEADBadTagException"
#define OUT_OF_MEMORY_ERROR          "java/lang/OutOfMemoryError"

/*
 * State carried across an SM4 cipher operation. The original key/iv are kept
 * so the context can be re-initialized to its post-init state after a
 * doFinal, matching the JCE contract that a Cipher may be reused for another
 * message without an explicit re-init.
 */
typedef struct {
    EVP_CIPHER_CTX *ctx;
    EVP_CIPHER     *fetched;   /* non-NULL only for GCM; must be freed */
    int             mode;      /* SM4_MODE_*; 0 for GCM */
    int             encrypt;
    int             padding;
    int             ivlen;     /* 0 for ECB */
    unsigned char   key[SM4_BLOCK_SIZE];
    unsigned char   iv[SM4_BLOCK_SIZE];
} SM4State;

static void sm4_throw(JNIEnv *env, const char *exceptionName, const char *message) {
    jclass clazz = (*env)->FindClass(env, exceptionName);
    if (clazz != NULL) {
        (*env)->ThrowNew(env, clazz, message);
    }
}

/*
 * Release an SM4State on any path (success or failure). Cleanses key/iv with
 * OPENSSL_cleanse -- which the compiler cannot optimize away, unlike memset --
 * so freed heap chunks never leak SM4 key material. Safe to call at any
 * partial-init state: NULL fields are simply skipped.
 */
static void sm4_state_free(SM4State *state) {
    if (state == NULL) {
        return;
    }
    if (state->ctx != NULL) {
        EVP_CIPHER_CTX_free(state->ctx);
    }
    if (state->fetched != NULL) {
        EVP_CIPHER_free(state->fetched);
    }
    OPENSSL_cleanse(state->key, sizeof(state->key));
    OPENSSL_cleanse(state->iv, sizeof(state->iv));
    free(state);
}

/*
 * Defense-in-depth bounds checks. The Java layer computes the upper bound in
 * nativeUpdate/nativeDoFinal, but a bug (integer overflow, miscounted state)
 * could otherwise let EVP write past the byte[]. Refuse the call rather than
 * corrupt the heap. A failure here indicates a bug in this provider, not a
 * user error, so IllegalStateException is used.
 */
static int sm4_check_in(JNIEnv *env, jbyteArray in, jint inOfs, jint inLen) {
    if (in == NULL) {
        sm4_throw(env, ILLEGAL_STATE_EXCEPTION, "Input buffer is null");
        return 0;
    }
    if (inOfs < 0 || inLen < 0) {
        sm4_throw(env, ILLEGAL_STATE_EXCEPTION,
                  "Invalid input offset or length");
        return 0;
    }
    jsize alen = (*env)->GetArrayLength(env, in);
    if (inOfs > alen || inLen > alen - inOfs) {
        sm4_throw(env, ILLEGAL_STATE_EXCEPTION,
                  "Input offset+length out of bounds");
        return 0;
    }
    return 1;
}

static int sm4_check_out(JNIEnv *env, jbyteArray out, jint outOfs, jint required) {
    if (out == NULL) {
        sm4_throw(env, ILLEGAL_STATE_EXCEPTION, "Output buffer is null");
        return 0;
    }
    if (outOfs < 0 || required < 0) {
        sm4_throw(env, ILLEGAL_STATE_EXCEPTION,
                  "Invalid output offset or required length");
        return 0;
    }
    jsize outLen = (*env)->GetArrayLength(env, out);
    if (outOfs > outLen || required > outLen - outOfs) {
        sm4_throw(env, ILLEGAL_STATE_EXCEPTION,
                  "Output buffer too small for native cipher operation");
        return 0;
    }
    return 1;
}

static const EVP_CIPHER *sm4_cipher_for_mode(int mode) {
    switch (mode) {
        case SM4_MODE_ECB: return EVP_sm4_ecb();
        case SM4_MODE_CBC: return EVP_sm4_cbc();
        case SM4_MODE_CTR: return EVP_sm4_ctr();
        default:           return NULL;
    }
}

/* ----------------------------------------------------------------------- */
/* Block/stream cipher: ECB / CBC / CTR                                    */
/* ----------------------------------------------------------------------- */

JNIEXPORT jlong JNICALL
Java_com_sun_crypto_provider_NativeSM4_cipherInit(
        JNIEnv *env, jclass clazz, jint mode, jboolean encrypt,
        jbyteArray key, jbyteArray iv, jboolean padding) {
    const EVP_CIPHER *cipher = sm4_cipher_for_mode(mode);
    if (cipher == NULL) {
        sm4_throw(env, ILLEGAL_STATE_EXCEPTION, "Unsupported SM4 mode");
        return 0;
    }

    if (key == NULL || (*env)->GetArrayLength(env, key) != SM4_BLOCK_SIZE) {
        sm4_throw(env, ILLEGAL_STATE_EXCEPTION, "SM4 key must be 16 bytes");
        return 0;
    }

    SM4State *state = (SM4State *) calloc(1, sizeof(SM4State));
    if (state == NULL) {
        sm4_throw(env, OUT_OF_MEMORY_ERROR, "Failed to allocate SM4 state");
        return 0;
    }
    state->mode = mode;
    state->encrypt = encrypt ? 1 : 0;
    state->padding = padding ? 1 : 0;

    (*env)->GetByteArrayRegion(env, key, 0, SM4_BLOCK_SIZE, (jbyte *) state->key);
    if (iv != NULL) {
        jsize ivlen = (*env)->GetArrayLength(env, iv);
        if (ivlen != SM4_BLOCK_SIZE) {
            sm4_state_free(state);
            sm4_throw(env, ILLEGAL_STATE_EXCEPTION, "SM4 IV must be 16 bytes");
            return 0;
        }
        (*env)->GetByteArrayRegion(env, iv, 0, SM4_BLOCK_SIZE, (jbyte *) state->iv);
        state->ivlen = SM4_BLOCK_SIZE;
    }

    state->ctx = EVP_CIPHER_CTX_new();
    if (state->ctx == NULL) {
        sm4_state_free(state);
        sm4_throw(env, ILLEGAL_STATE_EXCEPTION, "Failed to create cipher context");
        return 0;
    }

    if (1 != EVP_CipherInit_ex(state->ctx, cipher, NULL, state->key,
                               state->ivlen ? state->iv : NULL, state->encrypt)) {
        sm4_state_free(state);
        sm4_throw(env, ILLEGAL_STATE_EXCEPTION, "SM4 cipher init failed");
        return 0;
    }

    EVP_CIPHER_CTX_set_padding(state->ctx, state->padding);

    return (jlong) (intptr_t) state;
}

JNIEXPORT jint JNICALL
Java_com_sun_crypto_provider_NativeSM4_cipherUpdate(
        JNIEnv *env, jclass clazz, jlong ctxHandle,
        jbyteArray in, jint inOfs, jint inLen,
        jbyteArray out, jint outOfs) {
    SM4State *state = (SM4State *) (intptr_t) ctxHandle;
    if (state == NULL) {
        sm4_throw(env, ILLEGAL_STATE_EXCEPTION, "Cipher not initialized");
        return 0;
    }
    if (inLen == 0) {
        return 0;
    }
    if (!sm4_check_in(env, in, inOfs, inLen)) {
        return 0;
    }
    /* Upper bound for EVP_CipherUpdate output: for a block cipher, up to
     * inLen + block_size bytes may be emitted (flushing internally buffered
     * partial-block state); CTR is stream mode, so output equals inLen.
     * Guard the addition explicitly -- signed integer overflow in C is UB,
     * not merely "wraps to negative", so relying on sm4_check_out to catch
     * a negative required is not enough. */
    jint requiredOut;
    if (state->mode == SM4_MODE_CTR) {
        requiredOut = inLen;
    } else {
        if (inLen > INT32_MAX - SM4_BLOCK_SIZE) {
            sm4_throw(env, ILLEGAL_STATE_EXCEPTION,
                      "Input too large for native cipher operation");
            return 0;
        }
        requiredOut = inLen + SM4_BLOCK_SIZE;
    }
    if (!sm4_check_out(env, out, outOfs, requiredOut)) {
        return 0;
    }

    jbyte *inBytes = (*env)->GetByteArrayElements(env, in, NULL);
    if (inBytes == NULL) {
        return 0; /* OOME already pending */
    }
    jbyte *outBytes = (*env)->GetByteArrayElements(env, out, NULL);
    if (outBytes == NULL) {
        (*env)->ReleaseByteArrayElements(env, in, inBytes, JNI_ABORT);
        return 0;
    }

    int outl = 0;
    int ok = EVP_CipherUpdate(state->ctx,
                              (unsigned char *) (outBytes + outOfs), &outl,
                              (unsigned char *) (inBytes + inOfs), inLen);

    (*env)->ReleaseByteArrayElements(env, in, inBytes, JNI_ABORT);
    (*env)->ReleaseByteArrayElements(env, out, outBytes, ok ? 0 : JNI_ABORT);

    if (!ok) {
        sm4_throw(env, ILLEGAL_STATE_EXCEPTION, "SM4 update failed");
        return 0;
    }
    return outl;
}

JNIEXPORT jint JNICALL
Java_com_sun_crypto_provider_NativeSM4_cipherDoFinal(
        JNIEnv *env, jclass clazz, jlong ctxHandle,
        jbyteArray out, jint outOfs) {
    SM4State *state = (SM4State *) (intptr_t) ctxHandle;
    if (state == NULL) {
        sm4_throw(env, ILLEGAL_STATE_EXCEPTION, "Cipher not initialized");
        return 0;
    }
    /* Block modes may emit one final padding block; CTR emits nothing. */
    jint requiredOut = (state->mode == SM4_MODE_CTR) ? 0 : SM4_BLOCK_SIZE;
    if (!sm4_check_out(env, out, outOfs, requiredOut)) {
        return 0;
    }

    jbyte *outBytes = (*env)->GetByteArrayElements(env, out, NULL);
    if (outBytes == NULL) {
        return 0;
    }

    int outl = 0;
    int ok = EVP_CipherFinal_ex(state->ctx,
                                (unsigned char *) (outBytes + outOfs), &outl);

    (*env)->ReleaseByteArrayElements(env, out, outBytes, ok ? 0 : JNI_ABORT);

    if (!ok) {
        /*
         * EVP does not tell us the precise reason. Map it the way the JCE
         * would: an encrypt final can only fail here because the data is not
         * block aligned (NoPadding); a decrypt final fails on bad padding
         * (when padding is enabled) or misalignment otherwise.
         */
        if (state->encrypt || !state->padding) {
            sm4_throw(env, ILLEGAL_BLOCK_SIZE_EXCEPTION,
                      "Input length not multiple of 16 bytes");
        } else {
            sm4_throw(env, BAD_PADDING_EXCEPTION, "Given final block not "
                      "properly padded");
        }
        return 0;
    }

    /* Reset the context to its post-init state so the cipher can be reused
     * for another message without an explicit re-init (JCE semantics). */
    EVP_CipherInit_ex(state->ctx, NULL, NULL, NULL,
                      state->ivlen ? state->iv : NULL, -1);
    EVP_CIPHER_CTX_set_padding(state->ctx, state->padding);

    return outl;
}

JNIEXPORT void JNICALL
Java_com_sun_crypto_provider_NativeSM4_cipherFree(
        JNIEnv *env, jclass clazz, jlong ctxHandle) {
    sm4_state_free((SM4State *) (intptr_t) ctxHandle);
}

/*
 * Save the full cipher context (EVP internal state + key/iv metadata) into a
 * fresh SM4State and return its handle. Used to preserve retry semantics for
 * decrypt+padding doFinal, where the actual plaintext length is only known
 * after finalization.
 */
JNIEXPORT jlong JNICALL
Java_com_sun_crypto_provider_NativeSM4_cipherSave(
        JNIEnv *env, jclass clazz, jlong ctxHandle) {
    SM4State *state = (SM4State *) (intptr_t) ctxHandle;
    if (state == NULL || state->ctx == NULL) {
        sm4_throw(env, ILLEGAL_STATE_EXCEPTION, "Cipher not initialized");
        return 0;
    }
    SM4State *saved = (SM4State *) calloc(1, sizeof(SM4State));
    if (saved == NULL) {
        sm4_throw(env, OUT_OF_MEMORY_ERROR, "Failed to allocate SM4 state");
        return 0;
    }
    saved->ctx = EVP_CIPHER_CTX_new();
    if (saved->ctx == NULL) {
        sm4_state_free(saved);
        sm4_throw(env, ILLEGAL_STATE_EXCEPTION, "Failed to create cipher context");
        return 0;
    }
    if (1 != EVP_CIPHER_CTX_copy(saved->ctx, state->ctx)) {
        sm4_state_free(saved);
        sm4_throw(env, ILLEGAL_STATE_EXCEPTION, "Failed to copy cipher context");
        return 0;
    }
    saved->mode = state->mode;
    saved->encrypt = state->encrypt;
    saved->padding = state->padding;
    saved->ivlen = state->ivlen;
    memcpy(saved->key, state->key, sizeof(saved->key));
    memcpy(saved->iv, state->iv, sizeof(saved->iv));
    return (jlong) (intptr_t) saved;
}

/*
 * Restore state from `savedHandle` back into `ctxHandle`, then release the
 * saved state. After this call, ctxHandle is functionally identical to what
 * it was at the moment of the matching cipherSave() call.
 *
 * Ownership: on success, this function consumes and releases savedHandle.
 * On failure (exception thrown) the saved state is left intact and the
 * caller MUST release it via cipherFreeSaved to avoid a leak. This split is
 * what lets the Java side safely fall back to cipherFreeSaved in its
 * finally block without risking a double-free.
 */
JNIEXPORT void JNICALL
Java_com_sun_crypto_provider_NativeSM4_cipherRestore(
        JNIEnv *env, jclass clazz, jlong ctxHandle, jlong savedHandle) {
    SM4State *state = (SM4State *) (intptr_t) ctxHandle;
    SM4State *saved = (SM4State *) (intptr_t) savedHandle;
    if (state == NULL || state->ctx == NULL
            || saved == NULL || saved->ctx == NULL) {
        sm4_throw(env, ILLEGAL_STATE_EXCEPTION, "Invalid state for restore");
        return;
    }
    /* EVP_CIPHER_CTX_copy resets `state->ctx` before copying, so the
     * post-final state is discarded and replaced with the saved state. */
    if (1 != EVP_CIPHER_CTX_copy(state->ctx, saved->ctx)) {
        sm4_throw(env, ILLEGAL_STATE_EXCEPTION, "Failed to restore cipher context");
        /* Leave savedHandle owned by the caller; do not free here or Java's
         * finally-block cipherFreeSaved would double-free. */
        return;
    }
    sm4_state_free(saved);
}

/*
 * Release a saved state allocated by cipherSave() without touching any live
 * cipher context. Used when the operation succeeded and the saved copy is no
 * longer needed. Safe to call with 0.
 */
JNIEXPORT void JNICALL
Java_com_sun_crypto_provider_NativeSM4_cipherFreeSaved(
        JNIEnv *env, jclass clazz, jlong savedHandle) {
    sm4_state_free((SM4State *) (intptr_t) savedHandle);
}

/* ----------------------------------------------------------------------- */
/* GCM (SM4-GCM, OpenSSL 3.0+)                                             */
/* ----------------------------------------------------------------------- */

/* Reports whether this OpenSSL provides the SM4-GCM cipher, so the provider
 * can keep the pure-Java GaloisCounterMode fallback when it does not. */
JNIEXPORT jboolean JNICALL
Java_com_sun_crypto_provider_NativeSM4_sm4GcmAvailable(
        JNIEnv *env, jclass clazz) {
    EVP_CIPHER *cipher = EVP_CIPHER_fetch(NULL, "SM4-GCM", NULL);
    if (cipher == NULL) {
        return JNI_FALSE;
    }
    EVP_CIPHER_free(cipher);
    return JNI_TRUE;
}

JNIEXPORT jlong JNICALL
Java_com_sun_crypto_provider_NativeSM4_gcmInit(
        JNIEnv *env, jclass clazz, jboolean encrypt,
        jbyteArray key, jbyteArray iv) {
    if (key == NULL || (*env)->GetArrayLength(env, key) != SM4_BLOCK_SIZE) {
        sm4_throw(env, ILLEGAL_STATE_EXCEPTION, "SM4 key must be 16 bytes");
        return 0;
    }
    if (iv == NULL) {
        sm4_throw(env, ILLEGAL_STATE_EXCEPTION, "SM4-GCM requires an IV");
        return 0;
    }
    jsize ivlen = (*env)->GetArrayLength(env, iv);

    SM4State *state = (SM4State *) calloc(1, sizeof(SM4State));
    if (state == NULL) {
        sm4_throw(env, OUT_OF_MEMORY_ERROR, "Failed to allocate SM4 state");
        return 0;
    }
    state->mode = 0;
    state->encrypt = encrypt ? 1 : 0;

    state->fetched = EVP_CIPHER_fetch(NULL, "SM4-GCM", NULL);
    if (state->fetched == NULL) {
        sm4_state_free(state);
        sm4_throw(env, ILLEGAL_STATE_EXCEPTION, "SM4-GCM not available in OpenSSL");
        return 0;
    }

    state->ctx = EVP_CIPHER_CTX_new();
    if (state->ctx == NULL) {
        sm4_state_free(state);
        sm4_throw(env, ILLEGAL_STATE_EXCEPTION, "Failed to create cipher context");
        return 0;
    }

    (*env)->GetByteArrayRegion(env, key, 0, SM4_BLOCK_SIZE, (jbyte *) state->key);

    jbyte *ivBytes = (*env)->GetByteArrayElements(env, iv, NULL);
    if (ivBytes == NULL) {
        sm4_state_free(state);
        return 0;
    }

    int ok = EVP_CipherInit_ex(state->ctx, state->fetched, NULL, NULL, NULL, state->encrypt)
             && EVP_CIPHER_CTX_ctrl(state->ctx, EVP_CTRL_AEAD_SET_IVLEN, (int) ivlen, NULL)
             && EVP_CipherInit_ex(state->ctx, NULL, NULL, state->key,
                                  (unsigned char *) ivBytes, state->encrypt);

    (*env)->ReleaseByteArrayElements(env, iv, ivBytes, JNI_ABORT);

    if (!ok) {
        sm4_state_free(state);
        sm4_throw(env, ILLEGAL_STATE_EXCEPTION, "SM4-GCM init failed");
        return 0;
    }

    return (jlong) (intptr_t) state;
}

JNIEXPORT void JNICALL
Java_com_sun_crypto_provider_NativeSM4_gcmUpdateAAD(
        JNIEnv *env, jclass clazz, jlong ctxHandle,
        jbyteArray aad, jint ofs, jint len) {
    SM4State *state = (SM4State *) (intptr_t) ctxHandle;
    if (state == NULL || len == 0) {
        return;
    }
    if (!sm4_check_in(env, aad, ofs, len)) {
        return;
    }
    jbyte *aadBytes = (*env)->GetByteArrayElements(env, aad, NULL);
    if (aadBytes == NULL) {
        return;
    }
    int outl = 0;
    int ok = EVP_CipherUpdate(state->ctx, NULL, &outl,
                              (unsigned char *) (aadBytes + ofs), len);
    (*env)->ReleaseByteArrayElements(env, aad, aadBytes, JNI_ABORT);
    if (!ok) {
        sm4_throw(env, ILLEGAL_STATE_EXCEPTION, "SM4-GCM AAD update failed");
    }
}

JNIEXPORT jint JNICALL
Java_com_sun_crypto_provider_NativeSM4_gcmUpdate(
        JNIEnv *env, jclass clazz, jlong ctxHandle,
        jbyteArray in, jint inOfs, jint inLen,
        jbyteArray out, jint outOfs) {
    SM4State *state = (SM4State *) (intptr_t) ctxHandle;
    if (state == NULL) {
        sm4_throw(env, ILLEGAL_STATE_EXCEPTION, "Cipher not initialized");
        return 0;
    }
    if (inLen == 0) {
        return 0;
    }
    if (!sm4_check_in(env, in, inOfs, inLen)) {
        return 0;
    }
    /* GCM is a stream cipher: EVP_CipherUpdate writes exactly inLen bytes. */
    if (!sm4_check_out(env, out, outOfs, inLen)) {
        return 0;
    }
    jbyte *inBytes = (*env)->GetByteArrayElements(env, in, NULL);
    if (inBytes == NULL) {
        return 0;
    }
    jbyte *outBytes = (*env)->GetByteArrayElements(env, out, NULL);
    if (outBytes == NULL) {
        (*env)->ReleaseByteArrayElements(env, in, inBytes, JNI_ABORT);
        return 0;
    }
    int outl = 0;
    int ok = EVP_CipherUpdate(state->ctx,
                              (unsigned char *) (outBytes + outOfs), &outl,
                              (unsigned char *) (inBytes + inOfs), inLen);
    (*env)->ReleaseByteArrayElements(env, in, inBytes, JNI_ABORT);
    (*env)->ReleaseByteArrayElements(env, out, outBytes, ok ? 0 : JNI_ABORT);
    if (!ok) {
        sm4_throw(env, ILLEGAL_STATE_EXCEPTION, "SM4-GCM update failed");
        return 0;
    }
    return outl;
}

JNIEXPORT jint JNICALL
Java_com_sun_crypto_provider_NativeSM4_gcmEncryptDoFinal(
        JNIEnv *env, jclass clazz, jlong ctxHandle,
        jbyteArray out, jint outOfs, jint tagLen) {
    SM4State *state = (SM4State *) (intptr_t) ctxHandle;
    if (state == NULL) {
        sm4_throw(env, ILLEGAL_STATE_EXCEPTION, "Cipher not initialized");
        return 0;
    }
    if (tagLen < 0 || tagLen > SM4_BLOCK_SIZE) {
        sm4_throw(env, ILLEGAL_STATE_EXCEPTION, "Invalid GCM tag length");
        return 0;
    }
    /* For GCM, EVP_EncryptFinal_ex emits no ciphertext; the caller-visible
     * output at outOfs is just the tag we memcpy afterwards. */
    if (!sm4_check_out(env, out, outOfs, tagLen)) {
        return 0;
    }
    jbyte *outBytes = (*env)->GetByteArrayElements(env, out, NULL);
    if (outBytes == NULL) {
        return 0;
    }
    /* Route EVP's final output into a local scratch buffer rather than into
     * the Java heap. A misbehaving EVP that reports a non-zero finalLen
     * would otherwise have already written into the tag slot on pinned
     * arrays before we get a chance to reject the result. */
    unsigned char finalBlock[SM4_BLOCK_SIZE];
    int finalLen = 0;
    unsigned char tag[SM4_BLOCK_SIZE];
    int ok = EVP_EncryptFinal_ex(state->ctx, finalBlock, &finalLen)
             && EVP_CIPHER_CTX_ctrl(state->ctx, EVP_CTRL_AEAD_GET_TAG, tagLen, tag);
    if (ok) {
        /* GCM produces no plaintext-side final block. */
        if (finalLen != 0) {
            (*env)->ReleaseByteArrayElements(env, out, outBytes, JNI_ABORT);
            sm4_throw(env, ILLEGAL_STATE_EXCEPTION,
                      "GCM final produced unexpected ciphertext");
            return 0;
        }
        memcpy(outBytes + outOfs, tag, tagLen);
    }
    (*env)->ReleaseByteArrayElements(env, out, outBytes, ok ? 0 : JNI_ABORT);
    if (!ok) {
        sm4_throw(env, ILLEGAL_STATE_EXCEPTION, "SM4-GCM encrypt final failed");
        return 0;
    }
    return finalLen + tagLen;
}

JNIEXPORT jint JNICALL
Java_com_sun_crypto_provider_NativeSM4_gcmDecryptDoFinal(
        JNIEnv *env, jclass clazz, jlong ctxHandle,
        jbyteArray out, jint outOfs,
        jbyteArray tag, jint tagOfs, jint tagLen) {
    SM4State *state = (SM4State *) (intptr_t) ctxHandle;
    if (state == NULL) {
        sm4_throw(env, ILLEGAL_STATE_EXCEPTION, "Cipher not initialized");
        return 0;
    }
    if (tagLen < 0 || tagLen > SM4_BLOCK_SIZE) {
        sm4_throw(env, ILLEGAL_STATE_EXCEPTION, "Invalid GCM tag length");
        return 0;
    }
    if (!sm4_check_in(env, tag, tagOfs, tagLen)) {
        return 0;
    }
    /* GCM decrypt final emits no plaintext; validate outOfs is still within
     * the output buffer even though nothing will be written. */
    if (!sm4_check_out(env, out, outOfs, 0)) {
        return 0;
    }
    jbyte *tagBytes = (*env)->GetByteArrayElements(env, tag, NULL);
    if (tagBytes == NULL) {
        return 0;
    }
    if (1 != EVP_CIPHER_CTX_ctrl(state->ctx, EVP_CTRL_AEAD_SET_TAG, tagLen,
                                 (unsigned char *) (tagBytes + tagOfs))) {
        (*env)->ReleaseByteArrayElements(env, tag, tagBytes, JNI_ABORT);
        sm4_throw(env, ILLEGAL_STATE_EXCEPTION, "Failed to set GCM tag");
        return 0;
    }
    (*env)->ReleaseByteArrayElements(env, tag, tagBytes, JNI_ABORT);

    /* Route EVP's final output into a local scratch buffer -- GCM decrypt
     * final must not write plaintext, but we refuse to hand it a pointer
     * into the Java heap regardless, so a misbehaving EVP cannot corrupt
     * pinned bytes before we notice. */
    unsigned char finalBlock[SM4_BLOCK_SIZE];
    int finalLen = 0;
    if (!EVP_DecryptFinal_ex(state->ctx, finalBlock, &finalLen)) {
        sm4_throw(env, AEAD_BAD_TAG_EXCEPTION, "Tag mismatch");
        return 0;
    }
    if (finalLen != 0) {
        sm4_throw(env, ILLEGAL_STATE_EXCEPTION,
                  "GCM final produced unexpected plaintext");
        return 0;
    }
    return finalLen;
}

JNIEXPORT void JNICALL
Java_com_sun_crypto_provider_NativeSM4_gcmFree(
        JNIEnv *env, jclass clazz, jlong ctxHandle) {
    Java_com_sun_crypto_provider_NativeSM4_cipherFree(env, clazz, ctxHandle);
}
