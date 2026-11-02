/*
 * Copyright (C) 2025, 2026, Tencent. All rights reserved.
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

package sun.security.provider;

/**
 * Exposes NativeSun's internal SM3 streaming API for testing.
 */
public class NativeSunWrapper {

    public static boolean isNativeCryptoEnabled() {
        return NativeSun.isNativeCryptoEnabled();
    }

    public static long sm3Init() {
        return NativeSun.sm3Init();
    }

    public static void sm3Update(long ctx, byte[] in, int inOfs, int inLen) {
        NativeSun.sm3Update(ctx, in, inOfs, inLen);
    }

    public static void sm3DoFinal(long ctx, byte[] out, int outOfs) {
        NativeSun.sm3DoFinal(ctx, out, outOfs);
    }

    public static void sm3Reinit(long ctx) {
        NativeSun.sm3Reinit(ctx);
    }

    public static long sm3Copy(long ctx) {
        return NativeSun.sm3Copy(ctx);
    }

    public static void sm3Free(long ctx) {
        NativeSun.sm3Free(ctx);
    }
}
