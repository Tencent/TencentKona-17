/*
 * Copyright (C) 2026, Tencent. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation. Tencent designates
 * this particular file as subject to the "Classpath" exception as provided
 * in the LICENSE file that accompanied this code.
 *
 * This code is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License version 2 for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

package com.sun.crypto.provider;

import sun.security.util.OpenSSLUtil;

import java.security.AccessController;
import java.security.PrivilegedAction;

/**
 * The native SM4 implementation backed by OpenSSL's EVP interface.
 *
 * <p>This class only holds the enablement decision and the raw JNI entry
 * points. The pure-Java {@link SM4Engine} / {@link CipherCore} path remains
 * the fallback and is used whenever native crypto is disabled or a
 * mode/padding combination is not supported here.
 *
 * <p>Native acceleration is enabled only when the system property
 * {@code jdk.sunjce.enableNativeCrypto} is {@code true} and an OpenSSL
 * libcrypto has been loaded (see {@link OpenSSLUtil}).
 */
final class NativeSM4 {

    // Cipher mode selectors, must be kept in sync with sm4.c.
    static final int MODE_ECB = 1;
    static final int MODE_CBC = 2;
    static final int MODE_CTR = 3;

    private static final boolean IS_NATIVE_CRYPTO_ENABLED;
    private static final boolean IS_SM4_GCM_AVAILABLE;

    static {
        boolean enableNativeCrypto = Boolean.getBoolean(
                "jdk.sunjce.enableNativeCrypto");
        IS_NATIVE_CRYPTO_ENABLED = enableNativeCrypto
                // OpenSSL crypto lib must be loaded at first
                && OpenSSLUtil.isOpenSSLLoaded()
                && loadSunCryptoLib();
        // SM4-GCM needs OpenSSL 3.0+; probe it so the provider can keep the
        // pure-Java GCM fallback when this libcrypto lacks SM4-GCM.
        IS_SM4_GCM_AVAILABLE = IS_NATIVE_CRYPTO_ENABLED && sm4GcmAvailable();
    }

    // Load lib suncrypto (shared with the native SM3 implementation).
    @SuppressWarnings("removal")
    private static boolean loadSunCryptoLib() {
        boolean loaded = true;
        try {
            AccessController.doPrivileged(
                    new PrivilegedAction<Void>() {
                        @SuppressWarnings("restricted")
                        public Void run() {
                            System.loadLibrary("suncrypto");
                            return null;
                        }
                    });
        } catch (UnsatisfiedLinkError e) {
            System.err.println("Failed to load suncrypto: " + e);
            loaded = false;
        }
        return loaded;
    }

    static boolean isNativeCryptoEnabled() {
        return IS_NATIVE_CRYPTO_ENABLED;
    }

    // True only when native crypto is enabled AND this OpenSSL supports
    // SM4-GCM. The provider must fall back to the Java GCM otherwise.
    static boolean isSM4GCMAvailable() {
        return IS_SM4_GCM_AVAILABLE;
    }

    // Probe for the SM4-GCM cipher (OpenSSL 3.0+). Must only be called once
    // the suncrypto library is loaded.
    private static native boolean sm4GcmAvailable();

    // ---------------------------------------------------------------------
    // Block/stream cipher (ECB / CBC / CTR)
    //
    // cipherInit creates an EVP_CIPHER_CTX and returns an opaque pointer as a
    // jlong. The caller MUST eventually call cipherFree exactly once. The IV
    // may be null for ECB. When padding is false PKCS#7 padding is disabled
    // (NoPadding) and the total input length must be block aligned.
    // ---------------------------------------------------------------------
    static native long cipherInit(int mode, boolean encrypt,
                                  byte[] key, byte[] iv, boolean padding);

    // Processes inLen bytes and writes the produced bytes into out at outOfs.
    // Returns the number of bytes written (may be 0). Throws
    // ShortBufferException-style errors via IllegalStateException if out is
    // too small (the caller is expected to size out via getOutputSize first).
    static native int cipherUpdate(long ctx, byte[] in, int inOfs, int inLen,
                                   byte[] out, int outOfs);

    // Finalizes the operation (applies/strips padding) and writes the trailing
    // bytes into out at outOfs. Returns the number of bytes written.
    static native int cipherDoFinal(long ctx, byte[] out, int outOfs);

    // Releases the EVP_CIPHER_CTX. Safe to call with 0.
    static native void cipherFree(long ctx);

    // Save/restore the cipher context (via EVP_CIPHER_CTX_copy) so
    // decrypt+padding doFinal can throw ShortBufferException without
    // destroying the ability to retry on the same Cipher.
    //
    // Ownership: cipherSave() returns a fresh saved-state handle. The caller
    // MUST eventually release it via exactly one of:
    //   - cipherRestore(): consumes the saved handle ONLY on success (i.e.
    //     when it returns normally). If cipherRestore throws, the saved
    //     handle is left intact and the caller must still release it via
    //     cipherFreeSaved (typically from a finally block).
    //   - cipherFreeSaved(): always releases the handle; safe with 0.
    static native long cipherSave(long ctx);
    static native void cipherRestore(long ctx, long saved);
    static native void cipherFreeSaved(long saved);

    // ---------------------------------------------------------------------
    // GCM (SM4-GCM, requires OpenSSL 3.0+)
    // ---------------------------------------------------------------------
    static native long gcmInit(boolean encrypt, byte[] key, byte[] iv);

    static native void gcmUpdateAAD(long ctx, byte[] aad, int ofs, int len);

    static native int gcmUpdate(long ctx, byte[] in, int inOfs, int inLen,
                                byte[] out, int outOfs);

    // Encrypt: writes the remaining ciphertext then the tag (tagLen bytes)
    // into out starting at outOfs; returns total bytes written.
    static native int gcmEncryptDoFinal(long ctx, byte[] out, int outOfs,
                                        int tagLen);

    // Decrypt: verifies the tag (tagLen bytes read from tag[]) and writes the
    // remaining plaintext into out; returns bytes written. Throws
    // javax.crypto.AEADBadTagException on tag mismatch.
    static native int gcmDecryptDoFinal(long ctx, byte[] out, int outOfs,
                                        byte[] tag, int tagOfs, int tagLen);

    static native void gcmFree(long ctx);

    private NativeSM4() {}
}
