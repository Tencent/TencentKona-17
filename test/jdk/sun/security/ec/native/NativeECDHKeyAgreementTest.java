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

/*
 * @test
 * @summary The EC key pair generator based on OpenSSL.
 * @library /test/lib /test/jdk/openssl
 * @run junit/othervm NativeECDHKeyAgreementTest
 * @run junit/othervm/policy=test.policy NativeECDHKeyAgreementTest
 * @run junit/othervm -Djdk.sunec.enableNativeCrypto=true NativeECDHKeyAgreementTest
 * @run junit/othervm/policy=test.policy -Djdk.sunec.enableNativeCrypto=true NativeECDHKeyAgreementTest
 */

import java.security.KeyPair;
import java.security.KeyPairGenerator;
import java.security.spec.ECGenParameterSpec;

import org.junit.jupiter.api.Assertions;
import org.junit.jupiter.api.Test;

import javax.crypto.KeyAgreement;

@EnableOnNativeSunEC
public class NativeECDHKeyAgreementTest {

    @Test
    public void testKeyAgreement() throws Exception {
        checkKeyAgreement("secp256r1");
        checkKeyAgreement("secp384r1");
        checkKeyAgreement("secp521r1");
        checkKeyAgreement("curvesm2");
    }

    private void checkKeyAgreement(String curve) throws Exception {
        System.out.println(curve);

        KeyPairGenerator keyPairGen = KeyPairGenerator.getInstance("EC");
        keyPairGen.initialize(new ECGenParameterSpec(curve));
        KeyPair keyPair = keyPairGen.generateKeyPair();
        KeyPair peerKeyPair = keyPairGen.generateKeyPair();

        KeyAgreement keyAgreement = KeyAgreement.getInstance("ECDH");
        keyAgreement.init(keyPair.getPrivate());
        keyAgreement.doPhase(peerKeyPair.getPublic(), true);
        byte[] sharedKey = keyAgreement.generateSecret();

        keyAgreement.init(peerKeyPair.getPrivate());
        keyAgreement.doPhase(keyPair.getPublic(), true);
        byte[] peerSharedKey = keyAgreement.generateSecret();

        Assertions.assertArrayEquals(sharedKey, peerSharedKey);
    }
}
