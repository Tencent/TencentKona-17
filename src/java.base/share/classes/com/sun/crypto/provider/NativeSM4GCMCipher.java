/*
 * Copyright (C) 2026, Tencent. All rights reserved.
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

package com.sun.crypto.provider;

import javax.crypto.AEADBadTagException;
import javax.crypto.BadPaddingException;
import javax.crypto.Cipher;
import javax.crypto.CipherSpi;
import javax.crypto.IllegalBlockSizeException;
import javax.crypto.NoSuchPaddingException;
import javax.crypto.ShortBufferException;
import javax.crypto.spec.GCMParameterSpec;
import java.io.ByteArrayOutputStream;
import java.nio.ByteBuffer;
import java.security.AlgorithmParameters;
import java.security.GeneralSecurityException;
import java.security.InvalidAlgorithmParameterException;
import java.security.InvalidKeyException;
import java.security.Key;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.security.ProviderException;
import java.security.SecureRandom;
import java.security.spec.AlgorithmParameterSpec;
import java.security.spec.InvalidParameterSpecException;
import java.util.Arrays;

/**
 * SM4-GCM cipher backed by OpenSSL (EVP "SM4-GCM", requires OpenSSL 3.0+).
 *
 * <p>This CipherSpi is registered for {@code SM4/GCM/NoPadding} in place of
 * the pure-Java {@link GaloisCounterMode.SM4} only when native crypto is
 * enabled (see {@link SMEntries}). It follows the AEAD contract: no plaintext
 * is released before the authentication tag is verified, so all input is
 * buffered and processed in a single native pass on {@code doFinal}.
 *
 * <p>Parameter validation (tag length, IV, key size, IV-reuse) mirrors
 * {@link GaloisCounterMode} so behaviour is identical to the Java path.
 */
public final class NativeSM4GCMCipher extends CipherSpi {

    private static final int DEFAULT_IV_LEN = 12;   // bytes
    private static final int DEFAULT_TAG_LEN = 16;  // bytes
    private static final int SM4_KEY_SIZE = 16;     // bytes

    /*
     * If a single message is larger than this threshold the buffer's backing
     * array is replaced after doFinal rather than reused. This prevents a
     * Cipher that once processed a large message from permanently retaining
     * the corresponding heap allocation for its whole lifetime.
     */
    private static final int BUFFER_SHRINK_THRESHOLD = 64 * 1024; // 64 KiB

    /*
     * ByteArrayOutputStream subclass that exposes its internal array for
     * zero-copy hand-off to the native layer, and that can securely wipe
     * its content before the backing array is released or reused.
     */
    private static final class ClearableBAOS extends ByteArrayOutputStream {
        /** Direct access to the backing array (may be larger than count). */
        byte[] internalBuf() { return buf; }
        /** Number of valid bytes currently stored. */
        int internalCount() { return count; }

        /** Zero the live content and reset the write position. */
        void clearAndReset() {
            Arrays.fill(buf, 0, count, (byte) 0);
            count = 0;
        }
    }

    private boolean encryption;
    private boolean initialized;
    private boolean reInit;         // set after an encrypt doFinal
    private boolean dataProcessed;  // set once update/doFinal starts on data

    private byte[] keyBytes;
    private byte[] iv;
    private int tagLenBytes = DEFAULT_TAG_LEN;
    private SecureRandom random;

    // IV-reuse guard, mirroring GaloisCounterMode.
    private byte[] lastKey;
    private byte[] lastIv;

    private ClearableBAOS aadBuffer = new ClearableBAOS();
    private ClearableBAOS dataBuffer = new ClearableBAOS();

    public NativeSM4GCMCipher() {
    }

    @Override
    protected void engineSetMode(String mode) throws NoSuchAlgorithmException {
        if (mode != null && !mode.equalsIgnoreCase("GCM")) {
            throw new NoSuchAlgorithmException("Mode must be GCM");
        }
    }

    @Override
    protected void engineSetPadding(String padding)
            throws NoSuchPaddingException {
        if (padding != null && !padding.equalsIgnoreCase("NoPadding")) {
            throw new NoSuchPaddingException("Padding must be NoPadding");
        }
    }

    @Override
    protected int engineGetBlockSize() {
        return 16;
    }

