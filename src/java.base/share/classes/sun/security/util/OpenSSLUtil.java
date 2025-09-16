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

package sun.security.util;

import sun.security.action.GetPropertyAction;

import java.security.AccessController;
import java.security.PrivilegedAction;

public class OpenSSLUtil {

    private static final boolean IS_OPENSSL_LOADED;

    static {
        IS_OPENSSL_LOADED = loadOpenSSLCryptoLib();
    }

    // Load lib opensslcrypto
    @SuppressWarnings("removal")
    private static boolean loadOpenSSLCryptoLib() {
        // The absolute path to OpenSSL libcrypto file
        String opensslCryptoLibPath = GetPropertyAction.privilegedGetProperty(
                "jdk.openssl.cryptoLibPath");

        boolean loaded = true;
        try {
            AccessController.doPrivileged((PrivilegedAction<Void>) () -> {
                if (opensslCryptoLibPath == null) {
                    System.loadLibrary("opensslcrypto");
                } else {
                    System.load(opensslCryptoLibPath);
                }
                return null;
            });
        } catch (UnsatisfiedLinkError e) {
            System.err.println("Failed to load OpenSSL libcrypto: " + e);
            loaded = false;
        }
        return loaded;
    }

    public static boolean isOpenSSLLoaded() {
        return IS_OPENSSL_LOADED;
    }
}
