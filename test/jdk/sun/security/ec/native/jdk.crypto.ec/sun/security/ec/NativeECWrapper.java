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

package sun.security.ec;

import java.security.GeneralSecurityException;

/**
 * A wrapper of NativeEC for exposing the internal APIs.
 */
public class NativeECWrapper {

    public static boolean isNativeCryptoEnabled() {
        return NativeEC.isNativeCryptoEnabled();
    }

    public static int getCurveNID(String curve) {
        return NativeEC.getCurveNID(curve);
    }

    public static void ecGenKeyPair(int curveNID, byte[] seed,
            byte[] privKeyOut, byte[] pubKeyOut)
            throws GeneralSecurityException {
        NativeEC.ecGenKeyPair(curveNID, seed, privKeyOut, pubKeyOut);
    }

    public static void xdhComputePubKey(int curveNID,
            byte[] privKeyIn, byte[] pubKeyOut)
            throws GeneralSecurityException {
        NativeEC.xdhComputePubKey(curveNID, privKeyIn, pubKeyOut);
    }

    public static void ecdsaSignDigest(int curveNID, byte[] seed,
            byte[] privKey, byte[] digest, byte[] signatureOut)
            throws GeneralSecurityException {
        NativeEC.ecdsaSignDigest(curveNID, seed, privKey, digest, signatureOut);
    }

    public static int ecdsaVerifySignedDigest(int curveNID,
            byte[] pubKey, byte[] digest, byte[] signature)
            throws GeneralSecurityException {
        return NativeEC.ecdsaVerifySignedDigest(curveNID, pubKey, digest, signature);
    }

    public static void ecdhDeriveKey(int curveNID,
            byte[] privKey, byte[] peerPubKey, byte[] sharedKeyOut)
            throws GeneralSecurityException {
        NativeEC.ecdhDeriveKey(curveNID, privKey, peerPubKey, sharedKeyOut);
    }

    public static void xdhDeriveKey(int curveNID,
            byte[] privKey, byte[] peerPubKey, byte[] sharedKeyOut)
            throws GeneralSecurityException {
        NativeEC.xdhDeriveKey(curveNID, privKey, peerPubKey, sharedKeyOut);
    }
}
