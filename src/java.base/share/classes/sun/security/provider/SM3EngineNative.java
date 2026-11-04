/*
 * Copyright (C) 2022, 2026, Tencent. All rights reserved.
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

import jdk.internal.ref.CleanerFactory;

import java.lang.ref.Reference;

/**
 * The native SM3 engine backed by OpenSSL's EVP_MD_CTX.
 *
 * <p>Each {@code update} call feeds data directly to the native context via
 * {@code EVP_DigestUpdate}, avoiding the intermediate
 * {@code ByteArrayOutputStream} copy used by the old one-shot implementation.
 * {@code doFinal} calls {@code EVP_DigestFinal_ex} and then reinitialises the
 * context so the engine can be reused without a new {@code sm3Init} call.
 *
 * <p>The native {@code EVP_MD_CTX} is registered with a {@link Cleaner} so it
 * is released even if the engine is abandoned without a {@code doFinal}.
 */
public final class SM3EngineNative implements SM3Engine {

    /**
     * Wraps the native EVP_MD_CTX pointer and acts as the Cleaner's action.
     * Both {@link #free} and {@link #run} are idempotent.
     */
    private static final class Ctx implements Runnable {
        private long handle;

        Ctx(long handle) {
            this.handle = handle;
        }

        synchronized long get() {
            return handle;
        }

        synchronized void free() {
            if (handle != 0) {
                NativeSun.sm3Free(handle);
                handle = 0;
            }
        }

        @Override
        public void run() {
            free();
        }
    }

    private final Ctx nativeCtx;
    // Reused buffer for single-byte update calls. MessageDigest is not
    // thread-safe, so this instance field is safe to reuse across calls.
    private final byte[] singleByte = new byte[1];

    public SM3EngineNative() {
        this(NativeSun.sm3Init());
    }

    private SM3EngineNative(long existingHandle) {
        // Guard against OOME in Ctx allocation or Cleaner registration:
        // if either step fails, free the already-allocated native ctx so it
        // is not leaked. The catch block always rethrows, so nativeCtx is
        // definitely assigned on every normal-completion path.
        try {
            nativeCtx = new Ctx(existingHandle);
            CleanerFactory.cleaner().register(this, nativeCtx);
        } catch (OutOfMemoryError oom) {
            NativeSun.sm3Free(existingHandle);
            throw oom;
        }
    }

    @Override
    public void reset() {
        try {
            NativeSun.sm3Reinit(nativeCtx.get());
        } finally {
            Reference.reachabilityFence(this);
        }
    }

    @Override
    public void update(byte message) {
        singleByte[0] = message;
        try {
            NativeSun.sm3Update(nativeCtx.get(), singleByte, 0, 1);
        } finally {
            Reference.reachabilityFence(this);
        }
    }

    @Override
    public void update(byte[] message) {
        try {
            NativeSun.sm3Update(nativeCtx.get(), message, 0, message.length);
        } finally {
            Reference.reachabilityFence(this);
        }
    }

    @Override
    public void update(byte[] message, int offset, int length) {
        try {
            NativeSun.sm3Update(nativeCtx.get(), message, offset, length);
        } finally {
            Reference.reachabilityFence(this);
        }
    }

    @Override
    public void doFinal(byte[] out) {
        doFinal(out, 0);
    }

    // SM3MessageDigest already validates bounds; no redundant check here.
    @Override
    public void doFinal(byte[] out, int outOffset) {
        try {
            NativeSun.sm3DoFinal(nativeCtx.get(), out, outOffset);
        } finally {
            Reference.reachabilityFence(this);
        }
    }

    @Override
    public byte[] doFinal() {
        byte[] digest = new byte[32];
        doFinal(digest, 0);
        return digest;
    }

    @Override
    public SM3EngineNative clone() throws CloneNotSupportedException {
        long copyHandle = 0;
        try {
            copyHandle = NativeSun.sm3Copy(nativeCtx.get());
        } finally {
            Reference.reachabilityFence(this);
        }
        try {
            return new SM3EngineNative(copyHandle);
        } catch (OutOfMemoryError oom) {
            NativeSun.sm3Free(copyHandle);
            throw oom;
        }
    }
}
