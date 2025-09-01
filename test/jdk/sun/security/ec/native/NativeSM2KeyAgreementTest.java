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
 * @summary The SM2 key agreement based on OpenSSL.
 * @library /test/lib /test/jdk/openssl
 * @run junit/othervm -Djdk.sunec.enableNativeCrypto=true NativeSM2KeyAgreementTest
 * @run junit/othervm/policy=native.policy -Djdk.sunec.enableNativeCrypto=true NativeSM2KeyAgreementTest
 */

import org.junit.jupiter.api.Assertions;
import org.junit.jupiter.api.Test;

import javax.crypto.KeyAgreement;
import javax.crypto.SecretKey;
import java.nio.charset.StandardCharsets;
import java.security.KeyPair;
import java.security.KeyPairGenerator;
import java.security.interfaces.ECPrivateKey;
import java.security.interfaces.ECPublicKey;
import java.security.spec.ECGenParameterSpec;
import java.security.spec.SM2KeyAgreementParamSpec;

@EnableOnNativeSunEC
public class NativeSM2KeyAgreementTest {

    private static final byte[] ID_A = "ID_A".getBytes(StandardCharsets.UTF_8);
    private static final byte[] ID_B = "ID_B".getBytes(StandardCharsets.UTF_8);

    @Test
    public void testKeyAgreement() throws Exception {
        KeyPairGenerator keyPairGen = KeyPairGenerator.getInstance("EC");
        keyPairGen.initialize(new ECGenParameterSpec("curveSM2"));

        KeyPair keyPairA = keyPairGen.generateKeyPair();
        KeyPair keyPairTempA = keyPairGen.generateKeyPair();

        KeyPair keyPairB = keyPairGen.generateKeyPair();
        KeyPair keyPairTempB = keyPairGen.generateKeyPair();

        SM2KeyAgreementParamSpec paramSpecA = new SM2KeyAgreementParamSpec(
                ID_A,
                (ECPrivateKey) keyPairA.getPrivate(),
                (ECPublicKey) keyPairA.getPublic(),
                ID_B,
                (ECPublicKey) keyPairB.getPublic(),
                true,
                32);
        KeyAgreement keyAgreementA = KeyAgreement.getInstance("SM2");
        keyAgreementA.init(keyPairTempA.getPrivate(), paramSpecA);
        keyAgreementA.doPhase(keyPairTempB.getPublic(), true);
        SecretKey sharedKeyA = keyAgreementA.generateSecret("SM2SharedSecret");

        SM2KeyAgreementParamSpec paramSpecB = new SM2KeyAgreementParamSpec(
                ID_B,
                (ECPrivateKey) keyPairB.getPrivate(),
                (ECPublicKey) keyPairB.getPublic(),
                ID_A,
                (ECPublicKey) keyPairA.getPublic(),
                false,
                32);
        KeyAgreement keyAgreementB = KeyAgreement.getInstance("SM2");
        keyAgreementB.init(keyPairTempB.getPrivate(), paramSpecB);
        keyAgreementB.doPhase(keyPairTempA.getPublic(), true);
        SecretKey sharedKeyB = keyAgreementB.generateSecret("SM2SharedSecret");

        Assertions.assertArrayEquals(sharedKeyA.getEncoded(), sharedKeyB.getEncoded());
    }
}
