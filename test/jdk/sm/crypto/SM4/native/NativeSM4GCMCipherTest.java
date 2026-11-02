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
 * @summary SM4-GCM cipher, exercising both the pure-Java and the
 *          OpenSSL-accelerated (SM4-GCM) implementations.
 * @library /test/lib /test/jdk/openssl
 * @run junit/othervm NativeSM4GCMCipherTest
 * @run junit/othervm -Djdk.sunjce.enableNativeCrypto=true NativeSM4GCMCipherTest
 */

import org.junit.jupiter.api.Test;

import javax.crypto.AEADBadTagException;
import javax.crypto.Cipher;
import javax.crypto.spec.GCMParameterSpec;
import javax.crypto.spec.SecretKeySpec;
import java.nio.ByteBuffer;
import java.util.HexFormat;

import static org.junit.jupiter.api.Assertions.assertArrayEquals;
import static org.junit.jupiter.api.Assertions.assertThrows;

@EnableOnNativeSM4
public class NativeSM4GCMCipherTest {

    private static final HexFormat HEX = HexFormat.of();
    private static final byte[] KEY = HEX.parseHex("0123456789abcdeffedcba9876543210");
    private static final byte[] IV = HEX.parseHex("00000000000000000000000c");
    private static final byte[] AAD =
            HEX.parseHex("feedfacedeadbeeffeedfacedeadbeefabaddad2");

    private static SecretKeySpec key() {
        return new SecretKeySpec(KEY, "SM4");
    }

    private static byte[] gcm(int opmode, byte[] in, byte[] aad, int tagBits)
            throws Exception {
        Cipher c = Cipher.getInstance("SM4/GCM/NoPadding", "SunJCE");
        c.init(opmode, key(), new GCMParameterSpec(tagBits, IV));
        if (aad != null) {
            c.updateAAD(aad);
        }
        return c.doFinal(in);
    }

    @Test
    public void testRoundTrip() throws Exception {
        roundTrip(new byte[0], null, 128);
        roundTrip(randomBytes(13), null, 128);
        roundTrip(randomBytes(64), AAD, 128);
        roundTrip(randomBytes(64), AAD, 96);
        roundTrip(randomBytes(13), AAD, 104);
    }

    private void roundTrip(byte[] pt, byte[] aad, int tagBits) throws Exception {
        byte[] ct = gcm(Cipher.ENCRYPT_MODE, pt, aad, tagBits);
        byte[] back = gcm(Cipher.DECRYPT_MODE, ct, aad, tagBits);
        assertArrayEquals(pt, back);
    }

    @Test
    public void testMultiPartEqualsSingleShot() throws Exception {
        byte[] pt = randomBytes(64);
        byte[] oneShot = gcm(Cipher.ENCRYPT_MODE, pt, AAD, 128);

        Cipher c = Cipher.getInstance("SM4/GCM/NoPadding", "SunJCE");
        c.init(Cipher.ENCRYPT_MODE, key(), new GCMParameterSpec(128, IV));
        c.updateAAD(AAD, 0, 10);
        c.updateAAD(AAD, 10, AAD.length - 10);
        byte[] u1 = c.update(pt, 0, 20);
        byte[] u2 = c.update(pt, 20, 44);
        byte[] f = c.doFinal();
        assertArrayEquals(oneShot, concat(u1, u2, f));
    }

    @Test
    public void testTamperedTag() throws Exception {
        byte[] ct = gcm(Cipher.ENCRYPT_MODE, randomBytes(64), AAD, 128);
        ct[ct.length - 1] ^= 0x01;
        byte[] tampered = ct;
        assertThrows(AEADBadTagException.class,
                () -> gcm(Cipher.DECRYPT_MODE, tampered, AAD, 128));
    }

    @Test
    public void testWrongAAD() throws Exception {
        byte[] ct = gcm(Cipher.ENCRYPT_MODE, randomBytes(64), AAD, 128);
        assertThrows(AEADBadTagException.class,
                () -> gcm(Cipher.DECRYPT_MODE, ct, HEX.parseHex("00"), 128));
    }

    @Test
    public void testIvReuseRejected() throws Exception {
        Cipher c = Cipher.getInstance("SM4/GCM/NoPadding", "SunJCE");
        c.init(Cipher.ENCRYPT_MODE, key(), new GCMParameterSpec(128, IV));
        c.doFinal(randomBytes(13));
        // Reusing the same key+IV for another encryption must be rejected.
        assertThrows(IllegalStateException.class, () -> c.doFinal(randomBytes(13)));
    }

    // AAD supplied via ByteBuffer (heap/direct/read-only) must be supported and
    // must produce the same result as AAD supplied via byte[].
    @Test
    public void testByteBufferAAD() throws Exception {
        byte[] pt = randomBytes(64);
        byte[] refCt = gcm(Cipher.ENCRYPT_MODE, pt, AAD, 128);

        checkAADBuffer(ByteBuffer.wrap(AAD.clone()), pt, refCt);            // heap
        ByteBuffer direct = ByteBuffer.allocateDirect(AAD.length);
        direct.put(AAD).flip();
        checkAADBuffer(direct, pt, refCt);                                 // direct
        checkAADBuffer(ByteBuffer.wrap(AAD.clone()).asReadOnlyBuffer(),
                pt, refCt);                                                // read-only
    }

    private void checkAADBuffer(ByteBuffer aadBuf, byte[] pt, byte[] refCt)
            throws Exception {
        Cipher enc = Cipher.getInstance("SM4/GCM/NoPadding", "SunJCE");
        enc.init(Cipher.ENCRYPT_MODE, key(), new GCMParameterSpec(128, IV));
        enc.updateAAD(aadBuf);
        byte[] ct = enc.doFinal(pt);
        assertArrayEquals(refCt, ct, "ByteBuffer AAD ciphertext");

        Cipher dec = Cipher.getInstance("SM4/GCM/NoPadding", "SunJCE");
        dec.init(Cipher.DECRYPT_MODE, key(), new GCMParameterSpec(128, IV));
        dec.updateAAD(AAD);
        assertArrayEquals(pt, dec.doFinal(ct), "ByteBuffer AAD roundtrip");
    }

    // A GCM decrypt Cipher must be reusable for another message: after
    // doFinal succeeds, updateAAD + doFinal on new data should work without
    // re-init. Only encryption is guarded (IV reuse is unsafe).
    @Test
    public void testDecryptReuseAfterDoFinal() throws Exception {
        byte[] pt1 = randomBytes(32);
        byte[] pt2 = randomBytes(48);
        byte[] aad2 = HEX.parseHex("aabbccdd");
        byte[] ct1 = gcm(Cipher.ENCRYPT_MODE, pt1, AAD, 128);
        byte[] ct2 = gcm(Cipher.ENCRYPT_MODE, pt2, aad2, 128);

        Cipher dec = Cipher.getInstance("SM4/GCM/NoPadding", "SunJCE");
        dec.init(Cipher.DECRYPT_MODE, key(), new GCMParameterSpec(128, IV));
        dec.updateAAD(AAD);
        assertArrayEquals(pt1, dec.doFinal(ct1), "first decrypt");

        // Same Cipher, fresh AAD + fresh ciphertext -- must not fail with
        // "AAD must be supplied before encryption/decryption starts".
        dec.updateAAD(aad2);
        assertArrayEquals(pt2, dec.doFinal(ct2), "second decrypt on same Cipher");
    }

    private static byte[] randomBytes(int n) {
        byte[] b = new byte[n];
        for (int i = 0; i < n; i++) {
            b[i] = (byte) (i * 5 + 3);
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
        return out;
    }
}
