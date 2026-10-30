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
 * @summary SM3 MessageDigest via the JCA API, covering streaming update,
 *          clone, reset, and output-offset paths. Runs both with and without
 *          native crypto to verify SM3EngineNative and SM3EngineImpl produce
 *          identical results.
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

    private static final String DIGEST_EMPTY =
            "1ab21d8355cfa17f8e61194831e81a8f22bec8c728fefb747ed035eb5082aa2b";
    private static final String DIGEST_TEST =
            "55e12e91650d2fec56ec74e1d3e4ddbfce2ef3a65890c2a19ecf88a307e76a23";

    // -------------------------------------------------------------------
    // Existing one-shot test
    // -------------------------------------------------------------------

    @Test
    public void testDigest() throws Exception {
        testDigest("", DIGEST_EMPTY);
        testDigest("test", DIGEST_TEST);
    }

    private void testDigest(String message, String expectedDigest) throws Exception {
        MessageDigest md = MessageDigest.getInstance("SM3");
        byte[] digest = md.digest(message.getBytes(StandardCharsets.UTF_8));
        Assertions.assertEquals(expectedDigest, HEX.formatHex(digest));
    }

    // -------------------------------------------------------------------
    // Streaming update paths
    // -------------------------------------------------------------------

    /** Multiple update(byte[]) calls must yield the same digest as one-shot. */
    @Test
    public void testMultiUpdate() throws Exception {
        MessageDigest md = MessageDigest.getInstance("SM3");
        md.update("te".getBytes(StandardCharsets.UTF_8));
        md.update("st".getBytes(StandardCharsets.UTF_8));
        Assertions.assertEquals(DIGEST_TEST, HEX.formatHex(md.digest()));
    }

    /** update(byte) byte-by-byte must yield the same digest as one-shot. */
    @Test
    public void testByteByByteUpdate() throws Exception {
        MessageDigest md = MessageDigest.getInstance("SM3");
        for (byte b : "test".getBytes(StandardCharsets.UTF_8)) {
            md.update(b);
        }
        Assertions.assertEquals(DIGEST_TEST, HEX.formatHex(md.digest()));
    }

    /** update(byte[], offset, length) with a non-zero offset. */
    @Test
    public void testUpdateWithOffset() throws Exception {
        byte[] buf = "XXtest".getBytes(StandardCharsets.UTF_8);
        MessageDigest md = MessageDigest.getInstance("SM3");
        md.update(buf, 2, 4);  // skip the leading "XX"
        Assertions.assertEquals(DIGEST_TEST, HEX.formatHex(md.digest()));
    }

    // -------------------------------------------------------------------
    // Clone: mid-computation state copy
    // -------------------------------------------------------------------

    /** Clone mid-computation; both branches must converge on the same digest. */
    @Test
    public void testClone() throws Exception {
        MessageDigest md = MessageDigest.getInstance("SM3");
        md.update("te".getBytes(StandardCharsets.UTF_8));

        MessageDigest mdClone = (MessageDigest) md.clone();

        md.update("st".getBytes(StandardCharsets.UTF_8));
        mdClone.update("st".getBytes(StandardCharsets.UTF_8));

        byte[] orig  = md.digest();
        byte[] clone = mdClone.digest();

        Assertions.assertEquals(DIGEST_TEST, HEX.formatHex(orig));
        Assertions.assertArrayEquals(orig, clone,
                "Clone must produce the same digest as the original");
    }

    /** Clone of a freshly-initialised digest must compute correctly. */
    @Test
    public void testCloneOfFreshDigest() throws Exception {
        MessageDigest md = MessageDigest.getInstance("SM3");
        MessageDigest mdClone = (MessageDigest) md.clone();

        md.update("test".getBytes(StandardCharsets.UTF_8));
        mdClone.update("test".getBytes(StandardCharsets.UTF_8));

        Assertions.assertArrayEquals(md.digest(), mdClone.digest());
    }

    // -------------------------------------------------------------------
    // Reset: discards partial state
    // -------------------------------------------------------------------

    /** reset() must discard partial state; subsequent digest must reflect only
     *  data fed after the reset. */
    @Test
    public void testReset() throws Exception {
        MessageDigest md = MessageDigest.getInstance("SM3");
        md.update("ignored".getBytes(StandardCharsets.UTF_8));
        md.reset();
        md.update("test".getBytes(StandardCharsets.UTF_8));
        Assertions.assertEquals(DIGEST_TEST, HEX.formatHex(md.digest()));
    }

    /** After digest() the MessageDigest is implicitly reset; a subsequent
     *  digest() with no further updates must return the empty-message hash. */
    @Test
    public void testImplicitResetAfterDigest() throws Exception {
        MessageDigest md = MessageDigest.getInstance("SM3");
        md.update("test".getBytes(StandardCharsets.UTF_8));
        md.digest();  // consume and implicitly reset
        Assertions.assertEquals(DIGEST_EMPTY, HEX.formatHex(md.digest()));
    }

    // -------------------------------------------------------------------
    // Output offset: digest(byte[], int, int)
    // -------------------------------------------------------------------

    @Test
    public void testDigestWithOutputOffset() throws Exception {
        MessageDigest md = MessageDigest.getInstance("SM3");
        md.update("test".getBytes(StandardCharsets.UTF_8));
        byte[] out = new byte[33];
        int written = md.digest(out, 1, 32);
        Assertions.assertEquals(32, written);
        Assertions.assertEquals((byte) 0, out[0], "Byte before offset must be untouched");
        Assertions.assertEquals(DIGEST_TEST, HEX.formatHex(out, 1, 33));
    }
}
