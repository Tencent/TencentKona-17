/*
 * Copyright (C) 2025, THL A29 Limited, a Tencent company. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation. THL A29 Limited designates
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

package sun.security.rsa;

import sun.security.action.GetBooleanAction;
import sun.security.util.OpenSSLUtil;

import java.security.*;

/**
 * The native implementation with OpenSSL for SunRsaSign algorithms.
 */
final class NativeSunRsaSign {

    private static final boolean IS_NATIVE_CRYPTO_ENABLED;

    static {
        boolean enableNativeCrypto = GetBooleanAction.privilegedGetProperty(
                "jdk.sunrsasign.enableNativeCrypto");
        IS_NATIVE_CRYPTO_ENABLED = enableNativeCrypto
                // OpenSSL crypto lib must be loaded at first
                && OpenSSLUtil.isOpenSSLLoaded()
                && loadSunRSASignCryptoLib();
    }

    // Load lib sunrsasigncrypto
    @SuppressWarnings("removal")
    private static boolean loadSunRSASignCryptoLib() {
        boolean loaded = true;
        try {
            AccessController.doPrivileged((PrivilegedAction<Void>) () -> {
                    System.loadLibrary("sunrsasigncrypto");
                    return null;
            });
        } catch (UnsatisfiedLinkError e) {
            System.err.println("Failed to load sunrsasigncrypto: " + e);
            loaded = false;
        }
        return loaded;
    }

    static boolean isNativeCryptoEnabled() {
        return IS_NATIVE_CRYPTO_ENABLED;
    }

    static native void rsaModPow(byte[] base, byte[] exponent, byte[] modulus, byte[] out);
}
