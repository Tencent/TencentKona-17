/*
 * Copyright (C) 2025, Tencent. All rights reserved.
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

package sun.security.provider;

import sun.security.action.GetBooleanAction;
import sun.security.util.OpenSSLUtil;

import java.security.AccessController;
import java.security.PrivilegedAction;

/**
 * The native implementation with OpenSSL for Sun algorithms.
 */
final class NativeSun {

    private static final boolean IS_NATIVE_CRYPTO_ENABLED;

    static {
        boolean enableNativeCrypto = GetBooleanAction.privilegedGetProperty(
                "jdk.sun.enableNativeCrypto");
        IS_NATIVE_CRYPTO_ENABLED = enableNativeCrypto
                // OpenSSL crypto lib must be loaded at first
                && OpenSSLUtil.isOpenSSLLoaded()
                && loadSunCryptoLib();
    }

    // Load lib suncrypto
    @SuppressWarnings("removal")
    private static boolean loadSunCryptoLib() {
        boolean loaded = true;
        try {
            AccessController.doPrivileged((PrivilegedAction<Void>) () -> {
                    System.loadLibrary("suncrypto");
                    return null;
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

    public static native void sm3Digest(byte[] message, byte[] out, int outOffset);
}
