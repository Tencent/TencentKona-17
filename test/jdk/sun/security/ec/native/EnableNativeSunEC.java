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
 * @summary The native EC is enabled when the system property
 *          jdk.sunec.enableNativeCrypto is true and the OpenSSL libcrypto
 *          is available.
 * @modules jdk.crypto.ec/sun.security.ec
 * @library /test/lib /test/jdk/openssl
 * @build jdk.crypto.ec/sun.security.ec.NativeSunECWrapper
 * @run main/othervm EnableNativeSunEC
 * @run main/othervm/policy=native.policy EnableNativeSunEC
 * @run main/othervm -Djdk.sunec.enableNativeCrypto=true EnableNativeSunEC
 * @run main/othervm/policy=native.policy -Djdk.sunec.enableNativeCrypto=true EnableNativeSunEC
 */

import sun.security.ec.NativeSunECWrapper;

public class EnableNativeSunEC {

    public static void main(String[] args) {
        boolean nativeCryptoSupported = NativeSunECUtil.nativeCryptoSupported();

        boolean nativeSunECEnabled = Boolean.getBoolean("jdk.sunec.enableNativeCrypto");
        System.out.println("nativeSunECEnabled: " + nativeSunECEnabled);

        if (nativeCryptoSupported && nativeSunECEnabled) {
            if (NativeSunECWrapper.isNativeCryptoEnabled()) {
                System.out.println("Native SunEC is enabled as expected");
            } else {
                throw new RuntimeException("Native SunEC is disabled as unexpected");
            }
        } else {
            if (NativeSunECWrapper.isNativeCryptoEnabled()) {
                throw new RuntimeException("Native SunEC is enabled as unexpected");
            } else {
                System.out.println("Native SunEC is disabled as expected");
            }
        }
    }
}
