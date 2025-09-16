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

import javax.crypto.BadPaddingException;
import javax.crypto.IllegalBlockSizeException;
import java.security.SecureRandom;
import java.security.interfaces.ECKey;

abstract class SM2Engine {

    SM2PublicKey publicKey;
    SM2PrivateKey privateKey;
    SecureRandom random;

    private boolean encrypted;

    void init(boolean encrypted, ECKey key, SecureRandom random) {
        publicKey = null;
        privateKey = null;

        if (encrypted) {
            publicKey = (SM2PublicKey) key;
        } else {
            privateKey = (SM2PrivateKey) key;
        }
        this.random = random;

        this.encrypted = encrypted;
    }

    boolean encrypted() {
        return encrypted;
    }

    byte[] processBlock(byte[] input, int inputOffset, int inputLen)
            throws IllegalBlockSizeException, BadPaddingException {
        if (!checkInputBound(input, inputOffset, inputLen)) {
            throw new BadPaddingException("Invalid input");
        }

        if (encrypted) {
            return encrypt(input, inputOffset, inputLen);
        } else {
            return decrypt(input, inputOffset, inputLen);
        }
    }

    private static boolean checkInputBound(byte[] input, int offset, int len) {
        return input != null
                && offset >= 0 && len > 0
                && (input.length >= (offset + len));
    }

    abstract byte[] encrypt(byte[] input, int inputOffset, int inputLen)
            throws IllegalBlockSizeException, BadPaddingException;

    abstract byte[] decrypt(byte[] input, int inputOffset, int inputLen)
            throws IllegalBlockSizeException, BadPaddingException;
}
