/*
 * Copyright (C) 2025, 2026, Tencent. All rights reserved.
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
 * @summary Native SM3 streaming interface backed by OpenSSL EVP_MD_CTX.
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

    // Known-good SM3 digests (GB/T 32905-2016 test vectors).
    private static final String DIGEST_EMPTY =
            "1ab21d8355cfa17f8e61194831e81a8f22bec8c728fefb747ed035eb5082aa2b";
    private static final String DIGEST_TEST =
            "55e12e91650d2fec56ec74e1d3e4ddbfce2ef3a65890c2a19ecf88a307e76a23";

    // -------------------------------------------------------------------
    // Helpers
    // -------------------------------------------------------------------

    private static byte[] digest(byte[] message) {
        long ctx = NativeSunWrapper.sm3Init();
        try {
            NativeSunWrapper.sm3Update(ctx, message, 0, message.length);
            byte[] out = new byte[32];
            NativeSunWrapper.sm3DoFinal(ctx, out, 0);
            return out;
        } finally {
            NativeSunWrapper.sm3Free(ctx);
        }
    }

    // -------------------------------------------------------------------
    // Functional tests
    // -------------------------------------------------------------------

    @Test
    public void testOneShotEmpty() {
        Assertions.assertEquals(DIGEST_EMPTY,
                HEX.formatHex(digest("".getBytes(StandardCharsets.UTF_8))));
    }

    @Test
    public void testOneShotNonEmpty() {
        Assertions.assertEquals(DIGEST_TEST,
                HEX.formatHex(digest("test".getBytes(StandardCharsets.UTF_8))));
    }

    /** Multiple update calls must produce the same digest as a single one. */
    @Test
    public void testMultiUpdate() {
        byte[] msg = "test".getBytes(StandardCharsets.UTF_8);
        long ctx = NativeSunWrapper.sm3Init();
        try {
            for (byte b : msg) {
                NativeSunWrapper.sm3Update(ctx, new byte[]{b}, 0, 1);
            }
            byte[] out = new byte[32];
            NativeSunWrapper.sm3DoFinal(ctx, out, 0);
            Assertions.assertEquals(DIGEST_TEST, HEX.formatHex(out));
        } finally {
            NativeSunWrapper.sm3Free(ctx);
        }
    }

    /** doFinal resets the context; a second doFinal on the same ctx must
     *  yield the empty-message digest. */
    @Test
    public void testDoFinalResetsCtx() {
        long ctx = NativeSunWrapper.sm3Init();
        try {
            byte[] msg = "test".getBytes(StandardCharsets.UTF_8);
            NativeSunWrapper.sm3Update(ctx, msg, 0, msg.length);
            byte[] first = new byte[32];
            NativeSunWrapper.sm3DoFinal(ctx, first, 0);
            Assertions.assertEquals(DIGEST_TEST, HEX.formatHex(first));

            // ctx is now reset; no further updates — expect empty-message digest
            byte[] second = new byte[32];
            NativeSunWrapper.sm3DoFinal(ctx, second, 0);
            Assertions.assertEquals(DIGEST_EMPTY, HEX.formatHex(second));
        } finally {
            NativeSunWrapper.sm3Free(ctx);
        }
    }

    /** sm3Reinit discards partial state; subsequent doFinal yields the
     *  empty-message digest. */
    @Test
    public void testReinit() {
        long ctx = NativeSunWrapper.sm3Init();
        try {
            byte[] msg = "test".getBytes(StandardCharsets.UTF_8);
            NativeSunWrapper.sm3Update(ctx, msg, 0, msg.length);
            NativeSunWrapper.sm3Reinit(ctx);  // abort the partial hash

            byte[] out = new byte[32];
            NativeSunWrapper.sm3DoFinal(ctx, out, 0);
            Assertions.assertEquals(DIGEST_EMPTY, HEX.formatHex(out));
        } finally {
            NativeSunWrapper.sm3Free(ctx);
        }
    }

    /** sm3Copy captures mid-computation state; both branches must converge
     *  on the same final digest. */
    @Test
    public void testCopy() {
        long ctx = NativeSunWrapper.sm3Init();
        long copy = 0;
        try {
            byte[] part1 = "te".getBytes(StandardCharsets.UTF_8);
            byte[] part2 = "st".getBytes(StandardCharsets.UTF_8);
            NativeSunWrapper.sm3Update(ctx, part1, 0, part1.length);
            copy = NativeSunWrapper.sm3Copy(ctx);

            // Finish original ctx
            NativeSunWrapper.sm3Update(ctx, part2, 0, part2.length);
            byte[] outOrig = new byte[32];
            NativeSunWrapper.sm3DoFinal(ctx, outOrig, 0);

            // Finish copy independently
            NativeSunWrapper.sm3Update(copy, part2, 0, part2.length);
            byte[] outCopy = new byte[32];
            NativeSunWrapper.sm3DoFinal(copy, outCopy, 0);

            Assertions.assertEquals(DIGEST_TEST, HEX.formatHex(outOrig));
            Assertions.assertArrayEquals(outOrig, outCopy);
        } finally {
            NativeSunWrapper.sm3Free(ctx);
            if (copy != 0) NativeSunWrapper.sm3Free(copy);
        }
    }

    /** Output offset: the digest must land at the correct position. */
    @Test
    public void testOutputOffset() {
        long ctx = NativeSunWrapper.sm3Init();
        try {
            byte[] msg = "test".getBytes(StandardCharsets.UTF_8);
            NativeSunWrapper.sm3Update(ctx, msg, 0, msg.length);
            byte[] out = new byte[33];
            NativeSunWrapper.sm3DoFinal(ctx, out, 1);
            // First byte must be untouched (zero)
            Assertions.assertEquals((byte) 0, out[0]);
            Assertions.assertEquals(DIGEST_TEST,
                    HEX.formatHex(out, 1, 33));
        } finally {
            NativeSunWrapper.sm3Free(ctx);
        }
    }

    // -------------------------------------------------------------------
    // Concurrency tests
    // -------------------------------------------------------------------

    @Test
    public void runSerially() throws Exception {
        NativeSunUtil.execTaskSerially(() -> {
            testOneShotNonEmpty();
            testMultiUpdate();
            return null;
        });
    }

    @Test
    public void runParallelly() throws Exception {
        NativeSunUtil.execTaskParallelly(() -> {
            testOneShotNonEmpty();
            testMultiUpdate();
            return null;
        });
    }

    // -------------------------------------------------------------------
    // Negative / bounds tests
    // -------------------------------------------------------------------

    @Test
    public void testNullInput() {
        long ctx = NativeSunWrapper.sm3Init();
        try {
            Assertions.assertThrows(NullPointerException.class,
                    () -> NativeSunWrapper.sm3Update(ctx, null, 0, 4));
        } finally {
            NativeSunWrapper.sm3Free(ctx);
        }
    }

    @Test
    public void testNullOutput() {
        long ctx = NativeSunWrapper.sm3Init();
        try {
            Assertions.assertThrows(NullPointerException.class,
                    () -> NativeSunWrapper.sm3DoFinal(ctx, null, 0));
        } finally {
            NativeSunWrapper.sm3Free(ctx);
        }
    }

    @Test
    public void testSmallOutput() {
        long ctx = NativeSunWrapper.sm3Init();
        try {
            byte[] smallOut = new byte[31];
            Assertions.assertThrows(ArrayIndexOutOfBoundsException.class,
                    () -> NativeSunWrapper.sm3DoFinal(ctx, smallOut, 0));
        } finally {
            NativeSunWrapper.sm3Free(ctx);
        }
    }

    @Test
    public void testNegativeInputOffset() {
        long ctx = NativeSunWrapper.sm3Init();
        try {
            byte[] in = "test".getBytes(StandardCharsets.UTF_8);
            Assertions.assertThrows(ArrayIndexOutOfBoundsException.class,
                    () -> NativeSunWrapper.sm3Update(ctx, in, -1, in.length));
        } finally {
            NativeSunWrapper.sm3Free(ctx);
        }
    }

    @Test
    public void testNegativeInputLength() {
        long ctx = NativeSunWrapper.sm3Init();
        try {
            byte[] in = "test".getBytes(StandardCharsets.UTF_8);
            Assertions.assertThrows(ArrayIndexOutOfBoundsException.class,
                    () -> NativeSunWrapper.sm3Update(ctx, in, 0, -1));
        } finally {
            NativeSunWrapper.sm3Free(ctx);
        }
    }

    @Test
    public void testInputOffsetPlusLengthOverflow() {
        long ctx = NativeSunWrapper.sm3Init();
        try {
            byte[] in = "test".getBytes(StandardCharsets.UTF_8);
            Assertions.assertThrows(ArrayIndexOutOfBoundsException.class,
                    () -> NativeSunWrapper.sm3Update(ctx, in, 2, in.length));
        } finally {
            NativeSunWrapper.sm3Free(ctx);
        }
    }

    @Test
    public void testNegativeOutputOffset() {
        long ctx = NativeSunWrapper.sm3Init();
        try {
            byte[] out = new byte[32];
            Assertions.assertThrows(ArrayIndexOutOfBoundsException.class,
                    () -> NativeSunWrapper.sm3DoFinal(ctx, out, -1));
        } finally {
            NativeSunWrapper.sm3Free(ctx);
        }
    }
}
