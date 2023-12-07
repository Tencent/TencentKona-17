/*
 * Copyright (c) 2019, 2021, Oracle and/or its affiliates. All rights reserved.
 * Copyright (c) 2021, 2022, Loongson Technology. All rights reserved.
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
 */

#include "precompiled.hpp"
#include "asm/macroAssembler.inline.hpp"
#include "code/codeBlob.hpp"
#include "code/vmreg.inline.hpp"
#include "gc/z/zBarrier.inline.hpp"
#include "gc/z/zBarrierSet.hpp"
#include "gc/z/zBarrierSetAssembler.hpp"
#include "gc/z/zBarrierSetRuntime.hpp"
#include "gc/z/zThreadLocalData.hpp"
#include "memory/resourceArea.hpp"
#include "runtime/sharedRuntime.hpp"
#include "utilities/macros.hpp"
#ifdef COMPILER1
#include "c1/c1_LIRAssembler.hpp"
#include "c1/c1_MacroAssembler.hpp"
#include "gc/z/c1/zBarrierSetC1.hpp"
#endif // COMPILER1
#ifdef COMPILER2
#include "gc/z/c2/zBarrierSetC2.hpp"
#endif // COMPILER2

#ifdef PRODUCT
#define BLOCK_COMMENT(str) /* nothing */
#else
#define BLOCK_COMMENT(str) __ block_comment(str)
#endif

#undef __
#define __ masm->

#define A0 RA0
#define A1 RA1
#define T4 RT4

void ZBarrierSetAssembler::load_at(MacroAssembler* masm,
                                   DecoratorSet decorators,
                                   BasicType type,
                                   Register dst,
                                   Address src,
                                   Register tmp1,
                                   Register tmp_thread) {
  if (!ZBarrierSet::barrier_needed(decorators, type)) {
    // Barrier not needed
    BarrierSetAssembler::load_at(masm, decorators, type, dst, src, tmp1, tmp_thread);
    return;
  }

  // Allocate scratch register
  Register scratch = tmp1;

  assert_different_registers(dst, scratch, SCR1);

  Label done;

  //
  // Fast Path
  //

  // Load address
  __ lea(scratch, src);

  // Load oop at address
  __ ld_ptr(dst, scratch, 0);

  // Test address bad mask
  __ ld_ptr(SCR1, address_bad_mask_from_thread(TREG));
  __ andr(SCR1, dst, SCR1);
  __ beqz(SCR1, done);

  //
  // Slow path
  //
  __ enter();

  if (dst != V0) {
    __ push(V0);
  }
  __ push_call_clobbered_registers_except(RegSet::of(V0));

  if (dst != A0) {
    __ move(A0, dst);
  }
  __ move(A1, scratch);
  __ MacroAssembler::call_VM_leaf_base(ZBarrierSetRuntime::load_barrier_on_oop_field_preloaded_addr(decorators), 2);

  __ pop_call_clobbered_registers_except(RegSet::of(V0));

  // Make sure dst has the return value.
  if (dst != V0) {
    __ move(dst, V0);
    __ pop(V0);
  }
  __ leave();

  __ bind(done);
}

#ifdef ASSERT

void ZBarrierSetAssembler::store_at(MacroAssembler* masm,
                                        DecoratorSet decorators,
                                        BasicType type,
                                        Address dst,
                                        Register val,
                                        Register tmp1,
                                        Register tmp2) {
  // Verify value
  if (is_reference_type(type)) {
    // Note that src could be noreg, which means we
    // are storing null and can skip verification.
    if (val != noreg) {
      Label done;

      // tmp1 and tmp2 are often set to noreg.

      __ ld_ptr(AT, address_bad_mask_from_thread(TREG));
      __ andr(AT, val, AT);
      __ beqz(AT, done);
      __ stop("Verify oop store failed");
      __ should_not_reach_here();
      __ bind(done);
    }
  }

  // Store value
  BarrierSetAssembler::store_at(masm, decorators, type, dst, val, tmp1, tmp2);
}

#endif // ASSERT

void ZBarrierSetAssembler::arraycopy_prologue(MacroAssembler* masm,
                                              DecoratorSet decorators,
                                              bool is_oop,
                                              Register src,
                                              Register dst,
                                              Register count,
                                              RegSet saved_regs) {
  if (!is_oop) {
    // Barrier not needed
    return;
  }

  BLOCK_COMMENT("ZBarrierSetAssembler::arraycopy_prologue {");

  __ push(saved_regs);

  if (count == A0) {
    if (src == A1) {
      // exactly backwards!!
      __ move(AT, A0);
      __ move(A0, A1);
      __ move(A1, AT);
    } else {
      __ move(A1, count);
      __ move(A0, src);
    }
  } else {
    __ move(A0, src);
    __ move(A1, count);
  }

  __ call_VM_leaf(ZBarrierSetRuntime::load_barrier_on_oop_array_addr(), 2);

  __ pop(saved_regs);

  BLOCK_COMMENT("} ZBarrierSetAssembler::arraycopy_prologue");
}

