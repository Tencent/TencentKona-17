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
 * @summary The RSA crypto based on OpenSSL.
 * @modules java.base/sun.security.rsa
 * @library /test/lib /test/jdk/openssl
 * @build java.base/sun.security.rsa.NativeSunRsaSignWrapper NativeSunRsaSignUtil
 * @run junit/othervm -Djdk.sunrsasign.enableNativeCrypto=true NativeSunRsaSignTest
 * @run junit/othervm/policy=native.policy -Djdk.sunrsasign.enableNativeCrypto=true NativeSunRsaSignTest
 */

import org.junit.jupiter.api.Assertions;
import org.junit.jupiter.api.Test;
import sun.security.rsa.NativeSunRsaSignWrapper;

import java.math.BigInteger;
import java.util.HexFormat;

@EnableOnNativeSunRsaSign
public class NativeSunRsaSignTest {

    private static final HexFormat HEX = HexFormat.of();

    @Test
    public void testRSAModPow() {
        BigInteger base = new BigInteger("19058071224156864789844466979330892664777520457048234786139035643344145635582");
        BigInteger mod  = new BigInteger("75554098474976067521257305210610421240510163914613117319380559667371251381587");

        checkRSAModPow(
                base,
                BigInteger.valueOf(65537),
                mod,
                new BigInteger("5770048609366563851320890693196148833634112303472168971638730461010114147506"));
        checkRSAModPow(
                base,
                BigInteger.valueOf(75537),
                mod,
                new BigInteger("63446979364051087123350579021875958137036620431381329472348116892915461751531"));
        checkRSAModPow(
                base,
                new BigInteger("13456870775607312149"),
                mod,
                new BigInteger("39016891919893878823999350081191675846357272199067075794096200770872982089502"));
    }

    private void checkRSAModPow(BigInteger base, BigInteger exponent,
            BigInteger modulus, BigInteger expected) {
        byte[] actual = new byte[(modulus.bitLength() + 7) >> 3];
        NativeSunRsaSignWrapper.rsaModPow(base.toByteArray(), exponent.toByteArray(),
                modulus.toByteArray(), actual);
        Assertions.assertEquals(expected, new BigInteger(1, actual),
                HEX.formatHex(actual));
    }

    @Test
    public void runRSAModPowSerially() throws Exception {
        NativeSunRsaSignUtil.execTaskSerially(()-> {
            testRSAModPow();
            return null;
        });
    }

    @Test
    public void runRSAModPowParallelly() throws Exception {
        NativeSunRsaSignUtil.execTaskParallelly(()-> {
            testRSAModPow();
            return null;
        });
    }
}
