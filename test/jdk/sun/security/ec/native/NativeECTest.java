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
 * @summary The EC key pair generation based on OpenSSL.
 * @modules jdk.crypto.ec/sun.security.ec
 * @library /test/lib
 * @build jdk.crypto.ec/sun.security.ec.NativeECWrapper NativeECUtil EnableOnNativeCrypto
 * @run junit/othervm -Djdk.sunec.enableNativeCrypto=true NativeECTest
 * @run junit/othervm/policy=native.policy -Djdk.sunec.enableNativeCrypto=true NativeECTest
 */

import java.nio.charset.StandardCharsets;
import java.security.*;
import java.util.Arrays;
import java.util.HexFormat;

import sun.security.ec.NativeECWrapper;

import org.junit.jupiter.api.Assertions;
import org.junit.jupiter.api.Test;

@EnableOnNativeCrypto
public class NativeECTest {

    private static final HexFormat HEX = HexFormat.of();
    private static final byte[] MESSAGE = "test".getBytes(StandardCharsets.UTF_8);

    @Test
    public void testECGenKeyPair() throws Exception {
        checkECGenKeyPair("secp256r1", 256, false);
        checkECGenKeyPair("secp256r1", 256, true);

        checkECGenKeyPair("secp384r1", 384, false);
        checkECGenKeyPair("secp384r1", 384, true);

        checkECGenKeyPair("secp521r1", 521, false);
        checkECGenKeyPair("secp521r1", 521, true);

        checkECGenKeyPair("curvesm2", 256, false);
        checkECGenKeyPair("curvesm2", 256, true);
    }

    @Test
    public void runECGenKeyPairSerially() throws Exception {
        NativeECUtil.execTaskSerially(()-> {
            testECGenKeyPair();
            return null;
        });
    }

    @Test
    public void runECGenKeyPairParallelly() throws Exception {
        NativeECUtil.execTaskParallelly(()-> {
            testECGenKeyPair();
            return null;
        });
    }

    @Test
    public void testECGenKeyPairWithUnsupportedCurve() {
        Assertions.assertThrows(InvalidAlgorithmParameterException.class,
                ()-> checkECGenKeyPair("unknown", 256, false));
        Assertions.assertThrows(InvalidAlgorithmParameterException.class,
                ()-> checkECGenKeyPair("unknown", 256, true));
    }

    private void checkECGenKeyPair(
            String curve, int orderLenInBits, boolean needSeed)
            throws Exception {
        int privKeyLen = NativeECUtil.privKeyLen(orderLenInBits);
        int pubKeyLen = NativeECUtil.pubKeyLen(privKeyLen);
        byte[] seed = needSeed ? NativeECUtil.seed(orderLenInBits) : null;

        int curveNID = NativeECWrapper.getCurveNID(curve);
        byte[] privKey = new byte[privKeyLen];
        byte[] pubKey = new byte[pubKeyLen];
        NativeECWrapper.ecGenKeyPair(curveNID, seed, privKey, pubKey);

        // The keys must not be zero-arrays
        Assertions.assertFalse(Arrays.equals(new byte[privKeyLen], privKey));
        Assertions.assertFalse(Arrays.equals(new byte[pubKeyLen], pubKey));

        // Public key must be uncompressed point
        Assertions.assertEquals(4, pubKey[0]);
    }

    @Test
    public void testXDHComputePubKey() throws Exception {
        checkXDHComputePubKey("X25519", 32);
        checkXDHComputePubKey("X448", 56);
    }

    @Test
    public void runXDHComputePubKeySerially() throws Exception {
        NativeECUtil.execTaskSerially(()-> {
            testXDHComputePubKey();
            return null;
        });
    }

    @Test
    public void runXDHComputePubKeyParallelly() throws Exception {
        NativeECUtil.execTaskParallelly(()-> {
            testXDHComputePubKey();
            return null;
        });
    }

    @Test
    public void testXDHComputePubKeyWithUnsupportedCurve() {
        Assertions.assertThrows(InvalidAlgorithmParameterException.class,
                ()-> checkXDHComputePubKey("unknown", 32));
    }

    private void checkXDHComputePubKey(String curve, int keyLength)
            throws Exception {
        byte[] privKey = new byte[keyLength];
        new SecureRandom().nextBytes(privKey);

        int curveNID = NativeECWrapper.getCurveNID(curve);
        byte[] pubKey = new byte[keyLength];
        NativeECWrapper.xdhComputePubKey(curveNID, privKey, pubKey);

        // The keys must not be zero-arrays
        Assertions.assertFalse(Arrays.equals(new byte[privKey.length], privKey));
        Assertions.assertFalse(Arrays.equals(new byte[pubKey.length], pubKey));
    }

