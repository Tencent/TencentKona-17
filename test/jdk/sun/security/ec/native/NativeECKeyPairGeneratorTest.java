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
 * @modules jdk.crypto.ec/sun.security.ec
 * @library /test/lib
 * @run junit/othervm NativeECKeyPairGeneratorTest
 * @run junit/othervm/policy=test.policy NativeECKeyPairGeneratorTest
 * @run junit/othervm -Djdk.sunec.enableNativeCrypto=true NativeECKeyPairGeneratorTest
 * @run junit/othervm/policy=test.policy -Djdk.sunec.enableNativeCrypto=true NativeECKeyPairGeneratorTest
 */

import java.security.InvalidAlgorithmParameterException;
import java.security.KeyPair;
import java.security.KeyPairGenerator;
import java.security.interfaces.ECPrivateKey;
import java.security.spec.ECGenParameterSpec;
import java.util.Arrays;

import org.junit.jupiter.api.Assertions;
import org.junit.jupiter.api.Test;

@EnableOnNativeCrypto
public class NativeECKeyPairGeneratorTest {

    @Test
    public void testGenKeyPair() throws Exception {
        checkGenKeyPair("secp256r1", 32);
        checkGenKeyPair("secp384r1", 48);
        checkGenKeyPair("secp521r1", 66);
        checkGenKeyPair("curvesm2", 32);
    }

    @Test
    public void testGenKeyPairWithUnsupportedCurve() {
        Assertions.assertThrows(InvalidAlgorithmParameterException.class,
                ()-> checkGenKeyPair("unknown", 256));
    }

    private void checkGenKeyPair(String curve, int keyLength) throws Exception {
        System.out.println(curve + " with " + keyLength);

        KeyPairGenerator keyPairGen = KeyPairGenerator.getInstance("EC");
        keyPairGen.initialize(new ECGenParameterSpec(curve));
        KeyPair keyPair = keyPairGen.generateKeyPair();

        ECPrivateKey privKey = (ECPrivateKey) keyPair.getPrivate();
        byte[] sArray = privKey.getS().toByteArray();
        Assertions.assertTrue(
                sArray.length <= keyLength || (sArray.length == (keyLength + 1) && sArray[0] == 0),
                Arrays.toString(sArray));
    }
}
