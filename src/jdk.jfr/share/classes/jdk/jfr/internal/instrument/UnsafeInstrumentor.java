/*
 * Copyright (C) 2021, 2026, Tencent. All rights reserved.
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

package jdk.jfr.internal.instrument;

import jdk.jfr.events.Handlers;
import jdk.jfr.internal.handlers.EventHandler;

@JIInstrumentationTarget("jdk.internal.misc.Unsafe")
final class UnsafeInstrumentor {
    private UnsafeInstrumentor() {
    }

    @SuppressWarnings("deprecation")
    @JIInstrumentationMethod
    public long allocateMemory(long bytes) {
        EventHandler handler = Handlers.NATIVE_ALLOCATION;
        if (!handler.isEnabled()) {
            return allocateMemory(bytes);
        }
        long addr = 0;
        long start = 0;
        try {
            start = EventHandler.timestamp();
            addr = allocateMemory(bytes);
        } finally {
            long duration = EventHandler.timestamp() - start;
            if (handler.shouldCommit(duration)) {
                if (addr != 0) {
                    handler.write(start, duration, addr, bytes);
                }
            }
        }
        return addr;
    }

    @SuppressWarnings("deprecation")
    @JIInstrumentationMethod
    public long reallocateMemory(long address, long bytes) {
        EventHandler handler = Handlers.NATIVE_REALLOCATE;
        if (!handler.isEnabled()) {
            return reallocateMemory(address, bytes);
        }
        long addr = 0;
        long start = 0;
        try {
            start = EventHandler.timestamp();
            addr = reallocateMemory(address, bytes);
        } finally {
            long duration = EventHandler.timestamp() - start;
            if (handler.shouldCommit(duration)) {
                if (addr != 0) {
                    handler.write(start, duration, address, addr, bytes);
                }
            }
        }
        return addr;
    }

    @SuppressWarnings("deprecation")
    @JIInstrumentationMethod
    public void freeMemory(long address) {
        EventHandler handler = Handlers.NATIVE_FREE;
        if (!handler.isEnabled()) {
            freeMemory(address);
            return;
        }
        long start = 0;
        try {
            start = EventHandler.timestamp();
            freeMemory(address);
        } finally {
            long duration = EventHandler.timestamp() - start;
            if (handler.shouldCommit(duration)) {
                if (address != 0) {
                    handler.write(start, duration, address);
                }
            }
        }
    }
}
