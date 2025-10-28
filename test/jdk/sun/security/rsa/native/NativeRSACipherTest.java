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
 * @summary The RSA cipher based on OpenSSL.
 * @library /test/lib /test/jdk/openssl
 * @run junit/othervm NativeRSACipherTest
 * @run junit/othervm/policy=test.policy NativeRSACipherTest
 * @run junit/othervm -Djdk.sunrsasign.enableNativeCrypto=true NativeRSACipherTest
 * @run junit/othervm/policy=test.policy -Djdk.sunrsasign.enableNativeCrypto=true NativeRSACipherTest
 */

import org.junit.jupiter.api.Assertions;
import org.junit.jupiter.api.Test;

import javax.crypto.Cipher;
import java.nio.charset.StandardCharsets;
import java.security.KeyPair;
import java.security.KeyPairGenerator;
import java.security.spec.RSAKeyGenParameterSpec;

@EnableOnNativeSunRsaSign
public class NativeRSACipherTest {

    private static final byte[] MESSAGE = "test".getBytes(StandardCharsets.UTF_8);

    @Test
    public void testRSACipher() throws Exception {
        checkRSACipher("RSA", 1024);
        checkRSACipher("RSA", 2048);
        checkRSACipher("RSA", 3072);
        checkRSACipher("RSA", 4096);

        checkRSACipher("RSA/ECB/PKCS1Padding", 1024);
        checkRSACipher("RSA/ECB/PKCS1Padding", 2048);
        checkRSACipher("RSA/ECB/PKCS1Padding", 3072);
        checkRSACipher("RSA/ECB/PKCS1Padding", 4096);
    }

    private void checkRSACipher(String algo, int keySize)
            throws Exception {
        System.out.println("algo=" + algo + ", keySize=" + keySize);

        KeyPairGenerator keyPairGen = KeyPairGenerator.getInstance("RSA");
        keyPairGen.initialize(new RSAKeyGenParameterSpec(keySize,
                RSAKeyGenParameterSpec.F4));
        KeyPair keyPair = keyPairGen.generateKeyPair();

        Cipher encrypter = Cipher.getInstance(algo);
        encrypter.init(Cipher.ENCRYPT_MODE, keyPair.getPublic());
        encrypter.update(MESSAGE);
        byte[] ciphertext = encrypter.doFinal();

        Cipher decrypter = Cipher.getInstance(algo);
        decrypter.init(Cipher.DECRYPT_MODE, keyPair.getPrivate());
        decrypter.update(ciphertext);
        byte[] cleartext = decrypter.doFinal();

        Assertions.assertArrayEquals(MESSAGE, cleartext);
    }
}
