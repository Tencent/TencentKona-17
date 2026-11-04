/*
 * Copyright (c) 1997, 2014, Oracle and/or its affiliates. All rights reserved.
 * Copyright (c) 2015, 2021, Loongson Technology. All rights reserved.
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

#include "precompiled.hpp"
#include "asm/macroAssembler.hpp"
#include "asm/macroAssembler.inline.hpp"
#include "memory/resourceArea.hpp"
#include "runtime/arguments.hpp"
#include "runtime/java.hpp"
#include "runtime/stubCodeGenerator.hpp"
#include "runtime/vm_version.hpp"
#include "os_linux.hpp"
#ifdef TARGET_OS_FAMILY_linux
# include "os_linux.inline.hpp"
#endif

#define A0 RA0
#define A1 RA1
#define A2 RA2
#define A3 RA3
#define A4 RA4
#define A5 RA5
#define A6 RA6
#define A7 RA7
#define T0 RT0
#define T1 RT1
#define T2 RT2
#define T3 RT3
#define T4 RT4
#define T5 RT5
#define T6 RT6
#define T7 RT7
#define T8 RT8

VM_Version::CpuidInfo VM_Version::_cpuid_info   = { 0, };
bool VM_Version::_cpu_info_is_initialized = false;

static BufferBlob* stub_blob;
static const int stub_size = 600;

extern "C" {
  typedef void (*get_cpu_info_stub_t)(void*);
}
static get_cpu_info_stub_t get_cpu_info_stub = NULL;


class VM_Version_StubGenerator: public StubCodeGenerator {
 public:

  VM_Version_StubGenerator(CodeBuffer *c) : StubCodeGenerator(c) {}

  address generate_get_cpu_info() {
    assert(!VM_Version::cpu_info_is_initialized(), "VM_Version should not be initialized");
    StubCodeMark mark(this, "VM_Version", "get_cpu_info_stub");
#   define __ _masm->

    address start = __ pc();

    __ cpucfg(AT, R0);
    __ st_w(AT, Address(A0, VM_Version::cpucfg_info_id0_offset()));

    __ li(AT, 0x1);
    __ cpucfg(AT, AT);
    __ st_w(AT, Address(A0, VM_Version::cpucfg_info_id1_offset()));

    __ li(AT, 0x2);
    __ cpucfg(AT, AT);
    __ st_w(AT, Address(A0, VM_Version::cpucfg_info_id2_offset()));

    __ li(AT, 0x3);
    __ cpucfg(AT, AT);
    __ st_w(AT, Address(A0, VM_Version::cpucfg_info_id3_offset()));

    __ li(AT, 0x4);
    __ cpucfg(AT, AT);
    __ st_w(AT, Address(A0, VM_Version::cpucfg_info_id4_offset()));

    __ li(AT, 0x5);
    __ cpucfg(AT, AT);
    __ st_w(AT, Address(A0, VM_Version::cpucfg_info_id5_offset()));

    __ li(AT, 0x6);
    __ cpucfg(AT, AT);
    __ st_w(AT, Address(A0, VM_Version::cpucfg_info_id6_offset()));

    __ li(AT, 0x10);
    __ cpucfg(AT, AT);
    __ st_w(AT, Address(A0, VM_Version::cpucfg_info_id10_offset()));

    __ li(AT, 0x11);
    __ cpucfg(AT, AT);
    __ st_w(AT, Address(A0, VM_Version::cpucfg_info_id11_offset()));

    __ li(AT, 0x12);
    __ cpucfg(AT, AT);
    __ st_w(AT, Address(A0, VM_Version::cpucfg_info_id12_offset()));

    __ li(AT, 0x13);
    __ cpucfg(AT, AT);
    __ st_w(AT, Address(A0, VM_Version::cpucfg_info_id13_offset()));

    __ li(AT, 0x14);
    __ cpucfg(AT, AT);
    __ st_w(AT, Address(A0, VM_Version::cpucfg_info_id14_offset()));

    __ jr(RA);
#   undef __
    return start;
  };
};

uint32_t VM_Version::get_feature_flags_by_cpucfg() {
  uint32_t result = 0;
  if (_cpuid_info.cpucfg_info_id1.bits.ARCH == 0b00 || _cpuid_info.cpucfg_info_id1.bits.ARCH == 0b01 ) {
    result |= CPU_LA32;
  } else if (_cpuid_info.cpucfg_info_id1.bits.ARCH == 0b10 ) {
    result |= CPU_LA64;
  }

  if (_cpuid_info.cpucfg_info_id2.bits.FP_CFG != 0)
    result |= CPU_FP;

  if (_cpuid_info.cpucfg_info_id3.bits.CCDMA != 0)
    result |= CPU_CCDMA;
  if (_cpuid_info.cpucfg_info_id3.bits.LLDBAR != 0)
    result |= CPU_LLDBAR;
  if (_cpuid_info.cpucfg_info_id3.bits.SCDLY != 0)
    result |= CPU_SCDLY;
  if (_cpuid_info.cpucfg_info_id3.bits.LLEXC != 0)
    result |= CPU_LLEXC;

  result |= CPU_ULSYNC;

  return result;
}

void VM_Version::get_processor_features() {

  clean_cpuFeatures();

  get_os_cpu_info();

  get_cpu_info_stub(&_cpuid_info);
  _features |= get_feature_flags_by_cpucfg();

  _supports_cx8 = true;

  if (supports_lsx()) {
    if (FLAG_IS_DEFAULT(UseLSX)) {
      FLAG_SET_DEFAULT(UseLSX, true);
    }
  } else if (UseLSX) {
    warning("LSX instructions are not available on this CPU");
    FLAG_SET_DEFAULT(UseLSX, false);
  }

  if (supports_lasx()) {
    if (FLAG_IS_DEFAULT(UseLASX)) {
      FLAG_SET_DEFAULT(UseLASX, true);
    }
  } else if (UseLASX) {
    warning("LASX instructions are not available on this CPU");
    FLAG_SET_DEFAULT(UseLASX, false);
  }

  if (UseLASX && !UseLSX) {
    warning("LASX instructions depends on LSX, setting UseLASX to false");
    FLAG_SET_DEFAULT(UseLASX, false);
  }

#ifdef COMPILER2
  int max_vector_size = 0;
  int min_vector_size = 0;
  if (UseLASX) {
    max_vector_size = 32;
    min_vector_size = 16;
  }
  else if (UseLSX) {
    max_vector_size = 16;
    min_vector_size = 16;
  }

  if (!FLAG_IS_DEFAULT(MaxVectorSize)) {
    if (MaxVectorSize == 0) {
      // do nothing
    } else if (MaxVectorSize > max_vector_size) {
      warning("MaxVectorSize must be at most %i on this platform", max_vector_size);
      FLAG_SET_DEFAULT(MaxVectorSize, max_vector_size);
    } else if (MaxVectorSize < min_vector_size) {
      warning("MaxVectorSize must be at least %i or 0 on this platform, setting to: %i", min_vector_size, min_vector_size);
      FLAG_SET_DEFAULT(MaxVectorSize, min_vector_size);
    } else if (!is_power_of_2(MaxVectorSize)) {
      warning("MaxVectorSize must be a power of 2, setting to default: %i", max_vector_size);
      FLAG_SET_DEFAULT(MaxVectorSize, max_vector_size);
    }
  } else {
    // If default, use highest supported configuration
    FLAG_SET_DEFAULT(MaxVectorSize, max_vector_size);
  }
#endif

  char buf[256];

  // A note on the _features_string format:
  //   There are jtreg tests checking the _features_string for various properties.
  //   For some strange reason, these tests require the string to contain
  //   only _lowercase_ characters. Keep that in mind when being surprised
  //   about the unusual notation of features - and when adding new ones.
  //   Features may have one comma at the end.
  //   Furthermore, use one, and only one, separator space between features.
  //   Multiple spaces are considered separate tokens, messing up everything.
  jio_snprintf(buf, sizeof(buf), "%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s, "
    "0x%lx, fp_ver: %d, lvz_ver: %d, ",
    (is_la64()             ?  "la64"  : ""),
    (is_la32()             ?  "la32"  : ""),
    (supports_lsx()        ?  ", lsx" : ""),
    (supports_lasx()       ?  ", lasx" : ""),
    (supports_crypto()     ?  ", crypto" : ""),
    (supports_lam()        ?  ", am" : ""),
    (supports_ual()        ?  ", ual" : ""),
    (supports_lldbar()     ?  ", lldbar" : ""),
    (supports_scdly()      ?  ", scdly" : ""),
    (supports_llexc()      ?  ", llexc" : ""),
    (supports_lbt_x86()    ?  ", lbt_x86" : ""),
    (supports_lbt_arm()    ?  ", lbt_arm" : ""),
    (supports_lbt_mips()   ?  ", lbt_mips" : ""),
    (needs_llsync()        ?  ", needs_llsync" : ""),
    (needs_tgtsync()       ?  ", needs_tgtsync": ""),
    (needs_ulsync()        ?  ", needs_ulsync": ""),
    _cpuid_info.cpucfg_info_id0.bits.PRID,
    _cpuid_info.cpucfg_info_id2.bits.FP_VER,
    _cpuid_info.cpucfg_info_id2.bits.LVZ_VER);
  _features_string = os::strdup(buf);

  assert(!is_la32(), "Should Not Reach Here, what is the cpu type?");
  assert( is_la64(), "Should be LoongArch64");

  int dcache_line = 1 << _cpuid_info.cpucfg_info_id12.bits.LINESIZELOG2;

  // Limit AllocatePrefetchDistance so that it does not exceed the
  // static constraint of 512 defined in runtime/globals.hpp.
  if (FLAG_IS_DEFAULT(AllocatePrefetchDistance)) {
    // Based on RawAllocationRate results.
    FLAG_SET_DEFAULT(AllocatePrefetchDistance, MIN2(512, 3*dcache_line));
  }

  if (FLAG_IS_DEFAULT(AllocatePrefetchStepSize)) {
    FLAG_SET_DEFAULT(AllocatePrefetchStepSize, dcache_line);
  }

  // Prefetch tuning based on SPECjbb2015 results.
  //
  // Mechanism:
  // - SCAN: Hides memory read latency during object iteration.
  // - COPY: Hides RFO write latency during object evacuation.
  // Reducing these latencies directly shortens STW pauses, significantly
  // boosting Critical-jOPS.
  //
  // 1. Single-NUMA Architectures:
  //    - 192 bytes (3 cache lines) is the performance peak, delivering
  //      an approximate 40% Critical-jOPS boost over the baseline,
  //      most notably in adaptive young generation heap scenarios.
  //    - 256 bytes is a close second.
  //    - 576 bytes causes a ~6.7% regression compared to 192 due to
  //      cache pollution, though it still outperforms the baseline.
  //
  // 2. Multi-NUMA Architectures:
  //    Larger intervals (e.g., 576 bytes) help mitigate the higher
  //    latency associated with cross-node memory access.
  //
  // Trade-off:
  // We prioritize single-NUMA peak performance. The 192-byte setting
  // offers the maximum uplift for mainstream single-node configurations
  // while remaining performant on multi-NUMA systems.
  if (FLAG_IS_DEFAULT(PrefetchCopyIntervalInBytes)) {
    FLAG_SET_DEFAULT(PrefetchCopyIntervalInBytes, 3*dcache_line);
  }

  if (FLAG_IS_DEFAULT(PrefetchScanIntervalInBytes)) {
    FLAG_SET_DEFAULT(PrefetchScanIntervalInBytes, 3*dcache_line);
  }

  // Basic instructions are used to implement SHA Intrinsics on LA, so sha
  // instructions support is not needed.
  if (/*supports_crypto()*/ 1) {
    if (FLAG_IS_DEFAULT(UseSHA)) {
      FLAG_SET_DEFAULT(UseSHA, true);
    }
  } else if (UseSHA) {
    warning("SHA instructions are not available on this CPU");
    FLAG_SET_DEFAULT(UseSHA, false);
  }

  if (UseSHA/* && supports_crypto()*/) {
    if (FLAG_IS_DEFAULT(UseSHA1Intrinsics)) {
      FLAG_SET_DEFAULT(UseSHA1Intrinsics, true);
    }
  } else if (UseSHA1Intrinsics) {
    warning("Intrinsics for SHA-1 crypto hash functions not available on this CPU.");
    FLAG_SET_DEFAULT(UseSHA1Intrinsics, false);
  }

  if (UseSHA/* && supports_crypto()*/) {
    if (FLAG_IS_DEFAULT(UseSHA256Intrinsics)) {
      FLAG_SET_DEFAULT(UseSHA256Intrinsics, true);
    }
  } else if (UseSHA256Intrinsics) {
    warning("Intrinsics for SHA-224 and SHA-256 crypto hash functions not available on this CPU.");
    FLAG_SET_DEFAULT(UseSHA256Intrinsics, false);
  }

  if (UseSHA512Intrinsics) {
    warning("Intrinsics for SHA-384 and SHA-512 crypto hash functions not available on this CPU.");
    FLAG_SET_DEFAULT(UseSHA512Intrinsics, false);
  }

  if (UseSHA3Intrinsics) {
    warning("Intrinsics for SHA3-224, SHA3-256, SHA3-384 and SHA3-512 crypto hash functions not available on this CPU.");
    FLAG_SET_DEFAULT(UseSHA3Intrinsics, false);
  }

  if (!(UseSHA1Intrinsics || UseSHA256Intrinsics || UseSHA3Intrinsics || UseSHA512Intrinsics)) {
    FLAG_SET_DEFAULT(UseSHA, false);
  }

  if (FLAG_IS_DEFAULT(UseMD5Intrinsics)) {
    FLAG_SET_DEFAULT(UseMD5Intrinsics, true);
  }

  // Basic instructions are used to implement AES Intrinsics on LA, so AES
  // instructions support is not needed.
  if (/*supports_crypto()*/ 1) {
    if (FLAG_IS_DEFAULT(UseAES)) {
      FLAG_SET_DEFAULT(UseAES, true);
    }
  } else if (UseAES) {
    if (!FLAG_IS_DEFAULT(UseAES))
      warning("AES instructions are not available on this CPU");
    FLAG_SET_DEFAULT(UseAES, false);
  }

  if (UseAES/* && supports_crypto()*/) {
    if (FLAG_IS_DEFAULT(UseAESIntrinsics)) {
      FLAG_SET_DEFAULT(UseAESIntrinsics, true);
    }
  } else if (UseAESIntrinsics) {
    if (!FLAG_IS_DEFAULT(UseAESIntrinsics))
      warning("AES intrinsics are not available on this CPU");
    FLAG_SET_DEFAULT(UseAESIntrinsics, false);
  }

  if (UseAESCTRIntrinsics) {
    warning("AES/CTR intrinsics are not available on this CPU");
    FLAG_SET_DEFAULT(UseAESCTRIntrinsics, false);
  }

  if (FLAG_IS_DEFAULT(UseCRC32)) {
    FLAG_SET_DEFAULT(UseCRC32, true);
  }

  if (UseCRC32) {
    if (FLAG_IS_DEFAULT(UseCRC32Intrinsics)) {
      UseCRC32Intrinsics = true;
    }

    if (FLAG_IS_DEFAULT(UseCRC32CIntrinsics)) {
      UseCRC32CIntrinsics = true;
    }
  }

