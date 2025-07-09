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
 * @summary The EC crypto based on OpenSSL.
 * @modules jdk.crypto.ec/sun.security.ec
 * @library /test/lib /test/jdk/openssl
 * @build jdk.crypto.ec/sun.security.ec.NativeSunECWrapper NativeSunECUtil
 * @run junit/othervm -Djdk.sunec.enableNativeCrypto=true NativeSunECTest
 * @run junit/othervm/policy=native.policy -Djdk.sunec.enableNativeCrypto=true NativeSunECTest
 */

import java.nio.charset.StandardCharsets;
import java.security.*;
import java.util.Arrays;
import java.util.HexFormat;

import sun.security.ec.NativeSunECWrapper;

import org.junit.jupiter.api.Assertions;
import org.junit.jupiter.api.Test;

@EnableOnNativeSunEC
public class NativeSunECTest {

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
        NativeSunECUtil.execTaskSerially(()-> {
            testECGenKeyPair();
            return null;
        });
    }

    @Test
    public void runECGenKeyPairParallelly() throws Exception {
        NativeSunECUtil.execTaskParallelly(()-> {
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
        int privKeyLen = NativeSunECUtil.privKeyLen(orderLenInBits);
        int pubKeyLen = NativeSunECUtil.pubKeyLen(privKeyLen);
        byte[] seed = needSeed ? NativeSunECUtil.seed(orderLenInBits) : null;

        int curveNID = NativeSunECWrapper.getCurveNID(curve);
        byte[] privKey = new byte[privKeyLen];
        byte[] pubKey = new byte[pubKeyLen];
        NativeSunECWrapper.ecGenKeyPair(curveNID, seed, privKey, pubKey);

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
        NativeSunECUtil.execTaskSerially(()-> {
            testXDHComputePubKey();
            return null;
        });
    }

    @Test
    public void runXDHComputePubKeyParallelly() throws Exception {
        NativeSunECUtil.execTaskParallelly(()-> {
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

        int curveNID = NativeSunECWrapper.getCurveNID(curve);
        byte[] pubKey = new byte[keyLength];
        NativeSunECWrapper.xdhComputePubKey(curveNID, privKey, pubKey);

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
        NativeSunECUtil.execTaskSerially(()-> {
            testECDSASignature();
            return null;
        });
    }

    @Test
    public void runECDSASignatureParallelly() throws Exception {
        NativeSunECUtil.execTaskParallelly(()-> {
            testECDSASignature();
            return null;
        });
    }

    private void checkECDSASignature(String md, String curve,
            int orderLenInBits, boolean needSeed)
            throws Exception {
        int privKeyLen = NativeSunECUtil.privKeyLen(orderLenInBits);
        int pubKeyLen = NativeSunECUtil.pubKeyLen(privKeyLen);

        byte[] digest = MessageDigest.getInstance(md).digest(MESSAGE);
        byte[] alignedDigest = new byte[privKeyLen];
        int length = Math.min(digest.length, alignedDigest.length);
        System.arraycopy(digest, 0, alignedDigest, alignedDigest.length - length, length);

        byte[] seed = needSeed ? NativeSunECUtil.seed(orderLenInBits) : null;

        int curveNID = NativeSunECWrapper.getCurveNID(curve);
        byte[] privKey = new byte[privKeyLen];
        byte[] pubKey = new byte[pubKeyLen];
        NativeSunECWrapper.ecGenKeyPair(curveNID, seed, privKey, pubKey);

        byte[] signature = new byte[privKeyLen * 2];
        NativeSunECWrapper.ecdsaSignDigest(curveNID, seed, privKey, alignedDigest, signature);

        // This signature must not be zero-array.
        Assertions.assertFalse(Arrays.equals(new byte[signature.length], signature));

        int verified = NativeSunECWrapper.ecdsaVerifySignedDigest(
                curveNID, pubKey, alignedDigest, signature);
        Assertions.assertEquals(1, verified);
    }

    @Test
    public void testECDSASignatureWithDiffKeys() throws Exception {
        int curveNID = NativeSunECWrapper.getCurveNID("secp256r1");

        byte[] privKeyA = new byte[32];
        byte[] pubKeyA = new byte[32 * 2 + 1];
        NativeSunECWrapper.ecGenKeyPair(curveNID, null, privKeyA, pubKeyA);

        byte[] privKeyB = new byte[32];
        byte[] pubKeyB = new byte[32 * 2 + 1];
        NativeSunECWrapper.ecGenKeyPair(curveNID, null, privKeyB, pubKeyB);

        byte[] digest = MessageDigest.getInstance("SHA-256").digest(MESSAGE);

        byte[] signature = new byte[32 * 2];
        NativeSunECWrapper.ecdsaSignDigest(curveNID, null, privKeyA, digest, signature);

        int verified = NativeSunECWrapper.ecdsaVerifySignedDigest(
                curveNID, pubKeyB, digest, signature);
        Assertions.assertTrue(verified != 1);
    }

    @Test
    public void testECDSASignatureWithDiffCurves() throws Exception {
        int curveNID = NativeSunECWrapper.getCurveNID("secp256r1");
        byte[] privKeyA = new byte[32];
        byte[] pubKeyA = new byte[32 * 2 + 1];
        NativeSunECWrapper.ecGenKeyPair(curveNID, null, privKeyA, pubKeyA);

        byte[] digest = MessageDigest.getInstance("SHA-256").digest(MESSAGE);

        byte[] signature = new byte[32 * 2];
        NativeSunECWrapper.ecdsaSignDigest(curveNID, null, privKeyA, digest, signature);

        int altCurveNID = NativeSunECWrapper.getCurveNID("secp384r1");
        byte[] altPrivKey = new byte[48];
        byte[] altPubKey = new byte[48 * 2 + 1];
        NativeSunECWrapper.ecGenKeyPair(altCurveNID, null, altPrivKey, altPubKey);

        Assertions.assertThrows(KeyException.class,
                () -> NativeSunECWrapper.ecdsaVerifySignedDigest(
                        curveNID, altPubKey, digest, signature));
    }

    @Test
    public void testECDSASignatureOnParams() throws Exception {
        byte[] sha256Digest = MessageDigest.getInstance("SHA-256").digest(MESSAGE);
        byte[] sha384Digest = MessageDigest.getInstance("SHA-384").digest(MESSAGE);

        int secp256r1CurveNID = NativeSunECWrapper.getCurveNID("secp256r1");
        byte[] secp256r1PrivKey = new byte[32];
        byte[] secp256r1PubKey = new byte[32 * 2 + 1];
        NativeSunECWrapper.ecGenKeyPair(secp256r1CurveNID, null, secp256r1PrivKey, secp256r1PubKey);
        byte[] secp256r1Signature = new byte[32 * 2];
        NativeSunECWrapper.ecdsaSignDigest(
                secp256r1CurveNID, null, secp256r1PrivKey, sha256Digest, secp256r1Signature);

        int secp384r1CurveNID = NativeSunECWrapper.getCurveNID("secp384r1");
        byte[] secp384r1PrivKey = new byte[48];
        byte[] secp384r1PubKey = new byte[48 * 2 + 1];
        NativeSunECWrapper.ecGenKeyPair(secp384r1CurveNID, null, secp384r1PrivKey, secp384r1PubKey);
        byte[] secp384r1Signature = new byte[48 * 2];
        NativeSunECWrapper.ecdsaSignDigest(
                secp384r1CurveNID, null, secp384r1PrivKey, sha384Digest, secp384r1Signature);

        Assertions.assertThrows(InvalidAlgorithmParameterException.class,
                () -> NativeSunECWrapper.ecdsaSignDigest(
                        -1, null, secp256r1PrivKey, sha256Digest, secp256r1Signature));
        Assertions.assertThrows(KeyException.class,
                () -> NativeSunECWrapper.ecdsaSignDigest(
                        secp256r1CurveNID, null, secp384r1PrivKey, sha256Digest, secp256r1Signature));
        Assertions.assertThrows(IllegalStateException.class,
                () -> NativeSunECWrapper.ecdsaSignDigest(
                        secp256r1CurveNID, null, secp256r1PrivKey, sha256Digest, secp384r1Signature));
        Assertions.assertThrows(IllegalStateException.class,
                () -> NativeSunECWrapper.ecdsaSignDigest(
                        secp384r1CurveNID, null, secp384r1PrivKey, sha256Digest, secp256r1Signature));

        Assertions.assertThrows(InvalidAlgorithmParameterException.class,
                () -> NativeSunECWrapper.ecdsaVerifySignedDigest(
                        -1, secp256r1PubKey, sha256Digest, secp256r1Signature));
        Assertions.assertThrows(KeyException.class,
                () -> NativeSunECWrapper.ecdsaVerifySignedDigest(
                        secp256r1CurveNID, secp384r1PubKey, sha256Digest, secp256r1Signature));
        Assertions.assertThrows(SignatureException.class,
                () -> NativeSunECWrapper.ecdsaVerifySignedDigest(
                        secp256r1CurveNID, secp256r1PubKey, sha256Digest, secp384r1Signature));
        Assertions.assertThrows(SignatureException.class,
                () -> NativeSunECWrapper.ecdsaVerifySignedDigest(
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

        int curveNID = NativeSunECWrapper.getCurveNID(curve);

        byte[] sig = new byte[privKey.length * 2];
        NativeSunECWrapper.ecdsaSignDigest(curveNID, null, privKey, alignedDigest, sig);

        Assertions.assertEquals(1, NativeSunECWrapper.ecdsaVerifySignedDigest(
                curveNID, pubKey, alignedDigest, sig));

        Assertions.assertEquals(1, NativeSunECWrapper.ecdsaVerifySignedDigest(
                curveNID, pubKey, alignedDigest, HEX.parseHex(expRHex + expSHex)));
    }

    @Test
    public void testECDHKeyAgreement() throws Exception {
        checkECDHKeyAgreement("secp256r1", 256);
        checkECDHKeyAgreement("secp384r1", 384);
        checkECDHKeyAgreement("secp521r1", 521);
        checkECDHKeyAgreement("curvesm2", 256);
    }

    private void checkECDHKeyAgreement(String curve, int orderLenInBits)
            throws Exception {
        int privKeyLen = NativeSunECUtil.privKeyLen(orderLenInBits);
        int pubKeyLen = NativeSunECUtil.pubKeyLen(privKeyLen);
        int curveNID = NativeSunECWrapper.getCurveNID(curve);

        byte[] privKey = new byte[privKeyLen];
        byte[] pubKey = new byte[pubKeyLen];
        NativeSunECWrapper.ecGenKeyPair(curveNID, null, privKey, pubKey);

        byte[] peerPrivKey = new byte[privKeyLen];
        byte[] peerPubKey = new byte[pubKeyLen];
        NativeSunECWrapper.ecGenKeyPair(curveNID, null, peerPrivKey, peerPubKey);

        byte[] sharedKey = new byte[privKeyLen];
        NativeSunECWrapper.ecdhDeriveKey(curveNID, privKey, peerPubKey, sharedKey);

        byte[] peerSharedKey = new byte[privKeyLen];
        NativeSunECWrapper.ecdhDeriveKey(curveNID, peerPrivKey, pubKey, peerSharedKey);

        // This shared key must not be zero-array.
        Assertions.assertFalse(Arrays.equals(new byte[sharedKey.length], sharedKey));

        Assertions.assertArrayEquals(sharedKey, peerSharedKey);
    }

    @Test
    public void runECDHKeyAgreementSerially() throws Exception {
        NativeSunECUtil.execTaskSerially(()-> {
            testECDHKeyAgreement();
            return null;
        });
    }

    @Test
    public void runECDHKeyAgreementParallelly() throws Exception {
        NativeSunECUtil.execTaskParallelly(()-> {
            testECDHKeyAgreement();
            return null;
        });
    }

    @Test
    public void testECDHKeyAgreementWithDiffKeys() throws Exception {
        int curveNID = NativeSunECWrapper.getCurveNID("secp256r1");

        byte[] privKey = new byte[32];
        byte[] pubKey = new byte[32 * 2 + 1];
        NativeSunECWrapper.ecGenKeyPair(curveNID, null, privKey, pubKey);

        byte[] peerPrivKey = new byte[32];
        byte[] peerPubKey = new byte[32 * 2 + 1];
        NativeSunECWrapper.ecGenKeyPair(curveNID, null, peerPrivKey, peerPubKey);

        byte[] sharedKey = new byte[32];
        NativeSunECWrapper.ecdhDeriveKey(curveNID, privKey, pubKey, sharedKey);

        byte[] peerSharedKey = new byte[32];
        NativeSunECWrapper.ecdhDeriveKey(curveNID, peerPrivKey, pubKey, peerSharedKey);

        Assertions.assertFalse(Arrays.equals(sharedKey, peerSharedKey));
    }

    @Test
    public void testECDHKeyAgreementWithDiffCurves() throws Exception {
        int curveNID = NativeSunECWrapper.getCurveNID("secp256r1");
        byte[] privKey = new byte[32];
        byte[] pubKey = new byte[32 * 2 + 1];
        NativeSunECWrapper.ecGenKeyPair(curveNID, null, privKey, pubKey);

        int altCurveNID = NativeSunECWrapper.getCurveNID("curvesm2");
        byte[] peerAltPrivKey = new byte[32];
        byte[] peerAltPubKey = new byte[32 * 2 + 1];
        NativeSunECWrapper.ecGenKeyPair(altCurveNID, null, peerAltPrivKey, peerAltPubKey);

        byte[] sharedKey = new byte[32];
        Assertions.assertThrows(KeyException.class,
                () -> NativeSunECWrapper.ecdhDeriveKey(
                        curveNID, privKey, peerAltPubKey, sharedKey));
    }

    @Test
    public void testECDHKeyAgreementOnParams() throws Exception {
        int secp256r1NID = NativeSunECWrapper.getCurveNID("secp256r1");
        byte[] secp256r1PrivKey = new byte[32];
        byte[] secp256r1PubKey = new byte[32 * 2 + 1];
        NativeSunECWrapper.ecGenKeyPair(secp256r1NID, null, secp256r1PrivKey, secp256r1PubKey);
        byte[] secp256r1SharedKey = new byte[32];

        int secp384r1NID = NativeSunECWrapper.getCurveNID("secp384r1");
        byte[] secp384r1PrivKey = new byte[48];
        byte[] secp384r1PubKey = new byte[48 * 2 + 1];
        NativeSunECWrapper.ecGenKeyPair(secp384r1NID, null, secp384r1PrivKey, secp384r1PubKey);
        byte[] secp384r1SharedKey = new byte[48];

        Assertions.assertThrows(InvalidAlgorithmParameterException.class,
                () -> NativeSunECWrapper.ecdhDeriveKey(
                        -1, secp256r1PrivKey, secp256r1PubKey, secp256r1SharedKey));

        Assertions.assertThrows(InvalidKeyException.class,
                () -> NativeSunECWrapper.ecdhDeriveKey(
                        secp256r1NID, secp384r1PrivKey, secp256r1PubKey, secp256r1SharedKey));

        Assertions.assertThrows(InvalidKeyException.class,
                () -> NativeSunECWrapper.ecdhDeriveKey(
                        secp256r1NID, secp256r1PrivKey, secp384r1PubKey, secp256r1SharedKey));

        Assertions.assertThrows(IllegalStateException.class,
                () -> NativeSunECWrapper.ecdhDeriveKey(
                        secp256r1NID, secp256r1PrivKey, secp256r1PubKey, secp384r1SharedKey));
        Assertions.assertThrows(IllegalStateException.class,
                () -> NativeSunECWrapper.ecdhDeriveKey(
                        secp384r1NID, secp384r1PrivKey, secp384r1PubKey, secp256r1SharedKey));
    }

    @Test
    public void testXDHKeyAgreement() throws Exception {
        checkXDHKeyAgreement("x25519", 32);
        checkXDHKeyAgreement("x448", 56);
    }

    private void checkXDHKeyAgreement(String curve, int keyLength)
            throws Exception {
        int curveNID = NativeSunECWrapper.getCurveNID(curve);

        byte[] privKey = new byte[keyLength];
        new SecureRandom().nextBytes(privKey);
        byte[] pubKey = new byte[keyLength];
        NativeSunECWrapper.xdhComputePubKey(curveNID, privKey, pubKey);

        byte[] peerPrivKey = new byte[keyLength];
        new SecureRandom().nextBytes(peerPrivKey);
        byte[] peerPubKey = new byte[keyLength];
        NativeSunECWrapper.xdhComputePubKey(curveNID, peerPrivKey, peerPubKey);

        byte[] sharedKey = new byte[keyLength];
        NativeSunECWrapper.xdhDeriveKey(curveNID, privKey, peerPubKey, sharedKey);

        byte[] peerSharedKey = new byte[keyLength];
        NativeSunECWrapper.xdhDeriveKey(curveNID, peerPrivKey, pubKey, peerSharedKey);

        // This shared key must not be zero-array.
        Assertions.assertFalse(Arrays.equals(new byte[sharedKey.length], sharedKey));

        Assertions.assertArrayEquals(sharedKey, peerSharedKey);
    }

    @Test
    public void runXDHKeyAgreementSerially() throws Exception {
        NativeSunECUtil.execTaskSerially(()-> {
            testXDHKeyAgreement();
            return null;
        });
    }

    @Test
    public void runXDHKeyAgreementParallelly() throws Exception {
        NativeSunECUtil.execTaskParallelly(()-> {
            testXDHKeyAgreement();
            return null;
        });
    }

    @Test
    public void testXDHKeyAgreementWithDiffKeys() throws Exception {
        int curveNID = NativeSunECWrapper.getCurveNID("x25519");

        byte[] privKey = new byte[32];
        new SecureRandom().nextBytes(privKey);
        byte[] pubKey = new byte[32];
        NativeSunECWrapper.xdhComputePubKey(curveNID, privKey, pubKey);

        byte[] peerPrivKey = new byte[32];
        new SecureRandom().nextBytes(peerPrivKey);
        byte[] peerPubKey = new byte[32];
        NativeSunECWrapper.xdhComputePubKey(curveNID, peerPrivKey, peerPubKey);

        byte[] sharedKey = new byte[32];
        NativeSunECWrapper.xdhDeriveKey(curveNID, privKey, pubKey, sharedKey);

        byte[] peerSharedKey = new byte[32];
        NativeSunECWrapper.xdhDeriveKey(curveNID, peerPrivKey, pubKey, peerSharedKey);

        Assertions.assertFalse(Arrays.equals(sharedKey, peerSharedKey));
    }

    @Test
    public void testXDHKeyAgreementWithDiffCurves() throws Exception {
        int curveNID = NativeSunECWrapper.getCurveNID("x25519");
        byte[] privKey = new byte[32];
        new SecureRandom().nextBytes(privKey);
        byte[] pubKey = new byte[32];
        NativeSunECWrapper.xdhComputePubKey(curveNID, privKey, pubKey);

        int altCurveNID = NativeSunECWrapper.getCurveNID("x448");
        byte[] peerAltPrivKey = new byte[56];
        new SecureRandom().nextBytes(peerAltPrivKey);
        byte[] peerAltPubKey = new byte[56];
        NativeSunECWrapper.xdhComputePubKey(altCurveNID, peerAltPrivKey, peerAltPubKey);

        Assertions.assertThrows(KeyException.class,
                () -> NativeSunECWrapper.xdhDeriveKey(
                        curveNID, privKey, peerAltPubKey, new byte[32]));
    }

    @Test
    public void testXDHKeyAgreementOnParams() throws Exception {
        int x25519NID = NativeSunECWrapper.getCurveNID("x25519");
        byte[] x25519PrivKey = new byte[32];
        new SecureRandom().nextBytes(x25519PrivKey);
        byte[] x25519PubKey = new byte[32];
        NativeSunECWrapper.xdhComputePubKey(x25519NID, x25519PrivKey, x25519PubKey);
        byte[] x25519SharedKey = new byte[32];

        int x448NID = NativeSunECWrapper.getCurveNID("x448");
        byte[] x448PrivKey = new byte[56];
        new SecureRandom().nextBytes(x448PrivKey);
        byte[] x448PubKey = new byte[56];
        NativeSunECWrapper.xdhComputePubKey(x448NID, x448PrivKey, x448PubKey);
        byte[] x448SharedKey = new byte[56];

        Assertions.assertThrows(InvalidAlgorithmParameterException.class,
                () -> NativeSunECWrapper.xdhDeriveKey(
                        -1, x25519PrivKey, x25519PubKey, x25519SharedKey));

        Assertions.assertThrows(InvalidKeyException.class,
                () -> NativeSunECWrapper.xdhDeriveKey(
                        x25519NID, x448PrivKey, x25519PubKey, x25519SharedKey));

        Assertions.assertThrows(InvalidKeyException.class,
                () -> NativeSunECWrapper.xdhDeriveKey(
                        x25519NID, x25519PrivKey, x448PubKey, x25519SharedKey));

        Assertions.assertThrows(IllegalStateException.class,
                () -> NativeSunECWrapper.xdhDeriveKey(
                        x25519NID, x25519PrivKey, x25519PubKey, x448SharedKey));
        Assertions.assertThrows(IllegalStateException.class,
                () -> NativeSunECWrapper.xdhDeriveKey(
                        x448NID, x448PrivKey, x448PubKey, x25519SharedKey));
    }

    @Test
    public void testXDHKeyAgreementKAT() throws Exception {
        checkXDHKeyAgreementKAT(
                "X25519",
                "77076D0A7318A57D3C16C17251B26645DF4C2F87EBC0992AB177FBA51DB92C2A",
                "DE9EDB7D7B7DC1B4D35B61C2ECE435373F8343C85B78674DADFC7E146F882B4F",
                "4A5D9D5BA4CE2DE1728E3BF480350F25E07E21C947D19E3376F09B3C1E161742");
        checkXDHKeyAgreementKAT(
                "X25519",
                "5DAB087E624A8A4B79E17F8B83800EE66F3BB1292618B6FD1C2F8B27FF88E0EB",
                "8520F0098930A754748B7DDCB43EF75A0DBF3A0D26381AF4EBA4A98EAA9B4E6A",
                "4a5d9d5ba4ce2de1728e3bf480350f25e07e21c947d19e3376f09b3c1e161742");
        checkXDHKeyAgreementKAT(
                "X448",
                "9A8F4925D1519F5775CF46B04B5800D4EE9EE8BAE8BC5565D498C28DD9C9BAF574A9419744897391006382A6F127AB1D9AC2D8C0A598726B",
                "3EB7A829B0CD20F5BCFC0B599B6FECCF6DA4627107BDB0D4F345B43027D8B972FC3E34FB4232A13CA706DCB57AEC3DAE07BDC1C67BF33609",
                "07FFF4181AC6CC95EC1C16A94A0F74D12DA232CE40A77552281D282BB60C0B56FD2464C335543936521C24403085D59A449A5037514A879D");
        checkXDHKeyAgreementKAT(
                "X448",
                "1C306A7AC2A0E2E0990B294470CBA339E6453772B075811D8FAD0D1D6927C120BB5EE8972B0D3E21374C9C921B09D1B0366F10B65173992D",
                "9B08F7CC31B7E3E67D22D5AEA121074A273BD2B83DE09C63FAA73D2C22C5D9BBC836647241D953D40C5B12DA88120D53177F80E532C41FA0",
                "07fff4181ac6cc95ec1c16a94a0f74d12da232ce40a77552281d282bb60c0b56fd2464c335543936521c24403085d59a449a5037514a879d");
    }

    private void checkXDHKeyAgreementKAT(
            String curve, String privKeyHex, String peerPubKeyHex,
            String expectedSharedKeyHex) throws Exception {
        int curveNID = NativeSunECWrapper.getCurveNID(curve);
        byte[] privKey = HEX.parseHex(privKeyHex);
        byte[] peerPubKey = HEX.parseHex(peerPubKeyHex);
        byte[] expectedSharedKey = HEX.parseHex(expectedSharedKeyHex);

        byte[] sharedKey = new byte[privKey.length];
        NativeSunECWrapper.xdhDeriveKey(curveNID, privKey, peerPubKey, sharedKey);
        Assertions.assertArrayEquals(expectedSharedKey, sharedKey,
                HEX.formatHex(sharedKey));
    }
}