void ZBarrierSetAssembler::try_resolve_jobject_in_native(MacroAssembler* masm,
                                                         Register jni_env,
                                                         Register robj,
                                                         Register tmp,
                                                         Label& slowpath) {
  BLOCK_COMMENT("ZBarrierSetAssembler::try_resolve_jobject_in_native {");

  assert_different_registers(jni_env, robj, tmp);

  // Resolve jobject
  BarrierSetAssembler::try_resolve_jobject_in_native(masm, jni_env, robj, tmp, slowpath);

  // The Address offset is too large to direct load - -784. Our range is +127, -128.
  __ li(tmp, (int64_t)(in_bytes(ZThreadLocalData::address_bad_mask_offset()) -
              in_bytes(JavaThread::jni_environment_offset())));

  // Load address bad mask
  __ ldx_d(tmp, jni_env, tmp);

  // Check address bad mask
  __ andr(AT, robj, tmp);
  __ bnez(AT, slowpath);

  BLOCK_COMMENT("} ZBarrierSetAssembler::try_resolve_jobject_in_native");
}

#ifdef COMPILER1

#undef __
#define __ ce->masm()->

void ZBarrierSetAssembler::generate_c1_load_barrier_test(LIR_Assembler* ce,
                                                         LIR_Opr ref) const {
  assert_different_registers(SCR1, TREG, ref->as_register());
  __ ld_d(SCR1, address_bad_mask_from_thread(TREG));
  __ andr(SCR1, SCR1, ref->as_register());
}

void ZBarrierSetAssembler::generate_c1_load_barrier_stub(LIR_Assembler* ce,
                                                         ZLoadBarrierStubC1* stub) const {
  // Stub entry
  __ bind(*stub->entry());

  Register ref = stub->ref()->as_register();
  Register ref_addr = noreg;
  Register tmp = noreg;

  if (stub->tmp()->is_valid()) {
    // Load address into tmp register
    ce->leal(stub->ref_addr(), stub->tmp());
    ref_addr = tmp = stub->tmp()->as_pointer_register();
  } else {
    // Address already in register
    ref_addr = stub->ref_addr()->as_address_ptr()->base()->as_pointer_register();
  }

  assert_different_registers(ref, ref_addr, noreg);

  // Save V0 unless it is the result or tmp register
  // Set up SP to accomodate parameters and maybe V0.
  if (ref != V0 && tmp != V0) {
    __ addi_d(SP, SP, -32);
    __ st_d(V0, SP, 16);
  } else {
    __ addi_d(SP, SP, -16);
  }

  // Setup arguments and call runtime stub
  ce->store_parameter(ref_addr, 1);
  ce->store_parameter(ref, 0);

  __ call(stub->runtime_stub(), relocInfo::runtime_call_type);

  // Verify result
  __ verify_oop(V0, "Bad oop");

  // Move result into place
  if (ref != V0) {
    __ move(ref, V0);
  }

  // Restore V0 unless it is the result or tmp register
  if (ref != V0 && tmp != V0) {
    __ ld_d(V0, SP, 16);
    __ addi_d(SP, SP, 32);
  } else {
    __ addi_d(SP, SP, 16);
  }

  // Stub exit
  __ b(*stub->continuation());
}

#undef __
#define __ sasm->

void ZBarrierSetAssembler::generate_c1_load_barrier_runtime_stub(StubAssembler* sasm,
                                                                 DecoratorSet decorators) const {
  __ prologue("zgc_load_barrier stub", false);

  __ push_call_clobbered_registers_except(RegSet::of(V0));

  // Setup arguments
  __ load_parameter(0, A0);
  __ load_parameter(1, A1);

  __ call_VM_leaf(ZBarrierSetRuntime::load_barrier_on_oop_field_preloaded_addr(decorators), 2);

  __ pop_call_clobbered_registers_except(RegSet::of(V0));

  __ epilogue();
}
#endif // COMPILER1

#ifdef COMPILER2

OptoReg::Name ZBarrierSetAssembler::refine_register(const Node* node, OptoReg::Name opto_reg) {
  if (!OptoReg::is_reg(opto_reg)) {
    return OptoReg::Bad;
  }

  const VMReg vm_reg = OptoReg::as_VMReg(opto_reg);
  if (vm_reg->is_FloatRegister()) {
    return opto_reg & ~1;
  }

  return opto_reg;
}

