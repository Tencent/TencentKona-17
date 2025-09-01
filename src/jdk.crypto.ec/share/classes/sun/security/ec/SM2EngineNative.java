/*
 * Copyright (C) 2022, 2024, THL A29 Limited, a Tencent company. All rights reserved.
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

import sun.security.util.ECUtil;

import javax.crypto.BadPaddingException;
import javax.crypto.IllegalBlockSizeException;
import java.security.spec.SM2ParameterSpec;

final class SM2EngineNative extends SM2Engine {

    byte[] encrypt(byte[] input, int inputOffset, int inputLen)
            throws IllegalBlockSizeException, BadPaddingException {
        byte[] pubKey = ECUtil.encodePoint(publicKey.getW(), SM2ParameterSpec.CURVE);
        byte[] paddedPubKey = NativeSunEC.padZerosForValuePair(pubKey, 1, 32);
        return NativeSunEC.sm2Encrypt(paddedPubKey, input, inputOffset, inputLen);
    }

    byte[] decrypt(byte[] input, int inputOffset, int inputLen)
            throws IllegalBlockSizeException, BadPaddingException {
        byte[] privKey = privateKey.getS().toByteArray();
        return NativeSunEC.sm2Decrypt(privKey, input, inputOffset, inputLen);
    }
}
