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
 * @summary The EC key pair generation based on OpenSSL.
 * @modules jdk.crypto.ec/sun.security.ec
 * @library /test/lib
 * @build jdk.crypto.ec/sun.security.ec.NativeECWrapper NativeECUtil EnableOnNativeCrypto
 * @run junit/othervm -Djdk.sunec.enableNativeCrypto=true NativeECTest
 * @run junit/othervm/policy=native.policy -Djdk.sunec.enableNativeCrypto=true NativeECTest
 */

import java.nio.charset.StandardCharsets;
import java.security.*;
import java.util.Arrays;
import java.util.HexFormat;

import sun.security.ec.NativeECWrapper;

import org.junit.jupiter.api.Assertions;
import org.junit.jupiter.api.Test;

@EnableOnNativeCrypto
public class NativeECTest {

    private static final HexFormat HEX = HexFormat.of();
    private static final byte[] MESSAGE = "test".getBytes(StandardCharsets.UTF_8);

    @Test
    public void testECGenKeyPair() throws Exception {
        checkECGenKeyPair("secp256r1", 256, false);
        checkECGenKeyPair("secp256r1", 256, true);

        checkECGenKeyPair("secp384r1", 384, false);
        checkECGenKeyPair("secp384r1", 384, true);

        checkECGenKeyPair("secp521r1", 521, false);
        checkECGenKeyPair("secp521r1", 521, true);

        checkECGenKeyPair("curvesm2", 256, false);
        checkECGenKeyPair("curvesm2", 256, true);
    }

    @Test
    public void runECGenKeyPairSerially() throws Exception {
        NativeECUtil.execTaskSerially(()-> {
            testECGenKeyPair();
            return null;
        });
    }

    @Test
    public void runECGenKeyPairParallelly() throws Exception {
        NativeECUtil.execTaskParallelly(()-> {
            testECGenKeyPair();
            return null;
        });
    }

    @Test
    public void testECGenKeyPairWithUnsupportedCurve() {
        Assertions.assertThrows(InvalidAlgorithmParameterException.class,
                ()-> checkECGenKeyPair("unknown", 256, false));
        Assertions.assertThrows(InvalidAlgorithmParameterException.class,
                ()-> checkECGenKeyPair("unknown", 256, true));
    }

    private void checkECGenKeyPair(
            String curve, int orderLenInBits, boolean needSeed)
            throws Exception {
        int privKeyLen = NativeECUtil.privKeyLen(orderLenInBits);
        int pubKeyLen = NativeECUtil.pubKeyLen(privKeyLen);
        byte[] seed = needSeed ? NativeECUtil.seed(orderLenInBits) : null;

        int curveNID = NativeECWrapper.getCurveNID(curve);
        byte[] privKey = new byte[privKeyLen];
        byte[] pubKey = new byte[pubKeyLen];
        NativeECWrapper.ecGenKeyPair(curveNID, seed, privKey, pubKey);

        // The keys must not be zero-arrays
        Assertions.assertFalse(Arrays.equals(new byte[privKeyLen], privKey));
        Assertions.assertFalse(Arrays.equals(new byte[pubKeyLen], pubKey));

        // Public key must be uncompressed point
        Assertions.assertEquals(4, pubKey[0]);
    }

    @Test
    public void testXDHComputePubKey() throws Exception {
        checkXDHComputePubKey("X25519", 32);
        checkXDHComputePubKey("X448", 56);
    }

    @Test
    public void runXDHComputePubKeySerially() throws Exception {
        NativeECUtil.execTaskSerially(()-> {
            testXDHComputePubKey();
            return null;
        });
    }

    @Test
    public void runXDHComputePubKeyParallelly() throws Exception {
        NativeECUtil.execTaskParallelly(()-> {
            testXDHComputePubKey();
            return null;
        });
    }

    @Test
    public void testXDHComputePubKeyWithUnsupportedCurve() {
        Assertions.assertThrows(InvalidAlgorithmParameterException.class,
                ()-> checkXDHComputePubKey("unknown", 32));
    }

    private void checkXDHComputePubKey(String curve, int keyLength)
            throws Exception {
        byte[] privKey = new byte[keyLength];
        new SecureRandom().nextBytes(privKey);

        int curveNID = NativeECWrapper.getCurveNID(curve);
        byte[] pubKey = new byte[keyLength];
        NativeECWrapper.xdhComputePubKey(curveNID, privKey, pubKey);

        // The keys must not be zero-arrays
        Assertions.assertFalse(Arrays.equals(new byte[privKey.length], privKey));
        Assertions.assertFalse(Arrays.equals(new byte[pubKey.length], pubKey));
    }
}