    @Override
    protected int engineGetOutputSize(int inputLen) {
        int buffered = Math.addExact(dataBuffer.size(), inputLen);
        if (encryption) {
            return Math.addExact(buffered, tagLenBytes);
        }
        return Math.max(0, buffered - tagLenBytes);
    }

    @Override
    protected byte[] engineGetIV() {
        return (iv == null) ? null : iv.clone();
    }

    @Override
    protected AlgorithmParameters engineGetParameters() {
        if (iv == null) {
            return null;
        }
        try {
            AlgorithmParameters params = AlgorithmParameters.getInstance("GCM");
            params.init(new GCMParameterSpec(tagLenBytes * 8, iv));
            return params;
        } catch (GeneralSecurityException e) {
            throw new ProviderException("Could not encode parameters", e);
        }
    }

    @Override
    protected void engineInit(int opmode, Key key, SecureRandom random)
            throws InvalidKeyException {
        if (opmode == Cipher.DECRYPT_MODE || opmode == Cipher.UNWRAP_MODE) {
            throw new InvalidKeyException("No GCMParameterSpec specified");
        }
        try {
            engineInit(opmode, key, (AlgorithmParameterSpec) null, random);
        } catch (InvalidAlgorithmParameterException e) {
            // never happens without explicit parameters
            throw new InvalidKeyException(e.getMessage(), e);
        }
    }

    @Override
    protected void engineInit(int opmode, Key key,
                              AlgorithmParameterSpec params,
                              SecureRandom random)
            throws InvalidKeyException, InvalidAlgorithmParameterException {
        this.random = random;
        byte[] newIv;
        int tagLen;
        if (params == null) {
            newIv = createIv(random);
            tagLen = DEFAULT_TAG_LEN * 8;
        } else {
            if (!(params instanceof GCMParameterSpec)) {
                throw new InvalidAlgorithmParameterException(
                        "AlgorithmParameterSpec not of GCMParameterSpec");
            }
            GCMParameterSpec spec = (GCMParameterSpec) params;
            newIv = spec.getIV();
            if (newIv == null) {
                throw new InvalidAlgorithmParameterException("IV is null");
            }
            if (newIv.length == 0) {
                throw new InvalidAlgorithmParameterException("IV is empty");
            }
            newIv = newIv.clone();
            tagLen = spec.getTLen();
        }
        init(opmode, key, tagLen, newIv);
    }

    @Override
    protected void engineInit(int opmode, Key key, AlgorithmParameters params,
                              SecureRandom random)
            throws InvalidKeyException, InvalidAlgorithmParameterException {
        AlgorithmParameterSpec spec = null;
        if (params != null) {
            try {
                spec = params.getParameterSpec(GCMParameterSpec.class);
            } catch (InvalidParameterSpecException e) {
                throw new InvalidAlgorithmParameterException(e);
            }
        }
        engineInit(opmode, key, spec, random);
    }

    private void init(int opmode, Key key, int tagLen, byte[] newIv)
            throws InvalidKeyException, InvalidAlgorithmParameterException {
        boolean encrypt = (opmode == Cipher.ENCRYPT_MODE)
                || (opmode == Cipher.WRAP_MODE);

        if (tagLen < 96 || tagLen > 128 || ((tagLen & 0x07) != 0)) {
            throw new InvalidAlgorithmParameterException(
                    "Unsupported TLen value.  Must be one of "
                    + "{128, 120, 112, 104, 96}");
        }

        if (key == null) {
            throw new InvalidKeyException("The key must not be null");
        }
        byte[] keyValue = key.getEncoded();
        if (keyValue == null) {
            throw new InvalidKeyException("Key encoding must not be null");
        }
        if (keyValue.length != SM4_KEY_SIZE) {
            Arrays.fill(keyValue, (byte) 0);
            throw new InvalidKeyException("The key must be "
                    + SM4_KEY_SIZE + " bytes");
        }

        if (encrypt) {
            if (MessageDigest.isEqual(keyValue, lastKey)
                    && MessageDigest.isEqual(newIv, lastIv)) {
                Arrays.fill(keyValue, (byte) 0);
                throw new InvalidAlgorithmParameterException(
                        "Cannot reuse iv for GCM encryption");
            }
            if (lastKey != null) {
                Arrays.fill(lastKey, (byte) 0);
            }
            lastKey = keyValue.clone();
            lastIv = newIv.clone();
        }

        if (this.keyBytes != null) {
            Arrays.fill(this.keyBytes, (byte) 0);
        }
        this.keyBytes = keyValue;
        this.iv = newIv;
        this.tagLenBytes = tagLen >> 3;
        this.encryption = encrypt;
        this.reInit = false;
        this.dataProcessed = false;
        this.initialized = true;
        aadBuffer.clearAndReset();
        dataBuffer.clearAndReset();
    }

