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
 * @summary The SM2 signature based on OpenSSL.
 * @library /test/lib /test/jdk/openssl
 * @run junit/othervm -Djdk.sunec.enableNativeCrypto=true NativeSM2SignatureTest
 * @run junit/othervm/policy=native.policy -Djdk.sunec.enableNativeCrypto=true NativeSM2SignatureTest
 */

import org.junit.jupiter.api.Assertions;
import org.junit.jupiter.api.Test;

import java.nio.charset.StandardCharsets;
import java.security.KeyPair;
import java.security.KeyPairGenerator;
import java.security.Signature;
import java.security.interfaces.ECPublicKey;
import java.security.spec.ECGenParameterSpec;
import java.security.spec.SM2SignatureParameterSpec;

@EnableOnNativeSunEC
public class NativeSM2SignatureTest {

    private static final byte[] MESSAGE = "test".getBytes(StandardCharsets.UTF_8);

    @Test
    public void testSignature() throws Exception {
        KeyPairGenerator keyPairGen = KeyPairGenerator.getInstance("EC");
        keyPairGen.initialize(new ECGenParameterSpec("curveSM2"));
        KeyPair keyPair = keyPairGen.generateKeyPair();

        Signature signature = Signature.getInstance("SM2");
        signature.initSign(keyPair.getPrivate());
        signature.update(MESSAGE);
        byte[] sig = signature.sign();

        signature.initVerify(keyPair.getPublic());
        signature.update(MESSAGE);
        boolean verified = signature.verify(sig);

        Assertions.assertTrue(verified);
    }

    @Test
    public void testSignatureWithPubKey() throws Exception {
        KeyPairGenerator keyPairGen = KeyPairGenerator.getInstance("EC");
        keyPairGen.initialize(new ECGenParameterSpec("curveSM2"));
        KeyPair keyPair = keyPairGen.generateKeyPair();

        Signature signature = Signature.getInstance("SM2");
        // Set public key explicitly
        signature.setParameter(new SM2SignatureParameterSpec(
                (ECPublicKey) keyPair.getPublic()));
        signature.initSign(keyPair.getPrivate());
        signature.update(MESSAGE);
        byte[] sig = signature.sign();

        signature.initVerify(keyPair.getPublic());
        signature.update(MESSAGE);
        boolean verified = signature.verify(sig);

        Assertions.assertTrue(verified);
    }
}
