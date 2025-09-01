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
 * @run junit/othervm NativeSM3Test
 * @run junit/othervm/policy=test.policy NativeSM3Test
 * @run junit/othervm -Djdk.sun.enableNativeCrypto=true NativeSM3Test
 * @run junit/othervm/policy=test.policy -Djdk.sun.enableNativeCrypto=true NativeSM3Test
 */

import org.junit.jupiter.api.Assertions;
import org.junit.jupiter.api.Test;

import java.nio.charset.StandardCharsets;
import java.security.MessageDigest;
import java.util.HexFormat;

@EnableOnNativeSun
public class NativeSM3Test {

    private static final HexFormat HEX = HexFormat.of();

    @Test
    public void testDigest() throws Exception {
        testDigest("", "1ab21d8355cfa17f8e61194831e81a8f22bec8c728fefb747ed035eb5082aa2b");
        testDigest("test", "55e12e91650d2fec56ec74e1d3e4ddbfce2ef3a65890c2a19ecf88a307e76a23");
    }

    private void testDigest(String message, String expectedDigest) throws Exception {
        MessageDigest md = MessageDigest.getInstance("SM3");
        byte[] digest = md.digest(message.getBytes(StandardCharsets.UTF_8));
        Assertions.assertEquals(expectedDigest, HEX.formatHex(digest));
    }
}
