/*
 * Copyright (C) 2022, 2024, Tencent. All rights reserved.
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

package sun.security.provider;

import java.io.ByteArrayOutputStream;

/**
 * The native SM3 engine.
 */
public final class SM3EngineNative implements SM3Engine {

    private ByteArrayOutputStream buffer = new ByteArrayOutputStream();

    public void reset() {
        buffer.reset();
    }

    public void update(byte message) {
        buffer.write(message);
    }

    public void update(byte[] message) {
        buffer.writeBytes(message);
    }

    public void update(byte[] message, int offset, int length) {
        buffer.write(message, offset, length);
    }

    public void doFinal(byte[] out) {
        doFinal(out, 0);
    }

    // Not check the out bounds, because SM3MessageDigest already does that.
    public void doFinal(byte[] out, int outOffset) {
        byte[] message = buffer.toByteArray();
        NativeSun.sm3Digest(message, out, outOffset);
        reset();
    }

    public byte[] doFinal() {
        byte[] digest = new byte[32];
        doFinal(digest);
        return digest;
    }

    public SM3EngineNative clone() throws CloneNotSupportedException {
        SM3EngineNative clone = (SM3EngineNative) super.clone();
        clone.buffer = new ByteArrayOutputStream();
        clone.buffer.writeBytes(this.buffer.toByteArray());
        return clone;
    }
}