    @Test
    public void testECDSASignature() throws Exception {
        checkECDSASignature("SHA-1", "secp256r1", 256, true);
        checkECDSASignature("SHA-256", "secp256r1", 256, true);
        checkECDSASignature("SHA-256", "secp256r1", 256, false);
        checkECDSASignature("SHA-384", "secp256r1", 256, true);
        checkECDSASignature("SHA-512", "secp521r1", 521, true);

        checkECDSASignature("SHA-1", "secp384r1", 384, true);
        checkECDSASignature("SHA-256", "secp384r1", 384, true);
        checkECDSASignature("SHA-384", "secp384r1", 384, true);
        checkECDSASignature("SHA-384", "secp384r1", 384, false);
        checkECDSASignature("SHA-512", "secp384r1", 384, true);

        checkECDSASignature("SHA-1", "secp521r1", 521, true);
        checkECDSASignature("SHA-256", "secp521r1", 521, true);
        checkECDSASignature("SHA-384", "secp521r1", 521, true);
        checkECDSASignature("SHA-512", "secp521r1", 521, true);
        checkECDSASignature("SHA-512", "secp521r1", 521, false);
    }

    @Test
    public void runECDSASignatureSerially() throws Exception {
        NativeECUtil.execTaskSerially(()-> {
            testECDSASignature();
            return null;
        });
    }

    @Test
    public void runECDSASignatureParallelly() throws Exception {
        NativeECUtil.execTaskParallelly(()-> {
            testECDSASignature();
            return null;
        });
    }

    private void checkECDSASignature(String md, String curve,
            int orderLenInBits, boolean needSeed)
            throws Exception {
        int privKeyLen = NativeECUtil.privKeyLen(orderLenInBits);
        int pubKeyLen = NativeECUtil.pubKeyLen(privKeyLen);

        byte[] digest = MessageDigest.getInstance(md).digest(MESSAGE);
        byte[] alignedDigest = new byte[privKeyLen];
        int length = Math.min(digest.length, alignedDigest.length);
        System.arraycopy(digest, 0, alignedDigest, alignedDigest.length - length, length);

        byte[] seed = needSeed ? NativeECUtil.seed(orderLenInBits) : null;

        int curveNID = NativeECWrapper.getCurveNID(curve);
        byte[] privKey = new byte[privKeyLen];
        byte[] pubKey = new byte[pubKeyLen];
        NativeECWrapper.ecGenKeyPair(curveNID, seed, privKey, pubKey);

        byte[] signature = new byte[privKeyLen * 2];
        NativeECWrapper.ecdsaSignDigest(curveNID, seed, privKey, alignedDigest, signature);

        // This signature must not be zero-array.
        Assertions.assertFalse(Arrays.equals(new byte[signature.length], signature));

        int verified = NativeECWrapper.ecdsaVerifySignedDigest(
                curveNID, pubKey, alignedDigest, signature);
        Assertions.assertEquals(1, verified);
    }

    @Test
    public void testECDSASignatureWithDiffKeys() throws Exception {
        int curveNID = NativeECWrapper.getCurveNID("secp256r1");

        byte[] privKeyA = new byte[32];
        byte[] pubKeyA = new byte[32 * 2 + 1];
        NativeECWrapper.ecGenKeyPair(curveNID, null, privKeyA, pubKeyA);

        byte[] privKeyB = new byte[32];
        byte[] pubKeyB = new byte[32 * 2 + 1];
        NativeECWrapper.ecGenKeyPair(curveNID, null, privKeyB, pubKeyB);

        byte[] digest = MessageDigest.getInstance("SHA-256").digest(MESSAGE);

        byte[] signature = new byte[32 * 2];
        NativeECWrapper.ecdsaSignDigest(curveNID, null, privKeyA, digest, signature);

        int verified = NativeECWrapper.ecdsaVerifySignedDigest(
                curveNID, pubKeyB, digest, signature);
        Assertions.assertTrue(verified != 1);
    }

