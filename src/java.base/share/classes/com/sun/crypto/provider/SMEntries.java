/*
 * Copyright (C) 2023, Tencent. All rights reserved.
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

import java.security.Provider;

/**
 * Defines the entries on ShangMi algorithms.
 */
final class SMEntries {

    static void putEntries(Provider p) {
        p.put("Mac.HmacSM3", "com.sun.crypto.provider.HmacSM3");
        p.put("KeyGenerator.HmacSM3", "com.sun.crypto.provider.HmacSM3KeyGenerator");

        p.put("Cipher.SM4",
              "com.sun.crypto.provider.SM4Cipher$General");
        p.put("Cipher.SM4 SupportedModes", "CBC|CTR|ECB");
        p.put("Cipher.SM4 SupportedPaddings", "NOPADDING|PKCS7PADDING");
        p.put("Cipher.SM4/GCM/NoPadding",
              "com.sun.crypto.provider.GaloisCounterMode$SM4");

        p.put("AlgorithmParameters.SM4", "com.sun.crypto.provider.SM4Parameters");
        p.put("KeyGenerator.SM4", "com.sun.crypto.provider.SM4KeyGenerator");
    }
}