    private static byte[] createIv(SecureRandom random) {
        byte[] iv = new byte[DEFAULT_IV_LEN];
        (random != null ? random : new SecureRandom()).nextBytes(iv);
        return iv;
    }

    private void checkInit() {
        if (!initialized) {
            throw new IllegalStateException("Operation not initialized.");
        }
    }

    private void checkReInit() {
        if (reInit) {
            throw new IllegalStateException(
                    "Must use either different key or iv for GCM encryption");
        }
    }

    @Override
    protected void engineUpdateAAD(byte[] src, int offset, int len) {
        checkInit();
        checkReInit();
        if (dataProcessed) {
            throw new IllegalStateException(
                    "AAD must be supplied before encryption/decryption starts");
        }
        aadBuffer.write(src, offset, len);
    }

    @Override
    protected void engineUpdateAAD(ByteBuffer src) {
        if (src == null) {
            throw new IllegalArgumentException("src ByteBuffer is null");
        }
        checkInit();
        checkReInit();
        if (dataProcessed) {
            throw new IllegalStateException(
                    "AAD must be supplied before encryption/decryption starts");
        }
        int len = src.remaining();
        if (len <= 0) {
            return;
        }
        if (src.hasArray()) {
            aadBuffer.write(src.array(), src.arrayOffset() + src.position(), len);
            src.position(src.limit());
        } else {
            // direct or read-only buffer: copy out (this also advances position)
            byte[] tmp = new byte[len];
            src.get(tmp);
            aadBuffer.write(tmp, 0, len);
        }
    }

    @Override
    protected byte[] engineUpdate(byte[] input, int inputOffset, int inputLen) {
        checkInit();
        checkReInit();
        dataProcessed = true;
        if (inputLen > 0) {
            dataBuffer.write(input, inputOffset, inputLen);
        }
        // GCM produces no output until the tag is available/verified.
        return new byte[0];
    }

    @Override
    protected int engineUpdate(byte[] input, int inputOffset, int inputLen,
                               byte[] output, int outputOffset) {
        engineUpdate(input, inputOffset, inputLen);
        return 0;
    }

    @Override
    protected byte[] engineDoFinal(byte[] input, int inputOffset, int inputLen)
            throws IllegalBlockSizeException, BadPaddingException {
        checkInit();
        checkReInit();
        dataProcessed = true;
        if (inputLen > 0) {
            dataBuffer.write(input, inputOffset, inputLen);
        }
        // Hand the internal arrays directly to doEncrypt/doDecrypt to avoid
        // the extra copy that toByteArray() would produce.
        byte[] aad  = aadBuffer.internalBuf();
        int    aadLen  = aadBuffer.internalCount();
        byte[] data = dataBuffer.internalBuf();
        int    dataLen = dataBuffer.internalCount();
        try {
            return encryption
                    ? doEncrypt(aad, aadLen, data, dataLen)
                    : doDecrypt(aad, aadLen, data, dataLen);
        } finally {
            // Clear sensitive content (plaintext on encrypt, ciphertext on decrypt)
            // then decide whether to keep or replace the backing array.
            aadBuffer.clearAndReset();
            dataBuffer.clearAndReset();
            if (dataBuffer.internalBuf().length > BUFFER_SHRINK_THRESHOLD) {
                dataBuffer = new ClearableBAOS();
            }
            if (aadBuffer.internalBuf().length > BUFFER_SHRINK_THRESHOLD) {
                aadBuffer = new ClearableBAOS();
            }
            // Reset the AAD-ordering guard so the same Cipher can process
            // another message (updateAAD + doFinal) without an explicit
            // re-init. IV-reuse for encryption is still blocked by reInit,
            // which doEncrypt sets on success.
            dataProcessed = false;
        }
    }

