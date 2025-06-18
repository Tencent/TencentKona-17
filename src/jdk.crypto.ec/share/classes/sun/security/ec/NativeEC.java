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

package sun.security.ec;

import sun.security.action.GetBooleanAction;
import sun.security.action.GetPropertyAction;
import sun.security.util.ECUtil;

import java.security.*;
import java.security.spec.ECParameterSpec;

/**
 * The native implementation with OpenSSL for EC algorithms.
 */
final class NativeEC {

    private static boolean enableNativeCrypto;

    static {
        enableNativeCrypto = GetBooleanAction.privilegedGetProperty(
                "jdk.sunec.enableNativeCrypto");
        if (enableNativeCrypto) {
             // opensslcrypto must be loaded at first
            enableNativeCrypto = loadOpenSSLCryptoLib() &&
                                 loadSunECCryptoLib();
        }
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
            System.err.println("Failed to load opensslcrypto: " + e);
            loaded = false;
        }
        return loaded;
     }

    // Load lib suneccrypto
    @SuppressWarnings("removal")
    private static boolean loadSunECCryptoLib() {
        boolean loaded = true;
        try {
            AccessController.doPrivileged((PrivilegedAction<Void>) () -> {
                    System.loadLibrary("suneccrypto");
                    return null;
            });
        } catch (UnsatisfiedLinkError e) {
            System.err.println("Failed to load suneccrypto: " + e);
            loaded = false;
        }
        return loaded;
    }

    static boolean isNativeCryptoEnabled() {
        return enableNativeCrypto;
    }

    static int NID_SECP256R1 = 415;
    static int NID_SECP384R1 = 715;
    static int NID_SECP521R1 = 716;
    static int NID_CURVESM2 = 1172;

    static int NID_X25519 = 1034;
    static int NID_X448 = 1035;

    static int getCurveNID(String curve) {
        if ("secp256r1".equalsIgnoreCase(curve)) {
            return NID_SECP256R1;
        } else if ("secp384r1".equalsIgnoreCase(curve)) {
            return NID_SECP384R1;
        } else if ("secp521r1".equalsIgnoreCase(curve)) {
            return NID_SECP521R1;
        } else if ("curvesm2".equalsIgnoreCase(curve)) {
            return NID_CURVESM2;
        } else if ("x25519".equalsIgnoreCase(curve)) {
            return NID_X25519;
        } else if ("x448".equalsIgnoreCase(curve)) {
            return NID_X448;
        }

        return -1;
    }

    // Encoded OID on secp256r1: 06082A8648CE3D030107
    static byte[] ENCODED_SECP256R1 = new byte[] {
            6, 8, 42, -122, 72, -50, 61, 3, 1, 7};

    // Encoded OID on secp384r1: 06052B81040022
    static byte[] ENCODED_SECP384R1 = new byte[] {
            6, 5, 43, -127, 4, 0, 34};

    // Encoded OID on secp521r1: 06052B81040023
    static byte[] ENCODED_SECP521R1 = new byte[] {
            6, 5, 43, -127, 4, 0, 35};

    // Encoded OID on curveSM2: 06082A811CCF5501822D
    static byte[] ENCODED_CURVESM2 = new byte[] {
            6, 8, 42, -127, 28, -49, 85, 1, -126, 45 };

    static int getECCurveNID(byte[] encodedOID) {
        if (MessageDigest.isEqual(encodedOID, ENCODED_SECP256R1)) {
            return NID_SECP256R1;
        } else if (MessageDigest.isEqual(encodedOID, ENCODED_SECP384R1)) {
            return NID_SECP384R1;
        } else if (MessageDigest.isEqual(encodedOID, ENCODED_SECP521R1)) {
            return NID_SECP521R1;
        } else if (MessageDigest.isEqual(encodedOID, ENCODED_CURVESM2)) {
            return NID_CURVESM2;
        }

        return -1;
    }

    static int getXECCurveNID(String curve) {
        if ("x25519".equalsIgnoreCase(curve)) {
            return NID_X25519;
        } else if ("x448".equalsIgnoreCase(curve)) {
            return NID_X448;
        }

        return -1;
    }

    static boolean useNativeEC(ECParameterSpec params) {
        return enableNativeCrypto && isSupportedECCurve(params);
    }

    private static boolean isSupportedECCurve(ECParameterSpec params) {
        byte[] encodedParams = ECUtil.encodeECParameterSpec(null, params);
        int curveNID = getECCurveNID(encodedParams);
        return curveNID != -1;
    }

    static boolean useNativeECDSA(ECParameterSpec params) {
        return enableNativeCrypto && isSupportedECDSACurve(params);
    }

    // Only support secp256r1 and secp384r1
    private static boolean isSupportedECDSACurve(ECParameterSpec params) {
        byte[] encodedParams = ECUtil.encodeECParameterSpec(null, params);
        int curveNID = getECCurveNID(encodedParams);
        return curveNID == NID_SECP256R1 || curveNID == NID_SECP384R1;
    }

    static boolean useNativeXDH(String curve) {
        return enableNativeCrypto && getXECCurveNID(curve) != -1;
    }

    // If the length of value is less than expectedLength,
    // it will be padded with leading zeros.
    // If the length is expectedLength + 1, and the first is zero,
    // it just removes the leading zero.
    static byte[] padZerosForValue(byte[] value, int expectedLength) {
        if (value.length == expectedLength) {
            return value;
        }

        if (value.length == expectedLength + 1 && value[0] == 0) {
            byte[] paddedValue = new byte[expectedLength];
            System.arraycopy(value, 1, paddedValue, 0, expectedLength);
            return paddedValue;
        }

        if (value.length < expectedLength) {
            byte[] paddedValue = new byte[expectedLength];
            System.arraycopy(value, 0,
                    paddedValue, expectedLength - value.length,
                    value.length);
            return paddedValue;
        }

        throw new IllegalArgumentException("Invalid value format");
    }

    // valuePair is formatted as offset|A|B, and the lengths of A and B must be the same.
    // If the length of A/B is less than expectedLength, it will be padded with leading zeros.
    static byte[] padZerosForValuePair(byte[] valuePair, int offset, int expectedLength) {
        if ((valuePair.length - offset) % 2 != 0
                || (valuePair.length - offset) >= (expectedLength * 2)) {
            return valuePair;
        }

        int length = valuePair.length / 2;
        byte[] paddedValue = new byte[offset + expectedLength * 2];
        System.arraycopy(valuePair, 0, paddedValue, 0, offset);
        System.arraycopy(valuePair, 0, paddedValue, expectedLength - offset - length, length);
        System.arraycopy(valuePair, length, paddedValue, paddedValue.length - length, length);
        return paddedValue;
    }

    static byte[] padZerosForValuePair(byte[] valuePair, int expectedLength) {
        return padZerosForValuePair(valuePair, 0, expectedLength);
    }

    static native void ecGenKeyPair(int curveNID, byte[] seed,
            byte[] privKeyOut, byte[] pubKeyOut)
            throws GeneralSecurityException;

    static native void xdhComputePubKey(int curveNID,
            byte[] privKeyIn, byte[] pubKeyOut)
            throws GeneralSecurityException;

    static native void ecdsaSignDigest(int curveNID, byte[] seed,
            byte[] privKey, byte[] digest, byte[] signatureOut)
            throws GeneralSecurityException;

    static native int ecdsaVerifySignedDigest(int curveNID,
            byte[] pubKey, byte[] digest, byte[] signature)
            throws GeneralSecurityException;

    static native void ecdhDeriveKey(int curveNID,
            byte[] privKey, byte[] peerPubKey, byte[] sharedKeyOut)
            throws GeneralSecurityException;

    static native void xdhDeriveKey(int curveNID,
            byte[] privKey, byte[] peerPubKey, byte[] sharedKeyOut)
            throws GeneralSecurityException;
}
