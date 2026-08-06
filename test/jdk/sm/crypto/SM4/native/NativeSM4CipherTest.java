/*
 * Copyright (C) 2026, Tencent. All rights reserved.
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
 * @summary SM4 ECB/CBC/CTR cipher, exercising both the pure-Java and the
 *          OpenSSL-accelerated implementations.
 * @library /test/lib /test/jdk/openssl
 * @run junit/othervm NativeSM4CipherTest
 * @run junit/othervm -Djdk.sunjce.enableNativeCrypto=true NativeSM4CipherTest
 */

import org.junit.jupiter.api.Test;

import javax.crypto.Cipher;
import javax.crypto.IllegalBlockSizeException;
import javax.crypto.ShortBufferException;
import javax.crypto.spec.IvParameterSpec;
import javax.crypto.spec.SecretKeySpec;
import java.util.Arrays;
import java.util.HexFormat;

import static org.junit.jupiter.api.Assertions.assertArrayEquals;
import static org.junit.jupiter.api.Assertions.assertEquals;
import static org.junit.jupiter.api.Assertions.assertThrows;
import static org.junit.jupiter.api.Assertions.assertTrue;

@EnableOnNativeSM4
public class NativeSM4CipherTest {

    private static final HexFormat HEX = HexFormat.of();
    private static final byte[] KEY = HEX.parseHex("0123456789abcdeffedcba9876543210");
    private static final byte[] IV = HEX.parseHex("000102030405060708090a0b0c0d0e0f");

    private static SecretKeySpec key() {
        return new SecretKeySpec(KEY, "SM4");
    }

    // GB/T 32907-2016 single-block known-answer for ECB.
    @Test
    public void testKnownAnswer() throws Exception {
        byte[] pt = HEX.parseHex("0123456789abcdeffedcba9876543210");
        byte[] expected = HEX.parseHex("681edf34d206965e86b3e94f536e4246");

        Cipher c = Cipher.getInstance("SM4/ECB/NoPadding", "SunJCE");
        c.init(Cipher.ENCRYPT_MODE, key());
        assertArrayEquals(expected, c.doFinal(pt));
    }

    @Test
    public void testRoundTrip() throws Exception {
        byte[] aligned = randomBytes(48);
        byte[] unaligned = randomBytes(30);

        roundTrip("SM4/ECB/NoPadding", aligned, false);
        roundTrip("SM4/ECB/PKCS7Padding", unaligned, false);
        roundTrip("SM4/CBC/NoPadding", aligned, true);
        roundTrip("SM4/CBC/PKCS7Padding", unaligned, true);
        roundTrip("SM4/CTR/NoPadding", unaligned, true);
    }

    private void roundTrip(String transformation, byte[] pt, boolean withIv)
            throws Exception {
        Cipher enc = Cipher.getInstance(transformation, "SunJCE");
        Cipher dec = Cipher.getInstance(transformation, "SunJCE");
        if (withIv) {
            IvParameterSpec ivSpec = new IvParameterSpec(IV);
            enc.init(Cipher.ENCRYPT_MODE, key(), ivSpec);
            dec.init(Cipher.DECRYPT_MODE, key(), ivSpec);
        } else {
            enc.init(Cipher.ENCRYPT_MODE, key());
            dec.init(Cipher.DECRYPT_MODE, key());
        }
        byte[] ct = enc.doFinal(pt);
        assertArrayEquals(pt, dec.doFinal(ct), transformation);
    }

    @Test
    public void testMultiPartEqualsSingleShot() throws Exception {
        byte[] pt = randomBytes(48);

        Cipher single = Cipher.getInstance("SM4/CBC/NoPadding", "SunJCE");
        single.init(Cipher.ENCRYPT_MODE, key(), new IvParameterSpec(IV));
        byte[] oneShot = single.doFinal(pt);

        Cipher multi = Cipher.getInstance("SM4/CBC/NoPadding", "SunJCE");
        multi.init(Cipher.ENCRYPT_MODE, key(), new IvParameterSpec(IV));
        byte[] p1 = multi.update(pt, 0, 16);
        byte[] p2 = multi.update(pt, 16, 16);
        byte[] p3 = multi.doFinal(pt, 32, 16);
        byte[] joined = concat(p1, p2, p3);

        assertArrayEquals(oneShot, joined);
    }

    @Test
    public void testShortBuffer() throws Exception {
        Cipher c = Cipher.getInstance("SM4/CBC/PKCS7Padding", "SunJCE");
        c.init(Cipher.ENCRYPT_MODE, key(), new IvParameterSpec(IV));
        assertThrows(ShortBufferException.class,
                () -> c.doFinal(randomBytes(48), 0, 48, new byte[1], 0));
    }