    @Override
    protected int engineDoFinal(byte[] input, int inputOffset, int inputLen,
                                byte[] output, int outputOffset)
            throws ShortBufferException, IllegalBlockSizeException,
                   BadPaddingException {
        checkInit();
        checkReInit();
        // Check capacity before doFinal consumes the buffered data and (on
        // encryption) sets the reInit guard, so a ShortBufferException can be
        // retried with a larger buffer.
        int estOutSize = engineGetOutputSize(inputLen);
        if (output.length - outputOffset < estOutSize) {
            throw new ShortBufferException("Output buffer must be at least "
                    + estOutSize + " bytes long");
        }
        byte[] result = engineDoFinal(input, inputOffset, inputLen);
        System.arraycopy(result, 0, output, outputOffset, result.length);
        return result.length;
    }

    private byte[] doEncrypt(byte[] aad, int aadLen, byte[] data, int dataLen) {
        long ctx = NativeSM4.gcmInit(true, keyBytes, iv);
        try {
            if (aadLen > 0) {
                NativeSM4.gcmUpdateAAD(ctx, aad, 0, aadLen);
            }
            byte[] out = new byte[Math.addExact(dataLen, tagLenBytes)];
            int n = (dataLen > 0)
                    ? NativeSM4.gcmUpdate(ctx, data, 0, dataLen, out, 0) : 0;
            NativeSM4.gcmEncryptDoFinal(ctx, out, n, tagLenBytes);
            reInit = true;   // same key+iv must not be reused for encryption
            return out;
        } finally {
            NativeSM4.gcmFree(ctx);
        }
    }

    private byte[] doDecrypt(byte[] aad, int aadLen, byte[] data, int dataLen)
            throws AEADBadTagException {
        if (dataLen < tagLenBytes) {
            throw new AEADBadTagException("Input too short - need tag");
        }
        int ctLen = dataLen - tagLenBytes;
        long ctx = NativeSM4.gcmInit(false, keyBytes, iv);
        try {
            if (aadLen > 0) {
                NativeSM4.gcmUpdateAAD(ctx, aad, 0, aadLen);
            }
            byte[] out = new byte[ctLen];
            int n = (ctLen > 0)
                    ? NativeSM4.gcmUpdate(ctx, data, 0, ctLen, out, 0) : 0;
            int m = NativeSM4.gcmDecryptDoFinal(ctx, out, n, data, ctLen,
                                                tagLenBytes);
            int total = n + m;
            return (total == out.length) ? out : Arrays.copyOf(out, total);
        } finally {
            NativeSM4.gcmFree(ctx);
        }
    }

    @Override
    protected int engineGetKeySize(Key key) throws InvalidKeyException {
        byte[] encoded = key.getEncoded();
        if (encoded == null) {
            throw new InvalidKeyException("Key encoding must not be null");
        }
        try {
            if (encoded.length != SM4_KEY_SIZE) {
                throw new InvalidKeyException("Invalid SM4 key length: "
                        + encoded.length + " bytes");
            }
        } finally {
            Arrays.fill(encoded, (byte) 0);
        }
        return SM4_KEY_SIZE * 8;
    }

    @Override
    protected byte[] engineWrap(Key key)
            throws IllegalBlockSizeException, InvalidKeyException {
        byte[] encoded = key.getEncoded();
        if (encoded == null || encoded.length == 0) {
            throw new InvalidKeyException("Cannot get an encoding of "
                    + "the key to be wrapped");
        }
        try {
            return engineDoFinal(encoded, 0, encoded.length);
        } catch (BadPaddingException e) {
            // should never happen for encryption
            throw new IllegalBlockSizeException(e.getMessage());
        } finally {
            Arrays.fill(encoded, (byte) 0);
        }
    }

    @Override
    protected Key engineUnwrap(byte[] wrappedKey, String wrappedKeyAlgorithm,
                               int wrappedKeyType)
            throws InvalidKeyException, NoSuchAlgorithmException {
        byte[] encoded = null;
        try {
            encoded = engineDoFinal(wrappedKey, 0, wrappedKey.length);
            return ConstructKeys.constructKey(encoded, wrappedKeyAlgorithm,
                    wrappedKeyType);
        } catch (IllegalBlockSizeException | BadPaddingException e) {
            throw new InvalidKeyException("The wrapped key is not padded "
                    + "correctly", e);
        } finally {
            if (encoded != null) {
                Arrays.fill(encoded, (byte) 0);
            }
        }
    }
}
