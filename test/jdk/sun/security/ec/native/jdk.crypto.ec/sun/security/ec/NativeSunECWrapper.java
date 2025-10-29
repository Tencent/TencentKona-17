/*
 * Copyright (C) 2025, Tencent. All rights reserved.
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
 * A wrapper of NativeSunEC for exposing the internal APIs.
 */
public class NativeSunECWrapper {

    public static boolean isNativeCryptoEnabled() {
        return NativeSunEC.isNativeCryptoEnabled();
    }

    public static int getCurveNID(String curve) {
        return NativeSunEC.getCurveNID(curve);
    }

    public static byte[] padZerosForValuePair(byte[] valuePair, int offset, int expectedLength) {
        return NativeSunEC.padZerosForValuePair(valuePair, offset, expectedLength);
    }

    public static void ecGenKeyPair(int curveNID, byte[] seed,
            byte[] privKeyOut, byte[] pubKeyOut)
            throws GeneralSecurityException {
        NativeSunEC.ecGenKeyPair(curveNID, seed, privKeyOut, pubKeyOut);
    }

    public static void xdhComputePubKey(int curveNID,
            byte[] privKeyIn, byte[] pubKeyOut)
            throws GeneralSecurityException {
        NativeSunEC.xdhComputePubKey(curveNID, privKeyIn, pubKeyOut);
    }

    public static void ecdsaSignDigest(int curveNID, byte[] seed,
            byte[] privKey, byte[] digest, byte[] signatureOut)
            throws GeneralSecurityException {
        NativeSunEC.ecdsaSignDigest(curveNID, seed, privKey, digest, signatureOut);
    }

    public static int ecdsaVerifySignedDigest(int curveNID,
            byte[] pubKey, byte[] digest, byte[] signature)
            throws GeneralSecurityException {
        return NativeSunEC.ecdsaVerifySignedDigest(curveNID, pubKey, digest, signature);
    }

    public static void ecdhDeriveKey(int curveNID,
            byte[] privKey, byte[] peerPubKey, byte[] sharedKeyOut)
            throws GeneralSecurityException {
        NativeSunEC.ecdhDeriveKey(curveNID, privKey, peerPubKey, sharedKeyOut);
    }

    public static void xdhDeriveKey(int curveNID,
            byte[] privKey, byte[] peerPubKey, byte[] sharedKeyOut)
            throws GeneralSecurityException {
        NativeSunEC.xdhDeriveKey(curveNID, privKey, peerPubKey, sharedKeyOut);
    }

    public static byte[] sm2Encrypt(byte[] pubKey,
            byte[] plaintext, int plaintextOffset, int plaintextLenth) {
        return NativeSunEC.sm2Encrypt(pubKey,
                plaintext, plaintextOffset, plaintextLenth);
    }

    public static byte[] sm2Decrypt(byte[] privKey,
            byte[] ciphertext, int ciphertextOffset, int ciphertextLenth) {
        return NativeSunEC.sm2Decrypt(privKey,
                ciphertext, ciphertextOffset, ciphertextLenth);
    }

    public static byte[] sm2Sign(byte[] privKey, byte[] pubKey,
            byte[] id, byte[] message) {
        return NativeSunEC.sm2Sign(privKey, pubKey, id, message);
    }

    public static boolean sm2Verify(byte[] pubKey,
            byte[] id, byte[] message, byte[] signature) {
        return NativeSunEC.sm2Verify(pubKey, id, message, signature);
    }

    public static byte[] sm2DeriveKey(
            byte[] priKey, byte[] pubKey, byte[] ePrivKey, byte[] id,
            byte[] peerPubKey, byte[] peerEPubKey, byte[] peerId,
            boolean isInitiator, int sharedKeyLength) {
        return NativeSunEC.sm2DeriveKey(
                priKey, pubKey, ePrivKey, id,
                peerPubKey, peerEPubKey, peerId,
                isInitiator, sharedKeyLength);
    }
}
