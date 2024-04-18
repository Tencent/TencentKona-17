/*
 * Copyright (c) 1999, 2013, Oracle and/or its affiliates. All rights reserved.
 * Copyright (c) 2015, 2020, Loongson Technology. All rights reserved.
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
 *
 * Please contact Oracle, 500 Oracle Parkway, Redwood Shores, CA 94065 USA
 * or visit www.oracle.com if you need additional information or have any
 * questions.
 *
 */

#ifndef CPU_MIPS_VM_GLOBALDEFINITIONS_MIPS_HPP
#define CPU_MIPS_VM_GLOBALDEFINITIONS_MIPS_HPP
// Size of MIPS Instructions
const int BytesPerInstWord = 4;

const int StackAlignmentInBytes = (2*wordSize);

// Indicates whether the C calling conventions require that
// 32-bit integer argument values are properly extended to 64 bits.
// If set, SharedRuntime::c_calling_convention() must adapt
// signatures accordingly.
const bool CCallingConventionRequiresIntsAsLongs = false;

#define SUPPORTS_NATIVE_CX8

#define SUPPORT_RESERVED_STACK_AREA

#define PREFERRED_METASPACE_ALIGNMENT

#define COMPRESSED_CLASS_POINTERS_DEPENDS_ON_COMPRESSED_OOPS false

#endif // CPU_MIPS_VM_GLOBALDEFINITIONS_MIPS_HPP