    @Test
    public void testUnalignedNoPaddingFails() throws Exception {
        Cipher c = Cipher.getInstance("SM4/CBC/NoPadding", "SunJCE");
        c.init(Cipher.ENCRYPT_MODE, key(), new IvParameterSpec(IV));
        assertThrows(IllegalBlockSizeException.class,
                () -> c.doFinal(randomBytes(30)));
    }

    @Test
    public void testReuseAfterDoFinal() throws Exception {
        Cipher c = Cipher.getInstance("SM4/CBC/PKCS7Padding", "SunJCE");
        c.init(Cipher.ENCRYPT_MODE, key(), new IvParameterSpec(IV));
        byte[] pt = randomBytes(20);
        byte[] first = c.doFinal(pt);
        // The cipher must be reusable for another message without re-init.
        byte[] second = c.doFinal(pt);
        assertArrayEquals(first, second);
        assertTrue(first.length > 0);
    }

    // A ShortBufferException from doFinal(...,output,...) must not consume or
    // reset the underlying state: a retry with an adequate buffer (on the same
    // Cipher) must produce the correct result.
    @Test
    public void testShortBufferDoFinalRetriable() throws Exception {
        String[] transformations = {
            "SM4/CBC/NoPadding", "SM4/CBC/PKCS7Padding",
            "SM4/ECB/NoPadding", "SM4/ECB/PKCS7Padding", "SM4/CTR/NoPadding"
        };
        for (String tr : transformations) {
            boolean withIv = !tr.contains("ECB");
            boolean padded = tr.contains("PKCS7");
            byte[] pt = padded ? randomBytes(30) : randomBytes(48);
            byte[] expected = encryptOneShot(tr, pt, withIv);

            Cipher c = Cipher.getInstance(tr, "SunJCE");
            init(c, Cipher.ENCRYPT_MODE, withIv);
            assertThrows(ShortBufferException.class,
                    () -> c.doFinal(pt, 0, pt.length, new byte[1], 0), tr);

            // retry on the SAME cipher with a properly sized buffer
            byte[] out = new byte[c.getOutputSize(pt.length)];
            int n = c.doFinal(pt, 0, pt.length, out, 0);
            assertArrayEquals(expected, Arrays.copyOf(out, n), tr + " retry");
        }
    }

    // The same, but with buffered data from a prior update(): the ShortBuffer
    // must not lose the buffered bytes.
    @Test
    public void testShortBufferAfterUpdateRetriable() throws Exception {
        byte[] pt = randomBytes(48);
        byte[] expected = encryptOneShot("SM4/CBC/NoPadding", pt, true);

        Cipher c = Cipher.getInstance("SM4/CBC/NoPadding", "SunJCE");
        c.init(Cipher.ENCRYPT_MODE, key(), new IvParameterSpec(IV));
        byte[] head = c.update(pt, 0, 16);
        assertThrows(ShortBufferException.class,
                () -> c.doFinal(pt, 16, 32, new byte[1], 0));

        byte[] out = new byte[c.getOutputSize(32)];
        int n = c.doFinal(pt, 16, 32, out, 0);
        assertArrayEquals(expected, concat(head, Arrays.copyOf(out, n)));
    }

    private void init(Cipher c, int opmode, boolean withIv)
            throws Exception {
        if (withIv) {
            c.init(opmode, key(), new IvParameterSpec(IV));
        } else {
            c.init(opmode, key());
        }
    }

    private byte[] encryptOneShot(String tr, byte[] pt, boolean withIv)
            throws Exception {
        Cipher c = Cipher.getInstance(tr, "SunJCE");
        init(c, Cipher.ENCRYPT_MODE, withIv);
        return c.doFinal(pt);
    }

    // update() for a block cipher with fewer than one block of input must
    // emit 0 bytes and MUST NOT reject a zero-length output buffer. Regression
    // for the earlier over-strict getOutputSize() pre-check.
    @Test
    public void testUpdateSmallInputEmitsZero() throws Exception {
        String[] blockTransformations = {
            "SM4/CBC/NoPadding", "SM4/CBC/PKCS7Padding",
            "SM4/ECB/NoPadding", "SM4/ECB/PKCS7Padding"
        };
        byte[] one = new byte[] { 0x11 };
        for (String tr : blockTransformations) {
            Cipher c = Cipher.getInstance(tr, "SunJCE");
            init(c, Cipher.ENCRYPT_MODE, !tr.contains("ECB"));
            // zero-length output buffer must be acceptable when 0 bytes emit
            int n = c.update(one, 0, 1, new byte[0], 0);
            assertEquals(0, n, tr + " update should emit 0 bytes for 1B input");
        }
        // CTR (stream mode): 1 byte in -> 1 byte out; zero output buffer
        // must be rejected.
        Cipher ctr = Cipher.getInstance("SM4/CTR/NoPadding", "SunJCE");
        init(ctr, Cipher.ENCRYPT_MODE, true);
        assertThrows(ShortBufferException.class,
                () -> ctr.update(one, 0, 1, new byte[0], 0),
                "CTR update should require 1 byte output");
    }

