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

/*
 * @test
 * @summary The native RSA is enabled when the system property
 *          jdk.sunrsasign.enableNativeCrypto is true and the OpenSSL libcrypto
 *          is available.
 * @modules java.base/sun.security.rsa
 * @library /test/lib /test/jdk/openssl
 * @build java.base/sun.security.rsa.NativeSunRsaSignWrapper
 * @run main/othervm EnableNativeSunRsaSign
 * @run main/othervm/policy=native.policy EnableNativeSunRsaSign
 * @run main/othervm -Djdk.sunrsasign.enableNativeCrypto=true EnableNativeSunRsaSign
 * @run main/othervm/policy=native.policy -Djdk.sunrsasign.enableNativeCrypto=true EnableNativeSunRsaSign
 */

import sun.security.rsa.NativeSunRsaSignWrapper;

public class EnableNativeSunRsaSign {

    public static void main(String[] args) {
        boolean nativeCryptoSupported = NativeSunRsaSignUtil.nativeCryptoSupported();

        boolean nativeSunRsaSignEnabled = Boolean.getBoolean("jdk.sunrsasign.enableNativeCrypto");
        System.out.println("nativeSunRsaSignEnabled: " + nativeSunRsaSignEnabled);

        if (nativeCryptoSupported && nativeSunRsaSignEnabled) {
            if (NativeSunRsaSignWrapper.isNativeCryptoEnabled()) {
                System.out.println("Native SunRsaSign is enabled as expected");
            } else {
                throw new RuntimeException("Native SunRsaSign is disabled as unexpected");
            }
        } else {
            if (NativeSunRsaSignWrapper.isNativeCryptoEnabled()) {
                throw new RuntimeException("Native SunRsaSign is enabled as unexpected");
            } else {
                System.out.println("Native SunRsaSign is disabled as expected");
            }
        }
    }
}