#ifdef COMPILER2
  if (FLAG_IS_DEFAULT(UseMulAddIntrinsic)) {
    FLAG_SET_DEFAULT(UseMulAddIntrinsic, true);
  }

  if (FLAG_IS_DEFAULT(UseMontgomeryMultiplyIntrinsic)) {
    UseMontgomeryMultiplyIntrinsic = true;
  }
  if (FLAG_IS_DEFAULT(UseMontgomerySquareIntrinsic)) {
    UseMontgomerySquareIntrinsic = true;
  }
#endif

  // This machine allows unaligned memory accesses
  if (FLAG_IS_DEFAULT(UseUnalignedAccesses)) {
    FLAG_SET_DEFAULT(UseUnalignedAccesses, true);
  }

  if (FLAG_IS_DEFAULT(UseFMA)) {
    FLAG_SET_DEFAULT(UseFMA, true);
  }

  if (FLAG_IS_DEFAULT(UseCopySignIntrinsic)) {
    FLAG_SET_DEFAULT(UseCopySignIntrinsic, true);
  }

  if (UseLSX) {
    if (FLAG_IS_DEFAULT(UsePopCountInstruction)) {
      FLAG_SET_DEFAULT(UsePopCountInstruction, true);
    }
  } else if (UsePopCountInstruction) {
    if (!FLAG_IS_DEFAULT(UsePopCountInstruction))
      warning("PopCountI/L/VI(4) employs LSX whereas PopCountVI(8) hinges on LASX.");
    FLAG_SET_DEFAULT(UsePopCountInstruction, false);
  }

  if (UseActiveCoresMP) {
    if (os::Linux::sched_active_processor_count() != 1) {
      if (!FLAG_IS_DEFAULT(UseActiveCoresMP))
        warning("UseActiveCoresMP disabled because active processors are more than one.");
      FLAG_SET_DEFAULT(UseActiveCoresMP, false);
    }
  } else { // !UseActiveCoresMP
    if (FLAG_IS_DEFAULT(UseActiveCoresMP) && os::Linux::sched_active_processor_count() == 1) {
      FLAG_SET_DEFAULT(UseActiveCoresMP, true);
    }
  }

  UNSUPPORTED_OPTION(CriticalJNINatives);
}

void VM_Version::initialize() {
  ResourceMark rm;
  // Making this stub must be FIRST use of assembler

  stub_blob = BufferBlob::create("get_cpu_info_stub", stub_size);
  if (stub_blob == NULL) {
    vm_exit_during_initialization("Unable to allocate get_cpu_info_stub");
  }
  CodeBuffer c(stub_blob);
  VM_Version_StubGenerator g(&c);
  get_cpu_info_stub = CAST_TO_FN_PTR(get_cpu_info_stub_t,
                                     g.generate_get_cpu_info());

  get_processor_features();
}
