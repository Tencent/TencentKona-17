/*
 * Copyright (C) 2022, 2023, THL A29 Limited, a Tencent company. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation. THL A29 Limited designates
 * this particular file as subject to the "Classpath" exception as provided
 * in the LICENSE file that accompanied this code.
 *
 * This code is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License version 2 for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

package sun.security.ec;

import java.io.ByteArrayOutputStream;
import java.math.BigInteger;
import java.security.*;
import java.security.interfaces.ECPrivateKey;
import java.security.interfaces.ECPublicKey;
import java.security.spec.AlgorithmParameterSpec;
import java.security.spec.SM2ParameterSpec;
import java.security.spec.SM2SignatureParameterSpec;

import static java.math.BigInteger.ONE;
import static java.math.BigInteger.ZERO;

/**
 * SM2 signature with OpenSSL.
 */
public class SM2SignatureNative extends SignatureSpi {

    // The default ID 1234567812345678
    private static final byte[] DEFAULT_ID = new byte[] {
            49, 50, 51, 52, 53, 54, 55, 56,
            49, 50, 51, 52, 53, 54, 55, 56};

    private SM2PrivateKey privateKey;
    private SM2PublicKey publicKey;
    private byte[] id;

    private final ByteArrayOutputStream buffer = new ByteArrayOutputStream();

    @Override
    protected void engineInitSign(PrivateKey privateKey, SecureRandom random)
            throws InvalidKeyException {
        this.privateKey = null;
        buffer.reset();

        if (!(privateKey instanceof ECPrivateKey ecPrivateKey)) {
            throw new InvalidKeyException("Only ECPrivateKey accepted!");
        }

        BigInteger s = ecPrivateKey.getS();
        if (s.compareTo(ZERO) <= 0
                || s.compareTo(SM2ParameterSpec.ORDER.subtract(ONE)) >= 0) {
            throw new InvalidKeyException("The private key must be " +
                    "within the range [1, n - 2]");
        }

        this.privateKey = new SM2PrivateKey(ecPrivateKey);
    }

    @Override
    protected void engineInitSign(PrivateKey privateKey)
            throws InvalidKeyException {
        engineInitSign(privateKey, null);
    }

    @Override
    protected void engineInitVerify(PublicKey publicKey)
            throws InvalidKeyException {
        this.privateKey = null;
        this.publicKey = null;
        buffer.reset();

        if (!(publicKey instanceof ECPublicKey)) {
            throw new InvalidKeyException("Only ECPublicKey accepted!");
        }

        this.publicKey = new SM2PublicKey((ECPublicKey) publicKey);
    }

    @Override
    protected void engineSetParameter(AlgorithmParameterSpec params)
            throws InvalidAlgorithmParameterException {
        privateKey = null;
        publicKey = null;
        id = null;

        if (!(params instanceof SM2SignatureParameterSpec paramSpec)) {
            throw new InvalidAlgorithmParameterException(
                    "Only accept SM2SignatureParameterSpec");
        }

        publicKey = new SM2PublicKey(paramSpec.getPublicKey());
        id = paramSpec.getId();
    }

    @Deprecated
    @Override
    protected void engineSetParameter(String param, Object value)
            throws InvalidParameterException {
        throw new UnsupportedOperationException(
                "Use setParameter(AlgorithmParameterSpec params) instead");
    }

    @Deprecated
    @Override
    protected Object engineGetParameter(String param)
            throws InvalidParameterException {
        throw new UnsupportedOperationException(
                "getParameter(String param) not supported");
    }

    @Override
    protected void engineUpdate(byte b) throws SignatureException {
        byte[] buf = new byte[] {b};
        buffer.write(buf, 0, 1);
    }

    @Override
    protected void engineUpdate(byte[] b, int off, int len)
            throws SignatureException {
        buffer.write(b, off, len);
    }

    @Override
    protected byte[] engineSign() throws SignatureException {
        if (privateKey == null) {
            throw new SignatureException("Private key not initialized");
        }

        if (id == null) {
            id = DEFAULT_ID.clone();
        }

        byte[] sign = NativeSunEC.sm2Sign(privateKey.getEncoded(),
                publicKey == null ? null : publicKey.getEncoded(),
                id, buffer.toByteArray());
        buffer.reset();
        return sign;
    }

    private static byte[] combineKey(byte[] priKey, byte[] pubKey) {
        int keySize = priKey.length;
        keySize += pubKey == null ? 0 : pubKey.length;
        byte[] key = new byte[keySize];
        System.arraycopy(priKey, 0, key, 0, priKey.length);
        if (pubKey != null) {
            System.arraycopy(pubKey, 0, key, priKey.length, pubKey.length);
        }

        return key;
    }

    @Override
    protected boolean engineVerify(byte[] sigBytes) throws SignatureException {
        if (publicKey == null) {
            throw new SignatureException("Public key not initialized");
        }

        if (id == null) {
            id = DEFAULT_ID.clone();
        }

        boolean verified = NativeSunEC.sm2Verify(publicKey.getEncoded(),
                id, buffer.toByteArray(), sigBytes);
        buffer.reset();
        return verified;
    }
}