    @Test
    public void testECDSASignatureWithDiffCurves() throws Exception {
        int curveNID = NativeECWrapper.getCurveNID("secp256r1");
        byte[] privKeyA = new byte[32];
        byte[] pubKeyA = new byte[32 * 2 + 1];
        NativeECWrapper.ecGenKeyPair(curveNID, null, privKeyA, pubKeyA);

        byte[] digest = MessageDigest.getInstance("SHA-256").digest(MESSAGE);

        byte[] signature = new byte[32 * 2];
        NativeECWrapper.ecdsaSignDigest(curveNID, null, privKeyA, digest, signature);

        int altCurveNID = NativeECWrapper.getCurveNID("secp384r1");
        byte[] altPrivKey = new byte[48];
        byte[] altPubKey = new byte[48 * 2 + 1];
        NativeECWrapper.ecGenKeyPair(altCurveNID, null, altPrivKey, altPubKey);

        Assertions.assertThrows(KeyException.class,
                () -> NativeECWrapper.ecdsaVerifySignedDigest(
                        curveNID, altPubKey, digest, signature));
    }

    @Test
    public void testECDSASignatureOnParams() throws Exception {
        byte[] sha256Digest = MessageDigest.getInstance("SHA-256").digest(MESSAGE);
        byte[] sha384Digest = MessageDigest.getInstance("SHA-384").digest(MESSAGE);

        int secp256r1CurveNID = NativeECWrapper.getCurveNID("secp256r1");
        byte[] secp256r1PrivKey = new byte[32];
        byte[] secp256r1PubKey = new byte[32 * 2 + 1];
        NativeECWrapper.ecGenKeyPair(secp256r1CurveNID, null, secp256r1PrivKey, secp256r1PubKey);
        byte[] secp256r1Signature = new byte[32 * 2];
        NativeECWrapper.ecdsaSignDigest(
                secp256r1CurveNID, null, secp256r1PrivKey, sha256Digest, secp256r1Signature);

        int secp384r1CurveNID = NativeECWrapper.getCurveNID("secp384r1");
        byte[] secp384r1PrivKey = new byte[48];
        byte[] secp384r1PubKey = new byte[48 * 2 + 1];
        NativeECWrapper.ecGenKeyPair(secp384r1CurveNID, null, secp384r1PrivKey, secp384r1PubKey);
        byte[] secp384r1Signature = new byte[48 * 2];
        NativeECWrapper.ecdsaSignDigest(
                secp384r1CurveNID, null, secp384r1PrivKey, sha384Digest, secp384r1Signature);

        Assertions.assertThrows(InvalidAlgorithmParameterException.class,
                () -> NativeECWrapper.ecdsaSignDigest(
                        -1, null, secp256r1PrivKey, sha256Digest, secp256r1Signature));
        Assertions.assertThrows(KeyException.class,
                () -> NativeECWrapper.ecdsaSignDigest(
                        secp256r1CurveNID, null, secp384r1PrivKey, sha256Digest, secp256r1Signature));
        Assertions.assertThrows(IllegalStateException.class,
                () -> NativeECWrapper.ecdsaSignDigest(
                        secp256r1CurveNID, null, secp256r1PrivKey, sha256Digest, secp384r1Signature));
        Assertions.assertThrows(IllegalStateException.class,
                () -> NativeECWrapper.ecdsaSignDigest(
                        secp384r1CurveNID, null, secp384r1PrivKey, sha256Digest, secp256r1Signature));

        Assertions.assertThrows(InvalidAlgorithmParameterException.class,
                () -> NativeECWrapper.ecdsaVerifySignedDigest(
                        -1, secp256r1PubKey, sha256Digest, secp256r1Signature));
        Assertions.assertThrows(KeyException.class,
                () -> NativeECWrapper.ecdsaVerifySignedDigest(
                        secp256r1CurveNID, secp384r1PubKey, sha256Digest, secp256r1Signature));
        Assertions.assertThrows(SignatureException.class,
                () -> NativeECWrapper.ecdsaVerifySignedDigest(
                        secp256r1CurveNID, secp256r1PubKey, sha256Digest, secp384r1Signature));
        Assertions.assertThrows(SignatureException.class,
                () -> NativeECWrapper.ecdsaVerifySignedDigest(
                        secp384r1CurveNID, secp384r1PubKey, sha384Digest, secp256r1Signature));
    }

