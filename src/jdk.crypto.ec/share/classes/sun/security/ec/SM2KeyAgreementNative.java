/*
 * Copyright (C) 2022, 2025, Tencent. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation. Tencent designates
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

import javax.crypto.KeyAgreementSpi;
import javax.crypto.SecretKey;
import javax.crypto.ShortBufferException;
import javax.crypto.spec.SecretKeySpec;
import java.math.BigInteger;
import java.security.*;
import java.security.interfaces.ECPrivateKey;
import java.security.interfaces.ECPublicKey;
import java.security.spec.AlgorithmParameterSpec;
import java.security.spec.SM2KeyAgreementParamSpec;

import static java.math.BigInteger.ZERO;
import static java.security.spec.SM2ParameterSpec.*;

/**
 * SM2 key agreement with OpenSSL.
 */
public final class SM2KeyAgreementNative extends KeyAgreementSpi {

    private SM2PrivateKey ephemeralPrivateKey;
    private SM2KeyAgreementParamSpec paramSpec;
    private SM2PublicKey peerEphemeralPublicKey;

    @Override
    protected void engineInit(Key key, SecureRandom random) {
        throw new UnsupportedOperationException(
                "Use init(Key, AlgorithmParameterSpec, SecureRandom) instead");
    }

    @Override
    protected void engineInit(Key key, AlgorithmParameterSpec params,
            SecureRandom random)
            throws InvalidKeyException, InvalidAlgorithmParameterException {
        if (!(key instanceof ECPrivateKey ecPrivateKey)) {
            throw new InvalidKeyException("Only accept ECPrivateKey");
        }

        if (!(params instanceof SM2KeyAgreementParamSpec)) {
            throw new InvalidAlgorithmParameterException(
                    "Only accept SM2KeyAgreementParamSpec");
        }

        BigInteger s = ecPrivateKey.getS();
        if (s.compareTo(ZERO) <= 0 || s.compareTo(ORDER) >= 0) {
            throw new InvalidKeyException("The private key must be " +
                    "within the range [1, n - 1]");
        }

        ephemeralPrivateKey = new SM2PrivateKey(ecPrivateKey);
        paramSpec = (SM2KeyAgreementParamSpec) params;
        peerEphemeralPublicKey = null;
    }

    @Override
    protected Key engineDoPhase(Key key, boolean lastPhase)
            throws InvalidKeyException, IllegalStateException {
        if (ephemeralPrivateKey == null || paramSpec == null) {
            throw new IllegalStateException("Not initialized");
        }

        if (peerEphemeralPublicKey != null) {
            throw new IllegalStateException("Phase already executed");
        }

        if (!lastPhase) {
            throw new IllegalStateException(
                    "Only two party agreement supported, lastPhase must be true");
        }

        if (!(key instanceof ECPublicKey ecPublicKey)) {
            throw new InvalidKeyException("Only accept ECPublicKey");
        }

        peerEphemeralPublicKey = new SM2PublicKey(ecPublicKey);

        return null;
    }

    @Override
    protected byte[] engineGenerateSecret() throws IllegalStateException {
        if (ephemeralPrivateKey == null || (peerEphemeralPublicKey == null)) {
            throw new IllegalStateException("Not initialized correctly");
        }

        byte[] result;
        try {
            result = deriveKeyImplNative();
        } catch (Exception e) {
            throw new IllegalStateException(e);
        }
        peerEphemeralPublicKey = null;
        return result;
    }

    private byte[] deriveKeyImplNative() {
        return NativeSunEC.sm2DeriveKey(
                new SM2PrivateKey(paramSpec.privateKey()).getEncoded(),
                new SM2PublicKey(paramSpec.publicKey()).getEncoded(),
                ephemeralPrivateKey.getEncoded(),
                paramSpec.id(),
                new SM2PublicKey(paramSpec.peerPublicKey()).getEncoded(),
                peerEphemeralPublicKey.getEncoded(),
                paramSpec.peerId(),
                paramSpec.isInitiator(),
                paramSpec.sharedKeyLength());
    }

    @Override
    protected int engineGenerateSecret(byte[] sharedSecret, int offset)
            throws IllegalStateException, ShortBufferException {
        if (offset + paramSpec.sharedKeyLength() > sharedSecret.length) {
            throw new ShortBufferException("Need " + paramSpec.sharedKeyLength()
                    + " bytes, only " + (sharedSecret.length - offset)
                    + " available");
        }

        byte[] secret = engineGenerateSecret();
        System.arraycopy(secret, 0, sharedSecret, offset, secret.length);
        return secret.length;
    }

    @Override
    protected SecretKey engineGenerateSecret(String algorithm)
            throws IllegalStateException, NoSuchAlgorithmException {
        if (algorithm == null) {
            throw new NoSuchAlgorithmException("Algorithm must not be null");
        }

        return new SecretKeySpec(engineGenerateSecret(), algorithm);
    }
}
