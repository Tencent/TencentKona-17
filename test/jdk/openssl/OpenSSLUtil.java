/*
 * Copyright (C) 2025, THL A29 Limited, a Tencent company. All rights reserved.
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
import java.util.Locale;

public class OpenSSLUtil {

    public static String opensslCryptoPath() {
        String osName = System.getProperty("os.name").toLowerCase(Locale.ROOT);
        String os = "unsupported";
        String ext = "unsupported";
        if (osName.contains("linux")) {
            os = "linux";
            ext = ".so";
        }

        String archName = System.getProperty("os.arch").toLowerCase(Locale.ROOT);
        String arch = "unsupported";
        if (archName.contains("x86_64") || archName.contains("amd64")) {
            arch = "x86_64";
        } else if (archName.contains("aarch64") || archName.contains("arm64")) {
            arch = "aarch64";
        }

        String platformName = os + "-" + arch;
        String libName = "libopensslcrypto" + ext;

        Path testRoot = Paths.get(System.getProperty("test.root"));
        Path libPath = testRoot.resolve("openssl").resolve("lib")
                .resolve(platformName).resolve(libName);
        return Files.exists(libPath) ? libPath.toString() : null;
    }
}
