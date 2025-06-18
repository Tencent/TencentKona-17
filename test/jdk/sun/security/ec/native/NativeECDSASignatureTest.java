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
 * @summary The ECDSA signature based on OpenSSL.
 * @modules jdk.crypto.ec/sun.security.ec
 * @library /test/lib /test/jdk/openssl
 * @run junit/othervm NativeECDSASignatureTest
 * @run junit/othervm/policy=test.policy NativeECDSASignatureTest
 * @run junit/othervm -Djdk.sunec.enableNativeCrypto=true NativeECDSASignatureTest
 * @run junit/othervm/policy=test.policy -Djdk.sunec.enableNativeCrypto=true NativeECDSASignatureTest
 */

import org.junit.jupiter.api.Assertions;
import org.junit.jupiter.api.Test;

import java.math.BigInteger;
import java.nio.charset.StandardCharsets;
import java.security.*;
import java.security.spec.*;
import java.util.HexFormat;

@EnableOnNativeEC
public class NativeECDSASignatureTest {

    private static final HexFormat HEX = HexFormat.of();
    private static final byte[] MESSAGE = "test".getBytes(StandardCharsets.UTF_8);

    @Test
    public void testKAT() throws Exception {
        checkKAT("secp256r1",
                "C477F9F65C22CCE20657FAA5B2D1D8122336F851A508A1ED04E479C34985BF96",
                "B7E08AFDFE94BAD3F1DC8C734798BA1C62B3A0AD1E9EA2A38201CD0889BC7A19",
                "3603F747959DBF7A4BB226E41928729063ADC7AE43529E61B563BBC606CC5E09",
                "SHA256withECDSAinP1363Format",
                "Example of ECDSA with P-256",
                "2B42F576D07F4165FF65D1F3B1500F81E44C316F1F0B3EF57325B69ACA46104F",
                "DC42C2122D6392CD3E3A993A89502A8198C1886FE69D262C4B329BDB6B63FAF1");
        checkKAT("secp256r1",
                "C477F9F65C22CCE20657FAA5B2D1D8122336F851A508A1ED04E479C34985BF96",
                "B7E08AFDFE94BAD3F1DC8C734798BA1C62B3A0AD1E9EA2A38201CD0889BC7A19",
                "3603F747959DBF7A4BB226E41928729063ADC7AE43529E61B563BBC606CC5E09",
                "SHA3-256withECDSAinP1363Format",
                "Example of ECDSA with P-256",
                "2B42F576D07F4165FF65D1F3B1500F81E44C316F1F0B3EF57325B69ACA46104F",
                "0A861C2526900245C73BACB9ADAEC1A5ACB3BA1F7114A3C334FDCD5B7690DADD");

        checkKAT("secp384r1",
                "F92C02ED629E4B48C0584B1C6CE3A3E3B4FAAE4AFC6ACB0455E73DFC392E6A0AE393A8565E6B9714D1224B57D83F8A08",
                "3BF701BC9E9D36B4D5F1455343F09126F2564390F2B487365071243C61E6471FB9D2AB74657B82F9086489D9EF0F5CB5",
                "D1A358EAFBF952E68D533855CCBDAA6FF75B137A5101443199325583552A6295FFE5382D00CFCDA30344A9B5B68DB855",
                "SHA384withECDSAinP1363Format",
                "Example of ECDSA with P-384",
                "30EA514FC0D38D8208756F068113C7CADA9F66A3B40EA3B313D040D9B57DD41A332795D02CC7D507FCEF9FAF01A27088",
                "CC808E504BE414F46C9027BCBF78ADF067A43922D6FCAA66C4476875FBB7B94EFD1F7D5DBE620BFB821C46D549683AD8");
        checkKAT("secp384r1",
                "F92C02ED629E4B48C0584B1C6CE3A3E3B4FAAE4AFC6ACB0455E73DFC392E6A0AE393A8565E6B9714D1224B57D83F8A08",
                "3BF701BC9E9D36B4D5F1455343F09126F2564390F2B487365071243C61E6471FB9D2AB74657B82F9086489D9EF0F5CB5",
                "D1A358EAFBF952E68D533855CCBDAA6FF75B137A5101443199325583552A6295FFE5382D00CFCDA30344A9B5B68DB855",
                "SHA3-384withECDSAinP1363Format",
                "Example of ECDSA with P-384",
                "30EA514FC0D38D8208756F068113C7CADA9F66A3B40EA3B313D040D9B57DD41A332795D02CC7D507FCEF9FAF01A27088",
                "691B9D4969451A98036D53AA725458602125DE74881BBC333012CA4FA55BDE39D1BF16A6AAE3FE4992C567C6E7892337");

        checkKAT("secp521r1",
                "0100085F47B8E1B8B11B7EB33028C0B2888E304BFC98501955B45BBA1478DC184EEEDF09B86A5F7C21994406072787205E69A63709FE35AA93BA333514B24F961722",
                "98E91EEF9A68452822309C52FAB453F5F117C1DA8ED796B255E9AB8F6410CCA16E59DF403A6BDC6CA467A37056B1E54B3005D8AC030DECFEB68DF18B171885D5C4",
                "0164350C321AECFC1CCA1BA4364C9B15656150B4B78D6A48D7D28E7F31985EF17BE8554376B72900712C4B83AD668327231526E313F5F092999A4632FD50D946BC2E",
                "SHA512withECDSAinP1363Format",
                "Example of ECDSA with P-521",
                "0140C8EDCA57108CE3F7E7A240DDD3AD74D81E2DE62451FC1D558FDC79269ADACD1C2526EEEEF32F8C0432A9D56E2B4A8A732891C37C9B96641A9254CCFE5DC3E2BA",
                "00D72F15229D0096376DA6651D9985BFD7C07F8D49583B545DB3EAB20E0A2C1E8615BD9E298455BDEB6B61378E77AF1C54EEE2CE37B2C61F5C9A8232951CB988B5B1");
        checkKAT("secp521r1",
                "0100085F47B8E1B8B11B7EB33028C0B2888E304BFC98501955B45BBA1478DC184EEEDF09B86A5F7C21994406072787205E69A63709FE35AA93BA333514B24F961722",
                "0098E91EEF9A68452822309C52FAB453F5F117C1DA8ED796B255E9AB8F6410CCA16E59DF403A6BDC6CA467A37056B1E54B3005D8AC030DECFEB68DF18B171885D5C4",
                "0164350C321AECFC1CCA1BA4364C9B15656150B4B78D6A48D7D28E7F31985EF17BE8554376B72900712C4B83AD668327231526E313F5F092999A4632FD50D946BC2E",
                "SHA3-512withECDSAinP1363Format",
                "Example of ECDSA with P-521",
                "0140C8EDCA57108CE3F7E7A240DDD3AD74D81E2DE62451FC1D558FDC79269ADACD1C2526EEEEF32F8C0432A9D56E2B4A8A732891C37C9B96641A9254CCFE5DC3E2BA",
                "00B25188492D58E808EDEBD7BF440ED20DB771CA7C618595D5398E1B1C0098E300D8C803EC69EC5F46C84FC61967A302D366C627FCFA56F87F241EF921B6E627ADBF");
    }

