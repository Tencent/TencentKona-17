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
 * @summary The RSA signature based on OpenSSL.
 * @modules java.base/sun.security.rsa
 * @library /test/lib /test/jdk/openssl
 * @run junit/othervm NativeRSASignatureTest
 * @run junit/othervm/policy=test.policy NativeRSASignatureTest
 * @run junit/othervm -Djdk.sunrsasign.enableNativeCrypto=true NativeRSASignatureTest
 * @run junit/othervm/policy=test.policy -Djdk.sunrsasign.enableNativeCrypto=true NativeRSASignatureTest
 */

import org.junit.jupiter.api.Assertions;
import org.junit.jupiter.api.Test;

import java.nio.charset.StandardCharsets;
import java.security.KeyPair;
import java.security.KeyPairGenerator;
import java.security.Signature;
import java.security.spec.RSAKeyGenParameterSpec;

@EnableOnNativeSunRsaSign
public class NativeRSASignatureTest {

    private static final byte[] MESSAGE = "test".getBytes(StandardCharsets.UTF_8);

    @Test
    public void testRSASignature() throws Exception {
        checkRSASignature("SHA1withRSA", 1024);
        checkRSASignature("SHA1withRSA", 2048);
        checkRSASignature("SHA1withRSA", 3072);
        checkRSASignature("SHA1withRSA", 4096);

        checkRSASignature("SHA256withRSA", 1024);
        checkRSASignature("SHA256withRSA", 2048);
        checkRSASignature("SHA256withRSA", 3072);
        checkRSASignature("SHA256withRSA", 4096);

        checkRSASignature("SHA3-256withRSA", 1024);
        checkRSASignature("SHA3-256withRSA", 2048);
        checkRSASignature("SHA3-256withRSA", 3072);
        checkRSASignature("SHA3-256withRSA", 4096);
    }

    private void checkRSASignature(String algo, int keySize)
            throws Exception {
        System.out.println("algo=" + algo + ", keySize=" + keySize);

        KeyPairGenerator keyPairGen = KeyPairGenerator.getInstance("RSA");
        keyPairGen.initialize(new RSAKeyGenParameterSpec(keySize,
                RSAKeyGenParameterSpec.F4));
        KeyPair keyPair = keyPairGen.generateKeyPair();

        Signature signer = Signature.getInstance(algo);
        signer.initSign(keyPair.getPrivate());
        signer.update(MESSAGE);
        byte[] signature = signer.sign();

        Signature verifier = Signature.getInstance(algo);
        verifier.initVerify(keyPair.getPublic());
        verifier.update(MESSAGE);
        boolean verified = verifier.verify(signature);

        Assertions.assertTrue(verified);
    }
}