#undef __
#define __ _masm->

class ZSaveLiveRegisters {
private:
  MacroAssembler* const _masm;
  RegSet                _gp_regs;
  FloatRegSet           _fp_regs;
  FloatRegSet           _lsx_vp_regs;
  FloatRegSet           _lasx_vp_regs;

public:
  void initialize(ZLoadBarrierStubC2* stub) {
    // Record registers that needs to be saved/restored
    RegMaskIterator rmi(stub->live());
    while (rmi.has_next()) {
      const OptoReg::Name opto_reg = rmi.next();
      if (OptoReg::is_reg(opto_reg)) {
        const VMReg vm_reg = OptoReg::as_VMReg(opto_reg);
        if (vm_reg->is_Register()) {
          _gp_regs += RegSet::of(vm_reg->as_Register());
        } else if (vm_reg->is_FloatRegister()) {
          if (UseLASX && vm_reg->next(7))
            _lasx_vp_regs += FloatRegSet::of(vm_reg->as_FloatRegister());
          else if (UseLSX && vm_reg->next(3))
            _lsx_vp_regs += FloatRegSet::of(vm_reg->as_FloatRegister());
          else
            _fp_regs += FloatRegSet::of(vm_reg->as_FloatRegister());
        } else {
          fatal("Unknown register type");
        }
      }
    }

    // Remove C-ABI SOE registers, scratch regs and _ref register that will be updated
    _gp_regs -= RegSet::range(S0, S7) + RegSet::of(SP, SCR1, SCR2, stub->ref());
  }

  ZSaveLiveRegisters(MacroAssembler* masm, ZLoadBarrierStubC2* stub) :
      _masm(masm),
      _gp_regs(),
      _fp_regs(),
      _lsx_vp_regs(),
      _lasx_vp_regs() {

    // Figure out what registers to save/restore
    initialize(stub);

    // Save registers
    __ push(_gp_regs);
    __ push_fpu(_fp_regs);
    __ push_vp(_lsx_vp_regs  /* UseLSX  */);
    __ push_vp(_lasx_vp_regs /* UseLASX */);
  }

  ~ZSaveLiveRegisters() {
    // Restore registers
    __ pop_vp(_lasx_vp_regs /* UseLASX */);
    __ pop_vp(_lsx_vp_regs  /* UseLSX  */);
    __ pop_fpu(_fp_regs);
    __ pop(_gp_regs);
  }
};

#undef __
#define __ _masm->

class ZSetupArguments {
private:
  MacroAssembler* const _masm;
  const Register        _ref;
  const Address         _ref_addr;

public:
  ZSetupArguments(MacroAssembler* masm, ZLoadBarrierStubC2* stub) :
      _masm(masm),
      _ref(stub->ref()),
      _ref_addr(stub->ref_addr()) {

    // Setup arguments
    if (_ref_addr.base() == noreg) {
      // No self healing
      if (_ref != A0) {
        __ move(A0, _ref);
      }
      __ move(A1, 0);
    } else {
      // Self healing
      if (_ref == A0) {
        // _ref is already at correct place
        __ lea(A1, _ref_addr);
      } else if (_ref != A1) {
        // _ref is in wrong place, but not in A1, so fix it first
        __ lea(A1, _ref_addr);
        __ move(A0, _ref);
      } else if (_ref_addr.base() != A0 && _ref_addr.index() != A0) {
        assert(_ref == A1, "Mov ref first, vacating A0");
        __ move(A0, _ref);
        __ lea(A1, _ref_addr);
      } else {
        assert(_ref == A1, "Need to vacate A1 and _ref_addr is using A0");
        if (_ref_addr.base() == A0 || _ref_addr.index() == A0) {
          __ move(T4, A1);
          __ lea(A1, _ref_addr);
          __ move(A0, T4);
        } else {
          ShouldNotReachHere();
        }
      }
    }
  }

  ~ZSetupArguments() {
    // Transfer result
    if (_ref != V0) {
      __ move(_ref, V0);
    }
  }
};

#undef __
#define __ masm->

void ZBarrierSetAssembler::generate_c2_load_barrier_stub(MacroAssembler* masm, ZLoadBarrierStubC2* stub) const {
  BLOCK_COMMENT("ZLoadBarrierStubC2");

  // Stub entry
  __ bind(*stub->entry());

  {
    ZSaveLiveRegisters save_live_registers(masm, stub);
    ZSetupArguments setup_arguments(masm, stub);
    __ call_VM_leaf(stub->slow_path(), 2);
  }
  // Stub exit
  __ b(*stub->continuation());
}

#undef __

#endif // COMPILER2
