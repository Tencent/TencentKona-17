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

import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;

/**
 * Helper that locates the bundled OpenSSL libcrypto and the built-in
 * libsuncrypto so the native SM4 tests can enable OpenSSL acceleration.
 */
public class NativeSM4Util {

    public static boolean nativeCryptoSupported() {
        String opensslCryptoPath = OpenSSLUtil.opensslCryptoPath();
        boolean supported = suncryptoAvailable() && opensslCryptoPath != null;
        if (supported) {
            System.setProperty("jdk.openssl.cryptoLibPath", opensslCryptoPath);
        }
        return supported;
    }

    private static boolean suncryptoAvailable() {
        Path jdkLibDir = Paths.get(System.getProperty("test.jdk")).resolve("lib");
        return Files.exists(jdkLibDir.resolve("libsuncrypto.so"))
                || Files.exists(jdkLibDir.resolve("libsuncrypto.dylib"));
    }
}