    private void checkKAT(
            String curve, String privKeyHex, String pubXHex, String pubYHex,
            String algo, String message, String expRHex, String expSHex)
            throws Exception {
        System.out.println(algo + " with " + curve);

        byte[] messageBytes = message.getBytes();
        KeyPair keyPair = keyPair(curve, privKeyHex, pubXHex, pubYHex);

        Signature signer = Signature.getInstance(algo);
        signer.initSign(keyPair.getPrivate());
        signer.update(messageBytes);
        byte[] sig = signer.sign();

        Signature verifier = Signature.getInstance(algo);
        verifier.initVerify(keyPair.getPublic());
        verifier.update(messageBytes);
        Assertions.assertTrue(verifier.verify(sig));

        verifier.update(messageBytes);
        Assertions.assertTrue(verifier.verify(HEX.parseHex(expRHex + expSHex)));
    }

    @Test
    public void testECDSASignature() throws Exception {
        checkECDSASignature("SHA1withECDSA", "secp256r1", false);
        checkECDSASignature("SHA256withECDSA", "secp256r1", false);
        checkECDSASignature("SHA384withECDSA", "secp256r1", false);
        checkECDSASignature("SHA512withECDSA", "secp256r1", false);

        checkECDSASignature("SHA1withECDSA", "secp384r1", false);
        checkECDSASignature("SHA256withECDSA", "secp384r1", false);
        checkECDSASignature("SHA384withECDSA", "secp384r1", false);
        checkECDSASignature("SHA512withECDSA", "secp384r1", false);

        checkECDSASignature("SHA1withECDSA", "secp521r1", false);
        checkECDSASignature("SHA256withECDSA", "secp521r1", false);
        checkECDSASignature("SHA384withECDSA", "secp521r1", false);
        checkECDSASignature("SHA512withECDSA", "secp521r1", false);
    }

