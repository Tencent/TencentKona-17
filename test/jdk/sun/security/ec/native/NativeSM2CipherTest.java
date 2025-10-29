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
 * @summary The SM2 cipher based on OpenSSL.
 * @library /test/lib /test/jdk/openssl
 * @run junit/othervm -Djdk.sunec.enableNativeCrypto=true NativeSM2CipherTest
 * @run junit/othervm/policy=native.policy -Djdk.sunec.enableNativeCrypto=true NativeSM2CipherTest
 */

import org.junit.jupiter.api.Assertions;
import org.junit.jupiter.api.Test;

import javax.crypto.Cipher;
import java.nio.charset.StandardCharsets;
import java.security.KeyPair;
import java.security.KeyPairGenerator;
import java.security.spec.ECGenParameterSpec;

@EnableOnNativeSunEC
public class NativeSM2CipherTest {

    private static final byte[] MESSAGE = "test".getBytes(StandardCharsets.UTF_8);

    @Test
    public void testCipher() throws Exception {
        KeyPairGenerator keyPairGen = KeyPairGenerator.getInstance("EC");
        keyPairGen.initialize(new ECGenParameterSpec("curveSM2"));
        KeyPair keyPair = keyPairGen.generateKeyPair();

        Cipher cipher = Cipher.getInstance("SM2");
        cipher.init(Cipher.ENCRYPT_MODE, keyPair.getPublic());
        byte[] ciphertext = cipher.doFinal(MESSAGE);

        cipher.init(Cipher.DECRYPT_MODE, keyPair.getPrivate());
        byte[] cleartext = cipher.doFinal(ciphertext);

        Assertions.assertArrayEquals(MESSAGE, cleartext);
    }
}
