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
 * @summary The RSASSA-PSS signature based on OpenSSL.
 * @library /test/lib /test/jdk/openssl
 * @run junit/othervm NativeRSAPSSSignatureTest
 * @run junit/othervm/policy=test.policy NativeRSAPSSSignatureTest
 * @run junit/othervm -Djdk.sunrsasign.enableNativeCrypto=true NativeRSAPSSSignatureTest
 * @run junit/othervm/policy=test.policy -Djdk.sunrsasign.enableNativeCrypto=true NativeRSAPSSSignatureTest
 */

import org.junit.jupiter.api.Assertions;
import org.junit.jupiter.api.Test;

import java.nio.charset.StandardCharsets;
import java.security.KeyPair;
import java.security.KeyPairGenerator;
import java.security.Signature;
import java.security.spec.MGF1ParameterSpec;
import java.security.spec.PSSParameterSpec;
import java.security.spec.RSAKeyGenParameterSpec;

@EnableOnNativeSunRsaSign
public class NativeRSAPSSSignatureTest {

    private static final byte[] MESSAGE = "test".getBytes(StandardCharsets.UTF_8);

    @Test
    public void testRSAPSSSignature() throws Exception {
        checkRSAPSSSignature(1024, "SHA1");
        checkRSAPSSSignature(2048, "SHA1");
        checkRSAPSSSignature(3072, "SHA1");
        checkRSAPSSSignature(4096, "SHA1");

        checkRSAPSSSignature(1024, "SHA256");
        checkRSAPSSSignature(2048, "SHA256");
        checkRSAPSSSignature(3072, "SHA256");
        checkRSAPSSSignature(4096, "SHA256");

        checkRSAPSSSignature(1024, "SHA3-256");
        checkRSAPSSSignature(2048, "SHA3-256");
        checkRSAPSSSignature(3072, "SHA3-256");
        checkRSAPSSSignature(4096, "SHA3-256");
    }

    private void checkRSAPSSSignature(int keySize, String hashAlgo)
            throws Exception {
        System.out.println("keySize=" + keySize + ", hashAlgo=" + hashAlgo);

        KeyPairGenerator keyPairGen = KeyPairGenerator.getInstance("RSA");
        keyPairGen.initialize(new RSAKeyGenParameterSpec(keySize,
                RSAKeyGenParameterSpec.F4));
        KeyPair keyPair = keyPairGen.generateKeyPair();

        PSSParameterSpec pssSpec = new PSSParameterSpec(
                hashAlgo, "MGF1", new MGF1ParameterSpec(hashAlgo), 32, 1);

        Signature signer = Signature.getInstance("RSASSA-PSS");
        signer.setParameter(pssSpec);
        signer.initSign(keyPair.getPrivate());
        signer.update(MESSAGE);
        byte[] signature = signer.sign();

        Signature verifier = Signature.getInstance("RSASSA-PSS");
        verifier.setParameter(pssSpec);
        verifier.initVerify(keyPair.getPublic());
        verifier.update(MESSAGE);
        boolean verified = verifier.verify(signature);

        Assertions.assertTrue(verified);
    }
}