    @Test
    public void testECDSASignatureInP1363Format() throws Exception {
        checkECDSASignature("SHA1withECDSA", "secp256r1", true);
        checkECDSASignature("SHA256withECDSA", "secp256r1", true);
        checkECDSASignature("SHA384withECDSA", "secp256r1", true);
        checkECDSASignature("SHA512withECDSA", "secp256r1", true);

        checkECDSASignature("SHA1withECDSA", "secp384r1", true);
        checkECDSASignature("SHA256withECDSA", "secp384r1", true);
        checkECDSASignature("SHA384withECDSA", "secp384r1", true);
        checkECDSASignature("SHA512withECDSA", "secp384r1", true);

        checkECDSASignature("SHA1withECDSA", "secp521r1", true);
        checkECDSASignature("SHA256withECDSA", "secp521r1", true);
        checkECDSASignature("SHA384withECDSA", "secp521r1", true);
        checkECDSASignature("SHA512withECDSA", "secp521r1", true);
    }

    private void checkECDSASignature(String algo, String curve, boolean isP1363)
            throws Exception {
        System.out.println("algo=" + algo + ", curve=" + curve
                + ", isP1363=" + isP1363);

        String algorithm = isP1363 ? algo + "inP1363Format" : algo;
        KeyPairGenerator keyPairGen = KeyPairGenerator.getInstance("EC");
        keyPairGen.initialize(new ECGenParameterSpec(curve));
        KeyPair keyPair = keyPairGen.generateKeyPair();

        Signature signer = Signature.getInstance(algorithm);
        signer.initSign(keyPair.getPrivate());
        signer.update(MESSAGE);
        byte[] signature = signer.sign();

        Signature verifier = Signature.getInstance(algorithm);
        verifier.initVerify(keyPair.getPublic());
        verifier.update(MESSAGE);
        boolean verified = verifier.verify(signature);

        Assertions.assertTrue(verified);
    }

    private KeyPair keyPair(String curve, String privKeyHex,
            String pubXHex, String pubYHex) throws Exception {
        AlgorithmParameters params = AlgorithmParameters.getInstance("EC");
        params.init(new ECGenParameterSpec(curve));
        ECParameterSpec ecParams = params.getParameterSpec(ECParameterSpec.class);

        KeyFactory kf = KeyFactory.getInstance("EC");
        PrivateKey privKey = kf.generatePrivate(
                new ECPrivateKeySpec(toBigInt(privKeyHex), ecParams));

        ECPublicKeySpec pubKeySpec = new ECPublicKeySpec(
                new ECPoint(toBigInt(pubXHex), toBigInt(pubYHex)), ecParams);
        PublicKey pubKey = kf.generatePublic(pubKeySpec);

        return new KeyPair(pubKey, privKey);
    }

    private static BigInteger toBigInt(String hex) {
        byte[] bytes = HEX.parseHex(hex);
        return new BigInteger(1, bytes);
    }
}
