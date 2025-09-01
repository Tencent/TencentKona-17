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
 * @run junit/othervm NativeXDHKeyPairGeneratorTest
 * @run junit/othervm/policy=test.policy NativeXDHKeyPairGeneratorTest
 * @run junit/othervm -Djdk.sunec.enableNativeCrypto=true NativeXDHKeyPairGeneratorTest
 * @run junit/othervm/policy=test.policy -Djdk.sunec.enableNativeCrypto=true NativeXDHKeyPairGeneratorTest
 */

import org.junit.jupiter.api.Assertions;
import org.junit.jupiter.api.Test;

import java.security.InvalidAlgorithmParameterException;
import java.security.KeyPair;
import java.security.KeyPairGenerator;
import java.security.interfaces.XECPrivateKey;
import java.security.spec.NamedParameterSpec;
import java.util.Arrays;

@EnableOnNativeSunEC
public class NativeXDHKeyPairGeneratorTest {

    @Test
    public void testGenKeyPair() throws Exception {
        checkGenKeyPair("X25519", 32);
        checkGenKeyPair("X448", 56);
    }

    @Test
    public void testGenKeyPairWithUnsupportedCurve() {
        Assertions.assertThrows(InvalidAlgorithmParameterException.class,
                ()-> checkGenKeyPair("unknown", 32));
    }

    private void checkGenKeyPair(String curve, int keyLength) throws Exception {
        System.out.println(curve + " with " + keyLength);

        KeyPairGenerator keyPairGen = KeyPairGenerator.getInstance("XDH");
        keyPairGen.initialize(new NamedParameterSpec(curve));
        KeyPair keyPair = keyPairGen.generateKeyPair();

        XECPrivateKey privKey = (XECPrivateKey) keyPair.getPrivate();
        byte[] sArray = privKey.getScalar().orElse(new byte[0]);

        Assertions.assertTrue(sArray.length <= keyLength
                || (sArray.length == (keyLength + 1) && sArray[0] == 0),
                Arrays.toString(sArray));
    }
}