    // Sub-block update on decrypt must also emit 0 bytes (native+padding path
    // holds back one block for potential padding stripping).
    @Test
    public void testUpdateSmallInputDecryptEmitsZero() throws Exception {
        byte[] pt = randomBytes(48);
        byte[] ct = encryptOneShot("SM4/CBC/PKCS7Padding", pt, true);

        Cipher dec = Cipher.getInstance("SM4/CBC/PKCS7Padding", "SunJCE");
        dec.init(Cipher.DECRYPT_MODE, key(), new IvParameterSpec(IV));
        // one full block in: decrypt+padding must still emit 0 (hold-back)
        int n = dec.update(ct, 0, 16, new byte[0], 0);
        assertEquals(0, n);
    }

    // Decrypt+padding doFinal must accept an output buffer sized to the
    // ACTUAL plaintext length (which is up to blockSize less than the
    // ciphertext length). Regression for the earlier estOutSize-strict check.
    @Test
    public void testDoFinalDecryptExactPlaintextBufferFits() throws Exception {
        String[] paddedTransformations = {
            "SM4/CBC/PKCS7Padding", "SM4/ECB/PKCS7Padding"
        };
        for (String tr : paddedTransformations) {
            boolean withIv = !tr.contains("ECB");
            byte[] pt = randomBytes(30);
            byte[] ct = encryptOneShot(tr, pt, withIv);

            Cipher dec = Cipher.getInstance(tr, "SunJCE");
            init(dec, Cipher.DECRYPT_MODE, withIv);
            byte[] out = new byte[pt.length]; // exact plaintext length, < ct.length
            int n = dec.doFinal(ct, 0, ct.length, out, 0);
            assertEquals(pt.length, n, tr);
            assertArrayEquals(pt, out, tr);
        }
    }

    // In multi-part decrypt+padding, if doFinal(...,output,...) is called
    // with a buffer between (estOutSize - blockSize) and the actual plaintext
    // length, the actual size is only known AFTER finalize strips the
    // padding. The Cipher must save/restore its native state so a
    // ShortBufferException can be retried on the same Cipher with a bigger
    // buffer -- matching CipherCore's cipher.save()/restore() behaviour.
    //
    // Picking pt.length = 31 (1 byte of PKCS7 padding, ct.length = 32) sends
    // the retry path through the (estOutSize - blockSize, actual] band which
    // is where the earlier native version consumed state and broke retry.
    @Test
    public void testDoFinalDecryptShortBufferAfterUpdateRetriable()
            throws Exception {
        String[] paddedTransformations = {
            "SM4/CBC/PKCS7Padding", "SM4/ECB/PKCS7Padding"
        };
        for (String tr : paddedTransformations) {
            boolean withIv = !tr.contains("ECB");
            byte[] pt = randomBytes(31);
            byte[] ct = encryptOneShot(tr, pt, withIv);

            Cipher dec = Cipher.getInstance(tr, "SunJCE");
            init(dec, Cipher.DECRYPT_MODE, withIv);
            // Feed the first ct block via update(): with padding-mode decrypt
            // this must emit 0 bytes (holdback) and buffer the block.
            byte[] head = dec.update(ct, 0, 16);
            assertEquals(0, (head == null ? 0 : head.length),
                    tr + ": decrypt update should hold back a block");

            // doFinal with the remainder into a too-small buffer: buffer is
            // >= (estOutSize - blockSize) = 16, so up-front check passes, but
            // actual output = 31 > 16, must ShortBuffer AND preserve state.
            byte[] small = new byte[16];
            assertThrows(ShortBufferException.class,
                    () -> dec.doFinal(ct, 16, 16, small, 0),
                    tr + ": expected ShortBuffer on undersized retry buffer");

            // Retry on the SAME Cipher with an adequate buffer.
            byte[] out = new byte[pt.length];
            int n = dec.doFinal(ct, 16, 16, out, 0);
            assertEquals(pt.length, n, tr + ": retry length");
            assertArrayEquals(pt, out, tr + ": retry plaintext");
        }
    }

    private static byte[] randomBytes(int n) {
        byte[] b = new byte[n];
        for (int i = 0; i < n; i++) {
            b[i] = (byte) (i * 7 + 1);
        }
        return b;
    }

    private static byte[] concat(byte[]... parts) {
        int len = 0;
        for (byte[] p : parts) {
            if (p != null) {
                len += p.length;
            }
        }
        byte[] out = new byte[len];
        int o = 0;
        for (byte[] p : parts) {
            if (p != null && p.length > 0) {
                System.arraycopy(p, 0, out, o, p.length);
                o += p.length;
            }
        }
        return Arrays.copyOf(out, o);
    }
}
