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
 * @summary The EC key pair generator based on OpenSSL.
 * @library /test/lib /test/jdk/openssl
 * @run junit/othervm NativeXDHKeyAgreementTest
 * @run junit/othervm/policy=test.policy NativeXDHKeyAgreementTest
 * @run junit/othervm -Djdk.sunec.enableNativeCrypto=true NativeXDHKeyAgreementTest
 * @run junit/othervm/policy=test.policy -Djdk.sunec.enableNativeCrypto=true NativeXDHKeyAgreementTest
 */

import org.junit.jupiter.api.Assertions;
import org.junit.jupiter.api.Test;

import javax.crypto.KeyAgreement;
import java.security.KeyPair;
import java.security.KeyPairGenerator;
import java.security.spec.ECGenParameterSpec;

@EnableOnNativeSunEC
public class NativeXDHKeyAgreementTest {

    @Test
    public void testKeyAgreement() throws Exception {
        checkKeyAgreement("x25519");
        checkKeyAgreement("x448");
    }

    private void checkKeyAgreement(String curve) throws Exception {
        System.out.println(curve);

        KeyPairGenerator keyPairGen = KeyPairGenerator.getInstance("XDH");
        keyPairGen.initialize(new ECGenParameterSpec(curve));
        KeyPair keyPair = keyPairGen.generateKeyPair();
        KeyPair peerKeyPair = keyPairGen.generateKeyPair();

        KeyAgreement keyAgreement = KeyAgreement.getInstance("XDH");
        keyAgreement.init(keyPair.getPrivate());
        keyAgreement.doPhase(peerKeyPair.getPublic(), true);
        byte[] sharedKey = keyAgreement.generateSecret();

        keyAgreement.init(peerKeyPair.getPrivate());
        keyAgreement.doPhase(keyPair.getPublic(), true);
        byte[] peerSharedKey = keyAgreement.generateSecret();

        Assertions.assertArrayEquals(sharedKey, peerSharedKey);
    }
}
