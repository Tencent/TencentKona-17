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

#ifndef SUNEC_UTIL_H
#define SUNEC_UTIL_H

#ifdef __cplusplus
extern "C" {
#endif

#include "jni.h"

#define OPENSSL_SUCCESS       1
#define OPENSSL_FAILURE       0

#define SM2_FIELD_LEN        32
#define SM2_PRI_KEY_LEN      32
#define SM2_PUB_KEY_LEN      65
#define SM2_COMP_PUB_KEY_LEN 33
#define SM3_DIGEST_LEN       32

#define ILLEGAL_STATE_EXCEPTION "java/lang/IllegalStateException"
#define INVALID_ALGO_PARAM_EXCEPTION "java/security/InvalidAlgorithmParameterException"
#define INVALID_KEY_EXCEPTION "java/security/InvalidKeyException"
#define NULL_POINTER_EXCEPTION "java/lang/NullPointerException"
#define PROVIDER_EXCEPTION "java/security/ProviderException"
#define SIGNATURE_EXCEPTION "java/security/SignatureException"

void sunec_throw(JNIEnv *env, const char *exceptionName, const char *message);

#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */

#endif /* SUNEC_UTIL_H */