    @Test
    public void testECDSASignatureKAT() throws Exception {
        checkECDSASignatureKAT(
                "secp256r1",
                "C477F9F65C22CCE20657FAA5B2D1D8122336F851A508A1ED04E479C34985BF96",
                "B7E08AFDFE94BAD3F1DC8C734798BA1C62B3A0AD1E9EA2A38201CD0889BC7A19",
                "3603F747959DBF7A4BB226E41928729063ADC7AE43529E61B563BBC606CC5E09",
                "SHA256",
                "Example of ECDSA with P-256",
                "2B42F576D07F4165FF65D1F3B1500F81E44C316F1F0B3EF57325B69ACA46104F",
                "DC42C2122D6392CD3E3A993A89502A8198C1886FE69D262C4B329BDB6B63FAF1");
        checkECDSASignatureKAT(
                "secp256r1",
                "C477F9F65C22CCE20657FAA5B2D1D8122336F851A508A1ED04E479C34985BF96",
                "B7E08AFDFE94BAD3F1DC8C734798BA1C62B3A0AD1E9EA2A38201CD0889BC7A19",
                "3603F747959DBF7A4BB226E41928729063ADC7AE43529E61B563BBC606CC5E09",
                "SHA3-256",
                "Example of ECDSA with P-256",
                "2B42F576D07F4165FF65D1F3B1500F81E44C316F1F0B3EF57325B69ACA46104F",
                "0A861C2526900245C73BACB9ADAEC1A5ACB3BA1F7114A3C334FDCD5B7690DADD");

        checkECDSASignatureKAT(
                "secp384r1",
                "F92C02ED629E4B48C0584B1C6CE3A3E3B4FAAE4AFC6ACB0455E73DFC392E6A0AE393A8565E6B9714D1224B57D83F8A08",
                "3BF701BC9E9D36B4D5F1455343F09126F2564390F2B487365071243C61E6471FB9D2AB74657B82F9086489D9EF0F5CB5",
                "D1A358EAFBF952E68D533855CCBDAA6FF75B137A5101443199325583552A6295FFE5382D00CFCDA30344A9B5B68DB855",
                "SHA384",
                "Example of ECDSA with P-384",
                "30EA514FC0D38D8208756F068113C7CADA9F66A3B40EA3B313D040D9B57DD41A332795D02CC7D507FCEF9FAF01A27088",
                "CC808E504BE414F46C9027BCBF78ADF067A43922D6FCAA66C4476875FBB7B94EFD1F7D5DBE620BFB821C46D549683AD8");
        checkECDSASignatureKAT(
                "secp384r1",
                "F92C02ED629E4B48C0584B1C6CE3A3E3B4FAAE4AFC6ACB0455E73DFC392E6A0AE393A8565E6B9714D1224B57D83F8A08",
                "3BF701BC9E9D36B4D5F1455343F09126F2564390F2B487365071243C61E6471FB9D2AB74657B82F9086489D9EF0F5CB5",
                "D1A358EAFBF952E68D533855CCBDAA6FF75B137A5101443199325583552A6295FFE5382D00CFCDA30344A9B5B68DB855",
                "SHA3-384",
                "Example of ECDSA with P-384",
                "30EA514FC0D38D8208756F068113C7CADA9F66A3B40EA3B313D040D9B57DD41A332795D02CC7D507FCEF9FAF01A27088",
                "691B9D4969451A98036D53AA725458602125DE74881BBC333012CA4FA55BDE39D1BF16A6AAE3FE4992C567C6E7892337");
    }

    private void checkECDSASignatureKAT(
            String curve,
            String privKeyHex, String pubXHex, String pubYHex,
            String md, String message,
            String expRHex, String expSHex) throws Exception {
        System.out.println(md + " with " + curve);

        byte[] privKey = HEX.parseHex(privKeyHex);
        byte[] pubKey = HEX.parseHex("04" + pubXHex + pubYHex);

        byte[] digest = MessageDigest.getInstance(md).digest(message.getBytes());
        byte[] alignedDigest = new byte[privKey.length];
        int length = Math.min(digest.length, alignedDigest.length);
        System.arraycopy(digest, 0, alignedDigest, alignedDigest.length - length, length);

        int curveNID = NativeECWrapper.getCurveNID(curve);

        byte[] sig = new byte[privKey.length * 2];
        NativeECWrapper.ecdsaSignDigest(curveNID, null, privKey, alignedDigest, sig);

        Assertions.assertEquals(1, NativeECWrapper.ecdsaVerifySignedDigest(
                curveNID, pubKey, alignedDigest, sig));

        Assertions.assertEquals(1, NativeECWrapper.ecdsaVerifySignedDigest(
                curveNID, pubKey, alignedDigest, HEX.parseHex(expRHex + expSHex)));
    }
}
