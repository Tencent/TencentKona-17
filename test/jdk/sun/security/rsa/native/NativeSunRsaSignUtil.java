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

import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.*;

public class NativeSunRsaSignUtil {

    public static boolean nativeCryptoSupported() {
        String opensslCryptoPath = OpenSSLUtil.opensslCryptoPath();
        boolean supported = nativeSunRsaSignSupported() && opensslCryptoPath != null;
        if (supported) {
            System.setProperty("jdk.openssl.cryptoLibPath", opensslCryptoPath);
        }
        return supported;
    }

    private static boolean nativeSunRsaSignSupported() {
        Path jdkLibDir = Paths.get(System.getProperty("test.jdk")).resolve("lib");
        Path sunrsasigncryptoLinuxPath = jdkLibDir.resolve("libsunrsasigncrypto.so");
        Path sunrsasigncryptoMacPath = jdkLibDir.resolve("libsunrsasigncrypto.dylib");
        return Files.exists(sunrsasigncryptoLinuxPath) || Files.exists(sunrsasigncryptoMacPath);
    }

    public static void execTaskSerially(Callable<Void> task, int count)
            throws Exception {
        for (int i = 0; i < count; i++) {
            task.call();
        }
    }

    public static void execTaskSerially(Callable<Void> task)
            throws Exception{
        execTaskSerially(task, 10);
    }

    public static void execTaskParallelly(Callable<Void> task, int count)
            throws Exception {
        List<Callable<Void>> tasks = new ArrayList<>(count);
        for (int i = 0; i < count; i++) {
            tasks.add(task);
        }

        ExecutorService executorService = Executors.newFixedThreadPool(count);
        try {
            List<Future<Void>> futures = executorService.invokeAll(tasks);
            futures.forEach(future -> {
                try {
                    future.get();
                } catch (InterruptedException | ExecutionException e) {
                    throw new RuntimeException("Run task failed", e);
                }
            });
        } finally {
            executorService.shutdown();
        }
    }

    public static void execTaskParallelly(Callable<Void> task)
            throws Exception {
        execTaskParallelly(task, 10);
    }
}
