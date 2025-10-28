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
 * @summary The SUN crypto based on OpenSSL.
 * @modules java.base/sun.security.provider
 * @library /test/lib /test/jdk/openssl
 * @build java.base/sun.security.provider.NativeSunWrapper NativeSunUtil
 * @run junit/othervm -Djdk.sun.enableNativeCrypto=true NativeSunTest
 * @run junit/othervm/policy=native.policy -Djdk.sun.enableNativeCrypto=true NativeSunTest
 */

import org.junit.jupiter.api.Assertions;
import org.junit.jupiter.api.Test;
import sun.security.provider.NativeSunWrapper;

import java.nio.charset.StandardCharsets;
import java.util.HexFormat;

@EnableOnNativeSun
public class NativeSunTest {

    private static final HexFormat HEX = HexFormat.of();

    @Test
    public void testSM3Digest() {
        checkSM3Digest("", "1ab21d8355cfa17f8e61194831e81a8f22bec8c728fefb747ed035eb5082aa2b");
        checkSM3Digest("test", "55e12e91650d2fec56ec74e1d3e4ddbfce2ef3a65890c2a19ecf88a307e76a23");
    }

    private void checkSM3Digest(String message, String expectedDigest) {
        byte[] actual = new byte[32];
        NativeSunWrapper.sm3Digest(message.getBytes(StandardCharsets.UTF_8),
                actual, 0);
        Assertions.assertEquals(expectedDigest, HEX.formatHex(actual));
    }

    @Test
    public void runSM3DigestSerially() throws Exception {
        NativeSunUtil.execTaskSerially(()-> {
            testSM3Digest();
            return null;
        });
    }

    @Test
    public void runSM3DigestParallelly() throws Exception {
        NativeSunUtil.execTaskParallelly(()-> {
            testSM3Digest();
            return null;
        });
    }

    @Test
    public void testSM3Input() {
        byte[] smallOut = new byte[32];
        Assertions.assertThrows(NullPointerException.class,
                () -> NativeSunWrapper.sm3Digest(null, smallOut, 0));
    }

    @Test
    public void testSM3SmallOutput() {
        byte[] smallOut = new byte[31];
        Assertions.assertThrows(IndexOutOfBoundsException.class,
                () -> NativeSunWrapper.sm3Digest("".getBytes(), smallOut, 0));
    }

    @Test
    public void testSM3BigOutput() {
        byte[] bigOut = new byte[33];
        NativeSunWrapper.sm3Digest("test".getBytes(), bigOut, 1);
        Assertions.assertEquals(
                // The first byte is 0x00
                "0055e12e91650d2fec56ec74e1d3e4ddbfce2ef3a65890c2a19ecf88a307e76a23",
                HEX.formatHex(bigOut));
    }
}
