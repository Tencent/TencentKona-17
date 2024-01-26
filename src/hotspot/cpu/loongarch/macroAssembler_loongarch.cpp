/*
 * Copyright (c) 1997, 2014, Oracle and/or its affiliates. All rights reserved.
 * Copyright (c) 2017, 2022, Loongson Technology. All rights reserved.
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
#include "jvm.h"
#include "asm/assembler.hpp"
#include "asm/assembler.inline.hpp"
#include "asm/macroAssembler.inline.hpp"
#include "compiler/disassembler.hpp"
#include "gc/shared/barrierSet.hpp"
#include "gc/shared/barrierSetAssembler.hpp"
#include "gc/shared/collectedHeap.inline.hpp"
#include "interpreter/bytecodeHistogram.hpp"
#include "interpreter/interpreter.hpp"
#include "memory/resourceArea.hpp"
#include "memory/universe.hpp"
#include "nativeInst_loongarch.hpp"
#include "oops/compressedOops.inline.hpp"
#include "oops/klass.inline.hpp"
#include "prims/methodHandles.hpp"
#include "runtime/biasedLocking.hpp"
#include "runtime/interfaceSupport.inline.hpp"
#include "runtime/jniHandles.inline.hpp"
#include "runtime/objectMonitor.hpp"
#include "runtime/os.hpp"
#include "runtime/safepoint.hpp"
#include "runtime/safepointMechanism.hpp"
#include "runtime/sharedRuntime.hpp"
#include "runtime/stubRoutines.hpp"
#include "utilities/macros.hpp"

#ifdef COMPILER2
#include "opto/compile.hpp"
#include "opto/output.hpp"
#endif

#if INCLUDE_ZGC
#include "gc/z/zThreadLocalData.hpp"
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

// Implementation of MacroAssembler

intptr_t MacroAssembler::i[32] = {0};
float MacroAssembler::f[32] = {0.0};

void MacroAssembler::print(outputStream *s) {
  unsigned int k;
  for(k=0; k<sizeof(i)/sizeof(i[0]); k++) {
    s->print_cr("i%d = 0x%.16lx", k, i[k]);
  }
  s->cr();

  for(k=0; k<sizeof(f)/sizeof(f[0]); k++) {
    s->print_cr("f%d = %f", k, f[k]);
  }
  s->cr();
}

int MacroAssembler::i_offset(unsigned int k) { return (intptr_t)&((MacroAssembler*)0)->i[k]; }
int MacroAssembler::f_offset(unsigned int k) { return (intptr_t)&((MacroAssembler*)0)->f[k]; }

void MacroAssembler::save_registers(MacroAssembler *masm) {
#define __ masm->
  for(int k=0; k<32; k++) {
    __ st_w (as_Register(k), A0, i_offset(k));
  }

  for(int k=0; k<32; k++) {
    __ fst_s (as_FloatRegister(k), A0, f_offset(k));
  }
#undef __
}

void MacroAssembler::restore_registers(MacroAssembler *masm) {
#define __ masm->
  for(int k=0; k<32; k++) {
    __ ld_w (as_Register(k), A0, i_offset(k));
  }

  for(int k=0; k<32; k++) {
    __ fld_s (as_FloatRegister(k), A0, f_offset(k));
  }
#undef __
}


void MacroAssembler::pd_patch_instruction(address branch, address target, const char* file, int line) {
  jint& stub_inst = *(jint*)branch;
  jint *pc = (jint *)branch;

  if (high(stub_inst, 7) == pcaddu18i_op) {
    // far:
    //   pcaddu18i reg, si20
    //   jirl  r0, reg, si18

    assert(high(pc[1], 6) == jirl_op, "Not a branch label patch");
    jlong offs = target - branch;
    CodeBuffer cb(branch, 2 * BytesPerInstWord);
    MacroAssembler masm(&cb);
    if (reachable_from_branch_short(offs)) {
      // convert far to short
#define __ masm.
      __ b(target);
      __ nop();
#undef __
    } else {
      masm.patchable_jump_far(R0, offs);
    }
    return;
  } else if (high(stub_inst, 7) == pcaddi_op) {
    // see MacroAssembler::set_last_Java_frame:
    //   pcaddi reg, si20

    jint offs = (target - branch) >> 2;
    guarantee(is_simm(offs, 20), "Not signed 20-bit offset");
    CodeBuffer cb(branch, 1 * BytesPerInstWord);
    MacroAssembler masm(&cb);
    masm.pcaddi(as_Register(low(stub_inst, 5)), offs);
    return;
  } else if (high(stub_inst, 7) == pcaddu12i_op) {
    // pc-relative
    jlong offs = target - branch;
    guarantee(is_simm(offs, 32), "Not signed 32-bit offset");
    jint si12, si20;
    jint& stub_instNext = *(jint*)(branch+4);
    split_simm32(offs, si12, si20);
    CodeBuffer cb(branch, 2 * BytesPerInstWord);
    MacroAssembler masm(&cb);
    masm.pcaddu12i(as_Register(low(stub_inst, 5)), si20);
    masm.addi_d(as_Register(low((stub_instNext), 5)), as_Register(low((stub_instNext) >> 5, 5)), si12);
    return;
  } else if (high(stub_inst, 7) == lu12i_w_op) {
    // long call (absolute)
    CodeBuffer cb(branch, 3 * BytesPerInstWord);
    MacroAssembler masm(&cb);
    masm.call_long(target);
    return;
  }

  stub_inst = patched_branch(target - branch, stub_inst, 0);
}

bool MacroAssembler::reachable_from_branch_short(jlong offs) {
  if (ForceUnreachable) {
    return false;
  }
  return is_simm(offs >> 2, 26);
}

void MacroAssembler::patchable_jump_far(Register ra, jlong offs) {
  jint si18, si20;
  guarantee(is_simm(offs, 38), "Not signed 38-bit offset");
  split_simm38(offs, si18, si20);
  pcaddu18i(T4, si20);
  jirl(ra, T4, si18);
}

void MacroAssembler::patchable_jump(address target, bool force_patchable) {
  assert(ReservedCodeCacheSize < 4*G, "branch out of range");
  assert(CodeCache::find_blob(target) != NULL,
         "destination of jump not found in code cache");
  if (force_patchable || patchable_branches()) {
    jlong offs = target - pc();
    if (reachable_from_branch_short(offs)) { // Short jump
      b(offset26(target));
      nop();
    } else {                                 // Far jump
      patchable_jump_far(R0, offs);
    }
  } else {                                   // Real short jump
    b(offset26(target));
  }
}

void MacroAssembler::patchable_call(address target, address call_site) {
  jlong offs = target - (call_site ? call_site : pc());
  if (reachable_from_branch_short(offs - BytesPerInstWord)) { // Short call
    nop();
    bl((offs - BytesPerInstWord) >> 2);
  } else {                                                    // Far call
    patchable_jump_far(RA, offs);
  }
}

// Maybe emit a call via a trampoline.  If the code cache is small
// trampolines won't be emitted.

address MacroAssembler::trampoline_call(AddressLiteral entry, CodeBuffer *cbuf) {
  assert(JavaThread::current()->is_Compiler_thread(), "just checking");
  assert(entry.rspec().type() == relocInfo::runtime_call_type
         || entry.rspec().type() == relocInfo::opt_virtual_call_type
         || entry.rspec().type() == relocInfo::static_call_type
         || entry.rspec().type() == relocInfo::virtual_call_type, "wrong reloc type");

  // We need a trampoline if branches are far.
  if (far_branches()) {
    bool in_scratch_emit_size = false;
#ifdef COMPILER2
    // We don't want to emit a trampoline if C2 is generating dummy
    // code during its branch shortening phase.
    CompileTask* task = ciEnv::current()->task();
    in_scratch_emit_size =
      (task != NULL && is_c2_compile(task->comp_level()) &&
       Compile::current()->output()->in_scratch_emit_size());
#endif
    if (!in_scratch_emit_size) {
      address stub = emit_trampoline_stub(offset(), entry.target());
      if (stub == NULL) {
        postcond(pc() == badAddress);
        return NULL; // CodeCache is full
      }
    }
  }

  if (cbuf) cbuf->set_insts_mark();
  relocate(entry.rspec());
  if (!far_branches()) {
    bl(entry.target());
  } else {
    bl(pc());
  }
  // just need to return a non-null address
  postcond(pc() != badAddress);
  return pc();
}

// Emit a trampoline stub for a call to a target which is too far away.
//
// code sequences:
//
// call-site:
//   branch-and-link to <destination> or <trampoline stub>
//
// Related trampoline stub for this call site in the stub section:
//   load the call target from the constant pool
//   branch (RA still points to the call site above)

address MacroAssembler::emit_trampoline_stub(int insts_call_instruction_offset,
                                             address dest) {
  // Start the stub
  address stub = start_a_stub(NativeInstruction::nop_instruction_size
                   + NativeCallTrampolineStub::instruction_size);
  if (stub == NULL) {
    return NULL;  // CodeBuffer::expand failed
  }

  // Create a trampoline stub relocation which relates this trampoline stub
  // with the call instruction at insts_call_instruction_offset in the
  // instructions code-section.
  align(wordSize);
  relocate(trampoline_stub_Relocation::spec(code()->insts()->start()
                                            + insts_call_instruction_offset));
  const int stub_start_offset = offset();

  // Now, create the trampoline stub's code:
  // - load the call
  // - call
  pcaddi(T4, 0);
  ld_d(T4, T4, 16);
  jr(T4);
  nop();  //align
  assert(offset() - stub_start_offset == NativeCallTrampolineStub::data_offset,
         "should be");
  emit_int64((int64_t)dest);

  const address stub_start_addr = addr_at(stub_start_offset);

  NativeInstruction* ni = nativeInstruction_at(stub_start_addr);
  assert(ni->is_NativeCallTrampolineStub_at(), "doesn't look like a trampoline");

  end_a_stub();
  return stub_start_addr;
}

void MacroAssembler::beq_far(Register rs, Register rt, address entry) {
  if (is_simm16((entry - pc()) >> 2)) { // Short jump
    beq(rs, rt, offset16(entry));
  } else {                              // Far jump
    Label not_jump;
    bne(rs, rt, not_jump);
    b_far(entry);
    bind(not_jump);
  }
}

void MacroAssembler::beq_far(Register rs, Register rt, Label& L) {
  if (L.is_bound()) {
    beq_far(rs, rt, target(L));
  } else {
    Label not_jump;
    bne(rs, rt, not_jump);
    b_far(L);
    bind(not_jump);
  }
}

void MacroAssembler::bne_far(Register rs, Register rt, address entry) {
  if (is_simm16((entry - pc()) >> 2)) { // Short jump
    bne(rs, rt, offset16(entry));
  } else {                              // Far jump
    Label not_jump;
    beq(rs, rt, not_jump);
    b_far(entry);
    bind(not_jump);
  }
}

void MacroAssembler::bne_far(Register rs, Register rt, Label& L) {
  if (L.is_bound()) {
    bne_far(rs, rt, target(L));
  } else {
    Label not_jump;
    beq(rs, rt, not_jump);
    b_far(L);
    bind(not_jump);
  }
}

void MacroAssembler::blt_far(Register rs, Register rt, address entry, bool is_signed) {
  if (is_simm16((entry - pc()) >> 2)) { // Short jump
    if (is_signed) {
      blt(rs, rt, offset16(entry));
    } else {
      bltu(rs, rt, offset16(entry));
    }
  } else {                              // Far jump
    Label not_jump;
    if (is_signed) {
      bge(rs, rt, not_jump);
    } else {
      bgeu(rs, rt, not_jump);
    }
    b_far(entry);
    bind(not_jump);
  }
}

void MacroAssembler::blt_far(Register rs, Register rt, Label& L, bool is_signed) {
  if (L.is_bound()) {
    blt_far(rs, rt, target(L), is_signed);
  } else {
    Label not_jump;
    if (is_signed) {
      bge(rs, rt, not_jump);
    } else {
      bgeu(rs, rt, not_jump);
    }
    b_far(L);
    bind(not_jump);
  }
}

void MacroAssembler::bge_far(Register rs, Register rt, address entry, bool is_signed) {
  if (is_simm16((entry - pc()) >> 2)) { // Short jump
    if (is_signed) {
      bge(rs, rt, offset16(entry));
    } else {
      bgeu(rs, rt, offset16(entry));
    }
  } else {                              // Far jump
    Label not_jump;
    if (is_signed) {
      blt(rs, rt, not_jump);
    } else {
      bltu(rs, rt, not_jump);
    }
    b_far(entry);
    bind(not_jump);
  }
}

void MacroAssembler::bge_far(Register rs, Register rt, Label& L, bool is_signed) {
  if (L.is_bound()) {
    bge_far(rs, rt, target(L), is_signed);
  } else {
    Label not_jump;
    if (is_signed) {
      blt(rs, rt, not_jump);
    } else {
      bltu(rs, rt, not_jump);
    }
    b_far(L);
    bind(not_jump);
  }
}

void MacroAssembler::b_far(Label& L) {
  if (L.is_bound()) {
    b_far(target(L));
  } else {
    L.add_patch_at(code(), locator());
    if (ForceUnreachable) {
      patchable_jump_far(R0, 0);
    } else {
      b(0);
    }
  }
}

void MacroAssembler::b_far(address entry) {
  jlong offs = entry - pc();
  if (reachable_from_branch_short(offs)) { // Short jump
    b(offset26(entry));
  } else {                                 // Far jump
    patchable_jump_far(R0, offs);
  }
}

void MacroAssembler::ld_ptr(Register rt, Register base, Register offset) {
  ldx_d(rt, base, offset);
}

void MacroAssembler::st_ptr(Register rt, Register base, Register offset) {
  stx_d(rt, base, offset);
}

Address MacroAssembler::as_Address(AddressLiteral adr) {
  return Address(adr.target(), adr.rspec());
}

Address MacroAssembler::as_Address(ArrayAddress adr) {
  return Address::make_array(adr);
}

// tmp_reg1 and tmp_reg2 should be saved outside of atomic_inc32 (caller saved).
void MacroAssembler::atomic_inc32(address counter_addr, int inc, Register tmp_reg1, Register tmp_reg2) {
  li(tmp_reg1, inc);
  li(tmp_reg2, counter_addr);
  amadd_w(R0, tmp_reg1, tmp_reg2);
}

// Writes to stack successive pages until offset reached to check for
// stack overflow + shadow pages.  This clobbers tmp.
void MacroAssembler::bang_stack_size(Register size, Register tmp) {
  assert_different_registers(tmp, size, AT);
  move(tmp, SP);
  // Bang stack for total size given plus shadow page size.
  // Bang one page at a time because large size can bang beyond yellow and
  // red zones.
  Label loop;
  li(AT, os::vm_page_size());
  bind(loop);
  sub_d(tmp, tmp, AT);
  sub_d(size, size, AT);
  st_d(size, tmp, 0);
  blt(R0, size, loop);

  // Bang down shadow pages too.
  // At this point, (tmp-0) is the last address touched, so don't
  // touch it again.  (It was touched as (tmp-pagesize) but then tmp
  // was post-decremented.)  Skip this address by starting at i=1, and
  // touch a few more pages below.  N.B.  It is important to touch all
  // the way down to and including i=StackShadowPages.
  for (int i = 0; i < (int)(StackOverflow::stack_shadow_zone_size() / os::vm_page_size()) - 1; i++) {
    // this could be any sized move but this is can be a debugging crumb
    // so the bigger the better.
    sub_d(tmp, tmp, AT);
    st_d(size, tmp, 0);
  }
}

void MacroAssembler::reserved_stack_check() {
  Register thread = TREG;
#ifndef OPT_THREAD
  get_thread(thread);
#endif
  // testing if reserved zone needs to be enabled
  Label no_reserved_zone_enabling;

  ld_d(AT, Address(thread, JavaThread::reserved_stack_activation_offset()));
  sub_d(AT, SP, AT);
  blt(AT, R0,  no_reserved_zone_enabling);

  enter();   // RA and FP are live.
  call_VM_leaf(CAST_FROM_FN_PTR(address, SharedRuntime::enable_stack_reserved_zone), thread);
  leave();

  // We have already removed our own frame.
  // throw_delayed_StackOverflowError will think that it's been
  // called by our caller.
  li(AT, (long)StubRoutines::throw_delayed_StackOverflowError_entry());
  jr(AT);
  should_not_reach_here();

  bind(no_reserved_zone_enabling);
}

void MacroAssembler::biased_locking_enter(Register lock_reg,
                                         Register obj_reg,
                                         Register swap_reg,
                                         Register tmp_reg,
                                         bool swap_reg_contains_mark,
                                         Label& done,
                                         Label* slow_case,
                                         BiasedLockingCounters* counters) {
  assert(UseBiasedLocking, "why call this otherwise?");
  bool need_tmp_reg = false;
  if (tmp_reg == noreg) {
    need_tmp_reg = true;
    tmp_reg = T4;
  }
  assert_different_registers(lock_reg, obj_reg, swap_reg, tmp_reg, AT);
  assert(markWord::age_shift == markWord::lock_bits + markWord::biased_lock_bits, "biased locking makes assumptions about bit layout");
  Address mark_addr      (obj_reg, oopDesc::mark_offset_in_bytes());
  Address saved_mark_addr(lock_reg, 0);

  // Biased locking
  // See whether the lock is currently biased toward our thread and
  // whether the epoch is still valid
  // Note that the runtime guarantees sufficient alignment of JavaThread
  // pointers to allow age to be placed into low bits
  // First check to see whether biasing is even enabled for this object
  Label cas_label;
  if (!swap_reg_contains_mark) {
    ld_ptr(swap_reg, mark_addr);
  }

  if (need_tmp_reg) {
    push(tmp_reg);
  }
  move(tmp_reg, swap_reg);
  andi(tmp_reg, tmp_reg, markWord::biased_lock_mask_in_place);
  addi_d(AT, R0, markWord::biased_lock_pattern);
  sub_d(AT, AT, tmp_reg);
  if (need_tmp_reg) {
    pop(tmp_reg);
  }

  bne(AT, R0, cas_label);


  // The bias pattern is present in the object's header. Need to check
  // whether the bias owner and the epoch are both still current.
  // Note that because there is no current thread register on LA we
  // need to store off the mark word we read out of the object to
  // avoid reloading it and needing to recheck invariants below. This
  // store is unfortunate but it makes the overall code shorter and
  // simpler.
  st_ptr(swap_reg, saved_mark_addr);
  if (need_tmp_reg) {
    push(tmp_reg);
  }
  load_prototype_header(tmp_reg, obj_reg);
  xorr(tmp_reg, tmp_reg, swap_reg);
#ifndef OPT_THREAD
  get_thread(swap_reg);
  xorr(swap_reg, swap_reg, tmp_reg);
#else
  xorr(swap_reg, TREG, tmp_reg);
#endif

  li(AT, ~((int) markWord::age_mask_in_place));
  andr(swap_reg, swap_reg, AT);

  if (PrintBiasedLockingStatistics) {
    Label L;
    bne(swap_reg, R0, L);
    push(tmp_reg);
    push(A0);
    atomic_inc32((address)BiasedLocking::biased_lock_entry_count_addr(), 1, A0, tmp_reg);
    pop(A0);
    pop(tmp_reg);
    bind(L);
  }
  if (need_tmp_reg) {
    pop(tmp_reg);
  }
  beq(swap_reg, R0, done);
  Label try_revoke_bias;
  Label try_rebias;

  // At this point we know that the header has the bias pattern and
  // that we are not the bias owner in the current epoch. We need to
  // figure out more details about the state of the header in order to
  // know what operations can be legally performed on the object's
  // header.

  // If the low three bits in the xor result aren't clear, that means
  // the prototype header is no longer biased and we have to revoke
  // the bias on this object.

  li(AT, markWord::biased_lock_mask_in_place);
  andr(AT, swap_reg, AT);
  bne(AT, R0, try_revoke_bias);
  // Biasing is still enabled for this data type. See whether the
  // epoch of the current bias is still valid, meaning that the epoch
  // bits of the mark word are equal to the epoch bits of the
  // prototype header. (Note that the prototype header's epoch bits
  // only change at a safepoint.) If not, attempt to rebias the object
  // toward the current thread. Note that we must be absolutely sure
  // that the current epoch is invalid in order to do this because
  // otherwise the manipulations it performs on the mark word are
  // illegal.

  li(AT, markWord::epoch_mask_in_place);
  andr(AT,swap_reg, AT);
  bne(AT, R0, try_rebias);
  // The epoch of the current bias is still valid but we know nothing
  // about the owner; it might be set or it might be clear. Try to
  // acquire the bias of the object using an atomic operation. If this
  // fails we will go in to the runtime to revoke the object's bias.
  // Note that we first construct the presumed unbiased header so we
  // don't accidentally blow away another thread's valid bias.

  ld_ptr(swap_reg, saved_mark_addr);

  li(AT, markWord::biased_lock_mask_in_place | markWord::age_mask_in_place | markWord::epoch_mask_in_place);
  andr(swap_reg, swap_reg, AT);

  if (need_tmp_reg) {
    push(tmp_reg);
  }
#ifndef OPT_THREAD
  get_thread(tmp_reg);
  orr(tmp_reg, tmp_reg, swap_reg);
#else
  orr(tmp_reg, TREG, swap_reg);
#endif
  cmpxchg(Address(obj_reg, 0), swap_reg, tmp_reg, AT, false, true /* acquire */);
  if (need_tmp_reg) {
    pop(tmp_reg);
  }
  // If the biasing toward our thread failed, this means that
  // another thread succeeded in biasing it toward itself and we
  // need to revoke that bias. The revocation will occur in the
  // interpreter runtime in the slow case.
  if (PrintBiasedLockingStatistics) {
    Label L;
    bne(AT, R0, L);
    push(tmp_reg);
    push(A0);
    atomic_inc32((address)BiasedLocking::anonymously_biased_lock_entry_count_addr(), 1, A0, tmp_reg);
    pop(A0);
    pop(tmp_reg);
    bind(L);
  }
  if (slow_case != NULL) {
    beq_far(AT, R0, *slow_case);
  }
  b(done);

  bind(try_rebias);
  // At this point we know the epoch has expired, meaning that the
  // current "bias owner", if any, is actually invalid. Under these
  // circumstances _only_, we are allowed to use the current header's
  // value as the comparison value when doing the cas to acquire the
  // bias in the current epoch. In other words, we allow transfer of
  // the bias from one thread to another directly in this situation.
  //
  // FIXME: due to a lack of registers we currently blow away the age
  // bits in this situation. Should attempt to preserve them.
  if (need_tmp_reg) {
    push(tmp_reg);
  }
  load_prototype_header(tmp_reg, obj_reg);
#ifndef OPT_THREAD
  get_thread(swap_reg);
  orr(tmp_reg, tmp_reg, swap_reg);
#else
  orr(tmp_reg, tmp_reg, TREG);
#endif
  ld_ptr(swap_reg, saved_mark_addr);

  cmpxchg(Address(obj_reg, 0), swap_reg, tmp_reg, AT, false, true /* acquire */);
  if (need_tmp_reg) {
    pop(tmp_reg);
  }
  // If the biasing toward our thread failed, then another thread
  // succeeded in biasing it toward itself and we need to revoke that
  // bias. The revocation will occur in the runtime in the slow case.
  if (PrintBiasedLockingStatistics) {
    Label L;
    bne(AT, R0, L);
    push(AT);
    push(tmp_reg);
    atomic_inc32((address)BiasedLocking::rebiased_lock_entry_count_addr(), 1, AT, tmp_reg);
    pop(tmp_reg);
    pop(AT);
    bind(L);
  }
  if (slow_case != NULL) {
    beq_far(AT, R0, *slow_case);
  }

  b(done);
  bind(try_revoke_bias);
  // The prototype mark in the klass doesn't have the bias bit set any
  // more, indicating that objects of this data type are not supposed
  // to be biased any more. We are going to try to reset the mark of
  // this object to the prototype value and fall through to the
  // CAS-based locking scheme. Note that if our CAS fails, it means
  // that another thread raced us for the privilege of revoking the
  // bias of this particular object, so it's okay to continue in the
  // normal locking code.
  //
  // FIXME: due to a lack of registers we currently blow away the age
  // bits in this situation. Should attempt to preserve them.
  ld_ptr(swap_reg, saved_mark_addr);

  if (need_tmp_reg) {
    push(tmp_reg);
  }
  load_prototype_header(tmp_reg, obj_reg);
  cmpxchg(Address(obj_reg, 0), swap_reg, tmp_reg, AT, false, true /* acquire */);
  if (need_tmp_reg) {
    pop(tmp_reg);
  }
  // Fall through to the normal CAS-based lock, because no matter what
  // the result of the above CAS, some thread must have succeeded in
  // removing the bias bit from the object's header.
  if (PrintBiasedLockingStatistics) {
    Label L;
    bne(AT, R0, L);
    push(AT);
    push(tmp_reg);
    atomic_inc32((address)BiasedLocking::revoked_lock_entry_count_addr(), 1, AT, tmp_reg);
    pop(tmp_reg);
    pop(AT);
    bind(L);
  }

  bind(cas_label);
}

void MacroAssembler::biased_locking_exit(Register obj_reg, Register temp_reg, Label& done) {
  assert(UseBiasedLocking, "why call this otherwise?");

  // Check for biased locking unlock case, which is a no-op
  // Note: we do not have to check the thread ID for two reasons.
  // First, the interpreter checks for IllegalMonitorStateException at
  // a higher level. Second, if the bias was revoked while we held the
  // lock, the object could not be rebiased toward another thread, so
  // the bias bit would be clear.
  ld_d(temp_reg, Address(obj_reg, oopDesc::mark_offset_in_bytes()));
  andi(temp_reg, temp_reg, markWord::biased_lock_mask_in_place);
  addi_d(AT, R0, markWord::biased_lock_pattern);

  beq(AT, temp_reg, done);
}

// the stack pointer adjustment is needed. see InterpreterMacroAssembler::super_call_VM_leaf
// this method will handle the stack problem, you need not to preserve the stack space for the argument now
void MacroAssembler::call_VM_leaf_base(address entry_point, int number_of_arguments) {
  assert(number_of_arguments <= 4, "just check");
  assert(StackAlignmentInBytes == 16, "must be");
  move(AT, SP);
  bstrins_d(SP, R0, 3, 0);
  addi_d(SP, SP, -(StackAlignmentInBytes));
  st_d(AT, SP, 0);
  call(entry_point, relocInfo::runtime_call_type);
  ld_d(SP, SP, 0);
}


void MacroAssembler::jmp(address entry) {
  jlong offs = entry - pc();
  if (reachable_from_branch_short(offs)) { // Short jump
    b(offset26(entry));
  } else {                                 // Far jump
    patchable_jump_far(R0, offs);
  }
}

void MacroAssembler::jmp(address entry, relocInfo::relocType rtype) {
  switch (rtype) {
    case relocInfo::none:
      jmp(entry);
      break;
    default:
      {
        InstructionMark im(this);
        relocate(rtype);
        patchable_jump(entry);
      }
      break;
  }
}

void MacroAssembler::jmp_far(Label& L) {
  if (L.is_bound()) {
    assert(target(L) != NULL, "jmp most probably wrong");
    patchable_jump(target(L), true /* force patchable */);
  } else {
    L.add_patch_at(code(), locator());
    patchable_jump_far(R0, 0);
  }
}

// Move an oop into a register.  immediate is true if we want
// immediate instructions and nmethod entry barriers are not enabled.
// i.e. we are not going to patch this instruction while the code is being
// executed by another thread.
void MacroAssembler::movoop(Register dst, jobject obj, bool immediate) {
  int oop_index;
  if (obj == NULL) {
    oop_index = oop_recorder()->allocate_oop_index(obj);
  } else {
#ifdef ASSERT
    {
      ThreadInVMfromUnknown tiv;
      assert(Universe::heap()->is_in(JNIHandles::resolve(obj)), "should be real oop");
    }
#endif
    oop_index = oop_recorder()->find_index(obj);
  }
  RelocationHolder rspec = oop_Relocation::spec(oop_index);

  // nmethod entry barrier necessitate using the constant pool. They have to be
  // ordered with respected to oop accesses.
  // Using immediate literals would necessitate ISBs.
  if (BarrierSet::barrier_set()->barrier_set_nmethod() != NULL || !immediate) {
    address dummy = address(uintptr_t(pc()) & -wordSize); // A nearby aligned address
    relocate(rspec);
    patchable_li52(dst, (long)dummy);
  } else {
    relocate(rspec);
    patchable_li52(dst, (long)obj);
  }
}

void MacroAssembler::mov_metadata(Address dst, Metadata* obj) {
  int oop_index;
  if (obj) {
    oop_index = oop_recorder()->find_index(obj);
  } else {
    oop_index = oop_recorder()->allocate_metadata_index(obj);
  }
  relocate(metadata_Relocation::spec(oop_index));
  patchable_li52(AT, (long)obj);
  st_d(AT, dst);
}

void MacroAssembler::mov_metadata(Register dst, Metadata* obj) {
  int oop_index;
  if (obj) {
    oop_index = oop_recorder()->find_index(obj);
  } else {
    oop_index = oop_recorder()->allocate_metadata_index(obj);
  }
  relocate(metadata_Relocation::spec(oop_index));
  patchable_li52(dst, (long)obj);
}

void MacroAssembler::call(address entry) {
  jlong offs = entry - pc();
  if (reachable_from_branch_short(offs)) { // Short call (pc-rel)
    bl(offset26(entry));
  } else if (is_simm(offs, 38)) {          // Far call (pc-rel)
    patchable_jump_far(RA, offs);
  } else {                                 // Long call (absolute)
    call_long(entry);
  }
}

void MacroAssembler::call(address entry, relocInfo::relocType rtype) {
  switch (rtype) {
    case relocInfo::none:
      call(entry);
      break;
    case relocInfo::runtime_call_type:
      if (!is_simm(entry - pc(), 38)) {
        call_long(entry);
        break;
      }
      // fallthrough
    default:
      {
        InstructionMark im(this);
        relocate(rtype);
        patchable_call(entry);
      }
      break;
  }
}

void MacroAssembler::call(address entry, RelocationHolder& rh){
  switch (rh.type()) {
    case relocInfo::none:
      call(entry);
      break;
    case relocInfo::runtime_call_type:
      if (!is_simm(entry - pc(), 38)) {
        call_long(entry);
        break;
      }
      // fallthrough
    default:
      {
        InstructionMark im(this);
        relocate(rh);
        patchable_call(entry);
      }
      break;
  }
}

void MacroAssembler::call_long(address entry) {
  jlong value = (jlong)entry;
  lu12i_w(T4, split_low20(value >> 12));
  lu32i_d(T4, split_low20(value >> 32));
  jirl(RA, T4, split_low12(value));
}

address MacroAssembler::ic_call(address entry, jint method_index) {
  RelocationHolder rh = virtual_call_Relocation::spec(pc(), method_index);
  patchable_li52(IC_Klass, (long)Universe::non_oop_word());
  assert(entry != NULL, "call most probably wrong");
  InstructionMark im(this);
  return trampoline_call(AddressLiteral(entry, rh));
}

void MacroAssembler::c2bool(Register r) {
  sltu(r, R0, r);
}

#ifndef PRODUCT
extern "C" void findpc(intptr_t x);
#endif

void MacroAssembler::debug(char* msg/*, RegistersForDebugging* regs*/) {
  if ( ShowMessageBoxOnError ) {
    JavaThreadState saved_state = JavaThread::current()->thread_state();
    JavaThread::current()->set_thread_state(_thread_in_vm);
    {
      // In order to get locks work, we need to fake a in_VM state
      ttyLocker ttyl;
      ::tty->print_cr("EXECUTION STOPPED: %s\n", msg);
      if (CountBytecodes || TraceBytecodes || StopInterpreterAt) {
        BytecodeCounter::print();
      }

    }
    ThreadStateTransition::transition(JavaThread::current(), _thread_in_vm, saved_state);
  }
  else
    ::tty->print_cr("=============== DEBUG MESSAGE: %s ================\n", msg);
}


void MacroAssembler::stop(const char* msg) {
#ifndef PRODUCT
  block_comment(msg);
#endif
  csrrd(R0, 0);
  emit_int64((uintptr_t)msg);
}

void MacroAssembler::increment(Register reg, int imm) {
  if (!imm) return;
  if (is_simm(imm, 12)) {
    addi_d(reg, reg, imm);
  } else {
    li(AT, imm);
    add_d(reg, reg, AT);
  }
}

void MacroAssembler::decrement(Register reg, int imm) {
  increment(reg, -imm);
}

void MacroAssembler::increment(Address addr, int imm) {
  if (!imm) return;
  assert(is_simm(imm, 12), "must be");
  ld_ptr(AT, addr);
  addi_d(AT, AT, imm);
  st_ptr(AT, addr);
}

void MacroAssembler::decrement(Address addr, int imm) {
  increment(addr, -imm);
}

void MacroAssembler::call_VM(Register oop_result,
                             address entry_point,
                             bool check_exceptions) {
  call_VM_helper(oop_result, entry_point, 0, check_exceptions);
}

void MacroAssembler::call_VM(Register oop_result,
                             address entry_point,
                             Register arg_1,
                             bool check_exceptions) {
  if (arg_1!=A1) move(A1, arg_1);
  call_VM_helper(oop_result, entry_point, 1, check_exceptions);
}

void MacroAssembler::call_VM(Register oop_result,
                             address entry_point,
                             Register arg_1,
                             Register arg_2,
                             bool check_exceptions) {
  if (arg_1!=A1) move(A1, arg_1);
  if (arg_2!=A2) move(A2, arg_2);
  assert(arg_2 != A1, "smashed argument");
  call_VM_helper(oop_result, entry_point, 2, check_exceptions);
}

void MacroAssembler::call_VM(Register oop_result,
                             address entry_point,
                             Register arg_1,
                             Register arg_2,
                             Register arg_3,
                             bool check_exceptions) {
  if (arg_1!=A1) move(A1, arg_1);
  if (arg_2!=A2) move(A2, arg_2); assert(arg_2 != A1, "smashed argument");
  if (arg_3!=A3) move(A3, arg_3); assert(arg_3 != A1 && arg_3 != A2, "smashed argument");
  call_VM_helper(oop_result, entry_point, 3, check_exceptions);
}

void MacroAssembler::call_VM(Register oop_result,
                             Register last_java_sp,
                             address entry_point,
                             int number_of_arguments,
                             bool check_exceptions) {
  call_VM_base(oop_result, NOREG, last_java_sp, entry_point, number_of_arguments, check_exceptions);
}

void MacroAssembler::call_VM(Register oop_result,
                             Register last_java_sp,
                             address entry_point,
                             Register arg_1,
                             bool check_exceptions) {
  if (arg_1 != A1) move(A1, arg_1);
  call_VM(oop_result, last_java_sp, entry_point, 1, check_exceptions);
}

void MacroAssembler::call_VM(Register oop_result,
                             Register last_java_sp,
                             address entry_point,
                             Register arg_1,
                             Register arg_2,
                             bool check_exceptions) {
  if (arg_1 != A1) move(A1, arg_1);
  if (arg_2 != A2) move(A2, arg_2); assert(arg_2 != A1, "smashed argument");
  call_VM(oop_result, last_java_sp, entry_point, 2, check_exceptions);
}

void MacroAssembler::call_VM(Register oop_result,
                             Register last_java_sp,
                             address entry_point,
                             Register arg_1,
                             Register arg_2,
                             Register arg_3,
                             bool check_exceptions) {
  if (arg_1 != A1) move(A1, arg_1);
  if (arg_2 != A2) move(A2, arg_2); assert(arg_2 != A1, "smashed argument");
  if (arg_3 != A3) move(A3, arg_3); assert(arg_3 != A1 && arg_3 != A2, "smashed argument");
  call_VM(oop_result, last_java_sp, entry_point, 3, check_exceptions);
}

void MacroAssembler::call_VM_base(Register oop_result,
                                  Register java_thread,
                                  Register last_java_sp,
                                  address  entry_point,
                                  int      number_of_arguments,
                                  bool     check_exceptions) {
  // determine java_thread register
  if (!java_thread->is_valid()) {
#ifndef OPT_THREAD
    java_thread = T2;
    get_thread(java_thread);
#else
    java_thread = TREG;
#endif
  }
  // determine last_java_sp register
  if (!last_java_sp->is_valid()) {
    last_java_sp = SP;
  }
  // debugging support
  assert(number_of_arguments >= 0   , "cannot have negative number of arguments");
  assert(number_of_arguments <= 4   , "cannot have negative number of arguments");
  assert(java_thread != oop_result  , "cannot use the same register for java_thread & oop_result");
  assert(java_thread != last_java_sp, "cannot use the same register for java_thread & last_java_sp");

  assert(last_java_sp != FP, "this code doesn't work for last_java_sp == fp, which currently can't portably work anyway since C2 doesn't save fp");

  // set last Java frame before call
  Label before_call;
  bind(before_call);
  set_last_Java_frame(java_thread, last_java_sp, FP, before_call);

  // do the call
  move(A0, java_thread);
  call(entry_point, relocInfo::runtime_call_type);

  // restore the thread (cannot use the pushed argument since arguments
  // may be overwritten by C code generated by an optimizing compiler);
  // however can use the register value directly if it is callee saved.
#ifndef OPT_THREAD
  get_thread(java_thread);
#else
#ifdef ASSERT
  {
    Label L;
    get_thread(AT);
    beq(java_thread, AT, L);
    stop("MacroAssembler::call_VM_base: TREG not callee saved?");
    bind(L);
  }
#endif
#endif

  // discard thread and arguments
  ld_ptr(SP, java_thread, in_bytes(JavaThread::last_Java_sp_offset()));
  // reset last Java frame
  reset_last_Java_frame(java_thread, false);

  check_and_handle_popframe(java_thread);
  check_and_handle_earlyret(java_thread);
  if (check_exceptions) {
    // check for pending exceptions (java_thread is set upon return)
    Label L;
    ld_d(AT, java_thread, in_bytes(Thread::pending_exception_offset()));
    beq(AT, R0, L);
    lipc(AT, before_call);
    push(AT);
    jmp(StubRoutines::forward_exception_entry(), relocInfo::runtime_call_type);
    bind(L);
  }

  // get oop result if there is one and reset the value in the thread
  if (oop_result->is_valid()) {
    ld_d(oop_result, java_thread, in_bytes(JavaThread::vm_result_offset()));
    st_d(R0, java_thread, in_bytes(JavaThread::vm_result_offset()));
    verify_oop(oop_result);
  }
}

void MacroAssembler::call_VM_helper(Register oop_result, address entry_point, int number_of_arguments, bool check_exceptions) {
  move(V0, SP);
  //we also reserve space for java_thread here
  assert(StackAlignmentInBytes == 16, "must be");
  bstrins_d(SP, R0, 3, 0);
  call_VM_base(oop_result, NOREG, V0, entry_point, number_of_arguments, check_exceptions);
}

void MacroAssembler::call_VM_leaf(address entry_point, int number_of_arguments) {
  call_VM_leaf_base(entry_point, number_of_arguments);
}

void MacroAssembler::call_VM_leaf(address entry_point, Register arg_0) {
  if (arg_0 != A0) move(A0, arg_0);
  call_VM_leaf(entry_point, 1);
}

void MacroAssembler::call_VM_leaf(address entry_point, Register arg_0, Register arg_1) {
  if (arg_0 != A0) move(A0, arg_0);
  if (arg_1 != A1) move(A1, arg_1); assert(arg_1 != A0, "smashed argument");
  call_VM_leaf(entry_point, 2);
}

void MacroAssembler::call_VM_leaf(address entry_point, Register arg_0, Register arg_1, Register arg_2) {
  if (arg_0 != A0) move(A0, arg_0);
  if (arg_1 != A1) move(A1, arg_1); assert(arg_1 != A0, "smashed argument");
  if (arg_2 != A2) move(A2, arg_2); assert(arg_2 != A0 && arg_2 != A1, "smashed argument");
  call_VM_leaf(entry_point, 3);
}

void MacroAssembler::super_call_VM_leaf(address entry_point) {
  MacroAssembler::call_VM_leaf_base(entry_point, 0);
}

void MacroAssembler::super_call_VM_leaf(address entry_point,
                                                   Register arg_1) {
  if (arg_1 != A0) move(A0, arg_1);
  MacroAssembler::call_VM_leaf_base(entry_point, 1);
}

void MacroAssembler::super_call_VM_leaf(address entry_point,
                                                   Register arg_1,
                                                   Register arg_2) {
  if (arg_1 != A0) move(A0, arg_1);
  if (arg_2 != A1) move(A1, arg_2); assert(arg_2 != A0, "smashed argument");
  MacroAssembler::call_VM_leaf_base(entry_point, 2);
}

void MacroAssembler::super_call_VM_leaf(address entry_point,
                                                   Register arg_1,
                                                   Register arg_2,
                                                   Register arg_3) {
  if (arg_1 != A0) move(A0, arg_1);
  if (arg_2 != A1) move(A1, arg_2); assert(arg_2 != A0, "smashed argument");
  if (arg_3 != A2) move(A2, arg_3); assert(arg_3 != A0 && arg_3 != A1, "smashed argument");
  MacroAssembler::call_VM_leaf_base(entry_point, 3);
}

void MacroAssembler::check_and_handle_earlyret(Register java_thread) {
}

void MacroAssembler::check_and_handle_popframe(Register java_thread) {
}

void MacroAssembler::null_check(Register reg, int offset) {
  if (needs_explicit_null_check(offset)) {
    // provoke OS NULL exception if reg = NULL by
    // accessing M[reg] w/o changing any (non-CC) registers
    // NOTE: cmpl is plenty here to provoke a segv
    ld_w(AT, reg, 0);
  } else {
    // nothing to do, (later) access of M[reg + offset]
    // will provoke OS NULL exception if reg = NULL
  }
}

void MacroAssembler::enter() {
  push2(RA, FP);
  addi_d(FP, SP, 2 * wordSize);
}

void MacroAssembler::leave() {
  addi_d(SP, FP, -2 * wordSize);
  pop2(RA, FP);
}

void MacroAssembler::build_frame(int framesize) {
  assert(framesize >= 2 * wordSize, "framesize must include space for FP/RA");
  assert(framesize % (2 * wordSize) == 0, "must preserve 2 * wordSize alignment");
  if (Assembler::is_simm(-framesize, 12)) {
    addi_d(SP, SP, -framesize);
    st_ptr(FP, Address(SP, framesize - 2 * wordSize));
    st_ptr(RA, Address(SP, framesize - 1 * wordSize));
    if (PreserveFramePointer)
      addi_d(FP, SP, framesize);
  } else {
    addi_d(SP, SP, -2 * wordSize);
    st_ptr(FP, Address(SP, 0 * wordSize));
    st_ptr(RA, Address(SP, 1 * wordSize));
    if (PreserveFramePointer)
      addi_d(FP, SP, 2 * wordSize);
    li(SCR1, framesize - 2 * wordSize);
    sub_d(SP, SP, SCR1);
  }
  verify_cross_modify_fence_not_required();
}

void MacroAssembler::remove_frame(int framesize) {
  assert(framesize >= 2 * wordSize, "framesize must include space for FP/RA");
  assert(framesize % (2*wordSize) == 0, "must preserve 2*wordSize alignment");
  if (Assembler::is_simm(framesize, 12)) {
    ld_ptr(FP, Address(SP, framesize - 2 * wordSize));
    ld_ptr(RA, Address(SP, framesize - 1 * wordSize));
    addi_d(SP, SP, framesize);
  } else {
    li(SCR1, framesize - 2 * wordSize);
    add_d(SP, SP, SCR1);
    ld_ptr(FP, Address(SP, 0 * wordSize));
    ld_ptr(RA, Address(SP, 1 * wordSize));
    addi_d(SP, SP, 2 * wordSize);
  }
}

void MacroAssembler::unimplemented(const char* what) {
  const char* buf = NULL;
  {
    ResourceMark rm;
    stringStream ss;
    ss.print("unimplemented: %s", what);
    buf = code_string(ss.as_string());
  }
  stop(buf);
}

void MacroAssembler::get_thread(Register thread) {
#ifdef MINIMIZE_RAM_USAGE
  Register tmp;

  if (thread == AT)
    tmp = T4;
  else
    tmp = AT;

  move(thread, SP);
  shr(thread, PAGE_SHIFT);

  push(tmp);
  li(tmp, ((1UL << (SP_BITLENGTH - PAGE_SHIFT)) - 1));
  andr(thread, thread, tmp);
  shl(thread, Address::times_ptr); // sizeof(Thread *)
  li(tmp, (long)ThreadLocalStorage::sp_map_addr());
  add_d(tmp, tmp, thread);
  ld_ptr(thread, tmp, 0);
  pop(tmp);
#else
  if (thread != V0) {
    push(V0);
  }
  push_call_clobbered_registers_except(RegSet::of(V0));

  push(S5);
  move(S5, SP);
  assert(StackAlignmentInBytes == 16, "must be");
  bstrins_d(SP, R0, 3, 0);
  // TODO: confirm reloc
  call(CAST_FROM_FN_PTR(address, Thread::current), relocInfo::runtime_call_type);
  move(SP, S5);
  pop(S5);

  pop_call_clobbered_registers_except(RegSet::of(V0));
  if (thread != V0) {
    move(thread, V0);
    pop(V0);
  }
#endif // MINIMIZE_RAM_USAGE
}

void MacroAssembler::reset_last_Java_frame(Register java_thread, bool clear_fp) {
  // determine java_thread register
  if (!java_thread->is_valid()) {
#ifndef OPT_THREAD
    java_thread = T1;
    get_thread(java_thread);
#else
    java_thread = TREG;
#endif
  }
  // we must set sp to zero to clear frame
  st_ptr(R0, java_thread, in_bytes(JavaThread::last_Java_sp_offset()));
  // must clear fp, so that compiled frames are not confused; it is possible
  // that we need it only for debugging
  if(clear_fp) {
    st_ptr(R0, java_thread, in_bytes(JavaThread::last_Java_fp_offset()));
  }

  // Always clear the pc because it could have been set by make_walkable()
  st_ptr(R0, java_thread, in_bytes(JavaThread::last_Java_pc_offset()));
}

void MacroAssembler::reset_last_Java_frame(bool clear_fp) {
  Register thread = TREG;
#ifndef OPT_THREAD
  get_thread(thread);
#endif
  // we must set sp to zero to clear frame
  st_d(R0, thread, in_bytes(JavaThread::last_Java_sp_offset()));
  // must clear fp, so that compiled frames are not confused; it is
  // possible that we need it only for debugging
  if (clear_fp) {
    st_d(R0, thread, in_bytes(JavaThread::last_Java_fp_offset()));
  }

  // Always clear the pc because it could have been set by make_walkable()
  st_d(R0, thread, in_bytes(JavaThread::last_Java_pc_offset()));
}

void MacroAssembler::safepoint_poll(Label& slow_path, Register thread_reg, bool at_return, bool acquire, bool in_nmethod) {
  if (acquire) {
    ld_d(AT, thread_reg, in_bytes(JavaThread::polling_word_offset()));
    membar(Assembler::Membar_mask_bits(LoadLoad|LoadStore));
  } else {
    ld_d(AT, thread_reg, in_bytes(JavaThread::polling_word_offset()));
  }
  if (at_return) {
    // Note that when in_nmethod is set, the stack pointer is incremented before the poll. Therefore,
    // we may safely use the sp instead to perform the stack watermark check.
    blt_far(AT, in_nmethod ? SP : FP, slow_path, false /* signed */);
  } else {
    andi(AT, AT, SafepointMechanism::poll_bit());
    bnez(AT, slow_path);
  }
}

// Calls to C land
//
// When entering C land, the fp, & sp of the last Java frame have to be recorded
// in the (thread-local) JavaThread object. When leaving C land, the last Java fp
// has to be reset to 0. This is required to allow proper stack traversal.
void MacroAssembler::set_last_Java_frame(Register java_thread,
                                         Register last_java_sp,
                                         Register last_java_fp,
                                         Label& last_java_pc) {
  // determine java_thread register
  if (!java_thread->is_valid()) {
#ifndef OPT_THREAD
    java_thread = T2;
    get_thread(java_thread);
#else
    java_thread = TREG;
#endif
  }

  // determine last_java_sp register
  if (!last_java_sp->is_valid()) {
    last_java_sp = SP;
  }

  // last_java_fp is optional
  if (last_java_fp->is_valid()) {
    st_ptr(last_java_fp, java_thread, in_bytes(JavaThread::last_Java_fp_offset()));
  }

  // last_java_pc
  lipc(AT, last_java_pc);
  st_ptr(AT, java_thread, in_bytes(JavaThread::frame_anchor_offset() +
                                   JavaFrameAnchor::last_Java_pc_offset()));

  st_ptr(last_java_sp, java_thread, in_bytes(JavaThread::last_Java_sp_offset()));
}

void MacroAssembler::set_last_Java_frame(Register last_java_sp,
                                         Register last_java_fp,
                                         Label& last_java_pc) {
  set_last_Java_frame(NOREG, last_java_sp, last_java_fp, last_java_pc);
}

void MacroAssembler::set_last_Java_frame(Register last_java_sp,
                                         Register last_java_fp,
                                         Register last_java_pc) {
#ifndef OPT_THREAD
  Register java_thread = T2;
  get_thread(java_thread);
#else
  Register java_thread = TREG;
#endif

  // determine last_java_sp register
  if (!last_java_sp->is_valid()) {
    last_java_sp = SP;
  }

  // last_java_fp is optional
  if (last_java_fp->is_valid()) {
    st_ptr(last_java_fp, java_thread, in_bytes(JavaThread::last_Java_fp_offset()));
  }

  // last_java_pc is optional
  if (last_java_pc->is_valid()) {
    st_ptr(last_java_pc, java_thread, in_bytes(JavaThread::frame_anchor_offset() +
                                               JavaFrameAnchor::last_Java_pc_offset()));
  }

  st_ptr(last_java_sp, java_thread, in_bytes(JavaThread::last_Java_sp_offset()));
}

// Defines obj, preserves var_size_in_bytes, okay for t2 == var_size_in_bytes.
void MacroAssembler::tlab_allocate(Register obj,
                                   Register var_size_in_bytes,
                                   int con_size_in_bytes,
                                   Register t1,
                                   Register t2,
                                   Label& slow_case) {
  BarrierSetAssembler *bs = BarrierSet::barrier_set()->barrier_set_assembler();
  bs->tlab_allocate(this, obj, var_size_in_bytes, con_size_in_bytes, t1, t2, slow_case);
}

// Defines obj, preserves var_size_in_bytes
void MacroAssembler::eden_allocate(Register obj,
                                   Register var_size_in_bytes,
                                   int con_size_in_bytes,
                                   Register t1,
                                   Label& slow_case) {
  BarrierSetAssembler *bs = BarrierSet::barrier_set()->barrier_set_assembler();
  bs->eden_allocate(this, obj, var_size_in_bytes, con_size_in_bytes, t1, slow_case);
}


void MacroAssembler::incr_allocated_bytes(Register thread,
                                          Register var_size_in_bytes,
                                          int con_size_in_bytes,
                                          Register t1) {
  if (!thread->is_valid()) {
#ifndef OPT_THREAD
    assert(t1->is_valid(), "need temp reg");
    thread = t1;
    get_thread(thread);
#else
    thread = TREG;
#endif
  }

  ld_ptr(AT, thread, in_bytes(JavaThread::allocated_bytes_offset()));
  if (var_size_in_bytes->is_valid()) {
    add_d(AT, AT, var_size_in_bytes);
  } else {
    addi_d(AT, AT, con_size_in_bytes);
  }
  st_ptr(AT, thread, in_bytes(JavaThread::allocated_bytes_offset()));
}

void MacroAssembler::li(Register rd, jlong value) {
  jlong hi12 = bitfield(value, 52, 12);
  jlong lo52 = bitfield(value,  0, 52);

  if (hi12 != 0 && lo52 == 0) {
    lu52i_d(rd, R0, hi12);
  } else {
    jlong hi20 = bitfield(value, 32, 20);
    jlong lo20 = bitfield(value, 12, 20);
    jlong lo12 = bitfield(value,  0, 12);

    if (lo20 == 0) {
      ori(rd, R0, lo12);
    } else if (bitfield(simm12(lo12), 12, 20) == lo20) {
      addi_w(rd, R0, simm12(lo12));
    } else {
      lu12i_w(rd, lo20);
      if (lo12 != 0)
        ori(rd, rd, lo12);
    }
    if (hi20 != bitfield(simm20(lo20), 20, 20))
      lu32i_d(rd, hi20);
    if (hi12 != bitfield(simm20(hi20), 20, 12))
      lu52i_d(rd, rd, hi12);
  }
}

void MacroAssembler::patchable_li52(Register rd, jlong value) {
  int count = 0;

  if (value <= max_jint && value >= min_jint) {
    if (is_simm(value, 12)) {
      addi_d(rd, R0, value);
      count++;
    } else if (is_uimm(value, 12)) {
      ori(rd, R0, value);
      count++;
    } else {
      lu12i_w(rd, split_low20(value >> 12));
      count++;
      if (split_low12(value)) {
        ori(rd, rd, split_low12(value));
        count++;
      }
    }
  } else if (is_simm(value, 52)) {
    lu12i_w(rd, split_low20(value >> 12));
    count++;
    if (split_low12(value)) {
      ori(rd, rd, split_low12(value));
      count++;
    }
    lu32i_d(rd, split_low20(value >> 32));
    count++;
  } else {
    tty->print_cr("value = 0x%lx", value);
    guarantee(false, "Not supported yet !");
  }

  while (count < 3) {
    nop();
    count++;
  }
}

void MacroAssembler::lipc(Register rd, Label& L) {
  if (L.is_bound()) {
    jint offs = (target(L) - pc()) >> 2;
    guarantee(is_simm(offs, 20), "Not signed 20-bit offset");
    pcaddi(rd, offs);
  } else {
    InstructionMark im(this);
    L.add_patch_at(code(), locator());
    pcaddi(rd, 0);
  }
}

void MacroAssembler::set_narrow_klass(Register dst, Klass* k) {
  assert(UseCompressedClassPointers, "should only be used for compressed header");
  assert(oop_recorder() != NULL, "this assembler needs an OopRecorder");

  int klass_index = oop_recorder()->find_index(k);
  RelocationHolder rspec = metadata_Relocation::spec(klass_index);
  long narrowKlass = (long)CompressedKlassPointers::encode(k);

  relocate(rspec, Assembler::narrow_oop_operand);
  patchable_li52(dst, narrowKlass);
}

void MacroAssembler::set_narrow_oop(Register dst, jobject obj) {
  assert(UseCompressedOops, "should only be used for compressed header");
  assert(oop_recorder() != NULL, "this assembler needs an OopRecorder");

  int oop_index = oop_recorder()->find_index(obj);
  RelocationHolder rspec = oop_Relocation::spec(oop_index);

  relocate(rspec, Assembler::narrow_oop_operand);
  patchable_li52(dst, oop_index);
}

// ((OopHandle)result).resolve();
void MacroAssembler::resolve_oop_handle(Register result, Register tmp) {
  // OopHandle::resolve is an indirection.
  access_load_at(T_OBJECT, IN_NATIVE, result, Address(result, 0), tmp, NOREG);
}

// ((WeakHandle)result).resolve();
void MacroAssembler::resolve_weak_handle(Register rresult, Register rtmp) {
  assert_different_registers(rresult, rtmp);
  Label resolved;

  // A null weak handle resolves to null.
  beqz(rresult, resolved);

  // Only 64 bit platforms support GCs that require a tmp register
  // Only IN_HEAP loads require a thread_tmp register
  // WeakHandle::resolve is an indirection like jweak.
  access_load_at(T_OBJECT, IN_NATIVE | ON_PHANTOM_OOP_REF,
                 rresult, Address(rresult), rtmp, /*tmp_thread*/noreg);
  bind(resolved);
}

void MacroAssembler::load_mirror(Register mirror, Register method, Register tmp) {
  // get mirror
  const int mirror_offset = in_bytes(Klass::java_mirror_offset());
  ld_ptr(mirror, method, in_bytes(Method::const_offset()));
  ld_ptr(mirror, mirror, in_bytes(ConstMethod::constants_offset()));
  ld_ptr(mirror, mirror, ConstantPool::pool_holder_offset_in_bytes());
  ld_ptr(mirror, mirror, mirror_offset);
  resolve_oop_handle(mirror, tmp);
}

void MacroAssembler::verify_oop(Register reg, const char* s) {
  if (!VerifyOops) return;

  const char * b = NULL;
  stringStream ss;
  ss.print("verify_oop: %s: %s", reg->name(), s);
  b = code_string(ss.as_string());

  addi_d(SP, SP, -6 * wordSize);
  st_ptr(SCR1, Address(SP, 0 * wordSize));
  st_ptr(SCR2, Address(SP, 1 * wordSize));
  st_ptr(RA, Address(SP, 2 * wordSize));
  st_ptr(A0, Address(SP, 3 * wordSize));
  st_ptr(A1, Address(SP, 4 * wordSize));

  move(A1, reg);
  patchable_li52(A0, (uintptr_t)(address)b); // Fixed size instructions
  li(SCR2, StubRoutines::verify_oop_subroutine_entry_address());
  ld_ptr(SCR2, Address(SCR2));
  jalr(SCR2);

  ld_ptr(SCR1, Address(SP, 0 * wordSize));
  ld_ptr(SCR2, Address(SP, 1 * wordSize));
  ld_ptr(RA, Address(SP, 2 * wordSize));
  ld_ptr(A0, Address(SP, 3 * wordSize));
  ld_ptr(A1, Address(SP, 4 * wordSize));
  addi_d(SP, SP, 6 * wordSize);
}

void MacroAssembler::verify_oop_addr(Address addr, const char* s) {
  if (!VerifyOops) return;

  const char* b = NULL;
  {
    ResourceMark rm;
    stringStream ss;
    ss.print("verify_oop_addr: %s", s);
    b = code_string(ss.as_string());
  }

  addi_d(SP, SP, -6 * wordSize);
  st_ptr(SCR1, Address(SP, 0 * wordSize));
  st_ptr(SCR2, Address(SP, 1 * wordSize));
  st_ptr(RA, Address(SP, 2 * wordSize));
  st_ptr(A0, Address(SP, 3 * wordSize));
  st_ptr(A1, Address(SP, 4 * wordSize));

  patchable_li52(A0, (uintptr_t)(address)b); // Fixed size instructions
  // addr may contain sp so we will have to adjust it based on the
  // pushes that we just did.
  if (addr.uses(SP)) {
    lea(A1, addr);
    ld_ptr(A1, Address(A1, 6 * wordSize));
  } else {
    ld_ptr(A1, addr);
  }

  // call indirectly to solve generation ordering problem
  li(SCR2, StubRoutines::verify_oop_subroutine_entry_address());
  ld_ptr(SCR2, Address(SCR2));
  jalr(SCR2);

  ld_ptr(SCR1, Address(SP, 0 * wordSize));
  ld_ptr(SCR2, Address(SP, 1 * wordSize));
  ld_ptr(RA, Address(SP, 2 * wordSize));
  ld_ptr(A0, Address(SP, 3 * wordSize));
  ld_ptr(A1, Address(SP, 4 * wordSize));
  addi_d(SP, SP, 6 * wordSize);
}

// used registers :  SCR1, SCR2
void MacroAssembler::verify_oop_subroutine() {
  // RA: ra
  // A0: char* error message
  // A1: oop   object to verify
  Label exit, error;
  // increment counter
  li(SCR2, (long)StubRoutines::verify_oop_count_addr());
  ld_w(SCR1, SCR2, 0);
  addi_d(SCR1, SCR1, 1);
  st_w(SCR1, SCR2, 0);

  // make sure object is 'reasonable'
  beqz(A1, exit);         // if obj is NULL it is ok

#if INCLUDE_ZGC
  if (UseZGC) {
    // Check if mask is good.
    // verifies that ZAddressBadMask & A1 == 0
    ld_ptr(AT, Address(TREG, ZThreadLocalData::address_bad_mask_offset()));
    andr(AT, A1, AT);
    bnez(AT, error);
  }
#endif

  // Check if the oop is in the right area of memory
  // const int oop_mask = Universe::verify_oop_mask();
  // const int oop_bits = Universe::verify_oop_bits();
  const uintptr_t oop_mask = Universe::verify_oop_mask();
  const uintptr_t oop_bits = Universe::verify_oop_bits();
  li(SCR1, oop_mask);
  andr(SCR2, A1, SCR1);
  li(SCR1, oop_bits);
  bne(SCR2, SCR1, error);

  // make sure klass is 'reasonable'
  // add for compressedoops
  load_klass(SCR2, A1);
  beqz(SCR2, error);                        // if klass is NULL it is broken
  // return if everything seems ok
  bind(exit);

  jr(RA);

  // handle errors
  bind(error);
  push_call_clobbered_registers();
  call(CAST_FROM_FN_PTR(address, MacroAssembler::debug), relocInfo::runtime_call_type);
  pop_call_clobbered_registers();
  jr(RA);
}

void MacroAssembler::verify_tlab(Register t1, Register t2) {
#ifdef ASSERT
  assert_different_registers(t1, t2, AT);
  if (UseTLAB && VerifyOops) {
    Label next, ok;

    get_thread(t1);

    ld_ptr(t2, t1, in_bytes(JavaThread::tlab_top_offset()));
    ld_ptr(AT, t1, in_bytes(JavaThread::tlab_start_offset()));
    bgeu(t2, AT, next);

    stop("assert(top >= start)");

    bind(next);
    ld_ptr(AT, t1, in_bytes(JavaThread::tlab_end_offset()));
    bgeu(AT, t2, ok);

    stop("assert(top <= end)");

    bind(ok);

  }
#endif
}

RegisterOrConstant MacroAssembler::delayed_value_impl(intptr_t* delayed_value_addr,
                                                      Register tmp,
                                                      int offset) {
  //TODO: LA
  guarantee(0, "LA not implemented yet");
  return RegisterOrConstant(tmp);
}

void MacroAssembler::bswap_h(Register dst, Register src) {
  revb_2h(dst, src);
  ext_w_h(dst, dst);  // sign extension of the lower 16 bits
}

void MacroAssembler::bswap_hu(Register dst, Register src) {
  revb_2h(dst, src);
  bstrpick_d(dst, dst, 15, 0);  // zero extension of the lower 16 bits
}

void MacroAssembler::bswap_w(Register dst, Register src) {
  revb_2w(dst, src);
  slli_w(dst, dst, 0);  // keep sign, clear upper bits
}

void MacroAssembler::cmpxchg(Address addr, Register oldval, Register newval,
                             Register resflag, bool retold, bool acquire,
                             bool weak, bool exchange) {
  assert(oldval != resflag, "oldval != resflag");
  assert(newval != resflag, "newval != resflag");
  assert(addr.base() != resflag, "addr.base() != resflag");
  Label again, succ, fail;

  bind(again);
  ll_d(resflag, addr);
  bne(resflag, oldval, fail);
  move(resflag, newval);
  sc_d(resflag, addr);
  if (weak) {
    b(succ);
  } else {
    beqz(resflag, again);
  }
  if (exchange) {
    move(resflag, oldval);
  }
  b(succ);

  bind(fail);
  if (acquire) {
    membar(Assembler::Membar_mask_bits(LoadLoad|LoadStore));
  } else {
    dbar(0x700);
  }
  if (retold && oldval != R0)
    move(oldval, resflag);
  if (!exchange) {
    move(resflag, R0);
  }
  bind(succ);
}

void MacroAssembler::cmpxchg(Address addr, Register oldval, Register newval,
                             Register tmp, bool retold, bool acquire, Label& succ, Label* fail) {
  assert(oldval != tmp, "oldval != tmp");
  assert(newval != tmp, "newval != tmp");
  Label again, neq;

  bind(again);
  ll_d(tmp, addr);
  bne(tmp, oldval, neq);
  move(tmp, newval);
  sc_d(tmp, addr);
  beqz(tmp, again);
  b(succ);

  bind(neq);
  if (acquire) {
    membar(Assembler::Membar_mask_bits(LoadLoad|LoadStore));
  } else {
    dbar(0x700);
  }
  if (retold && oldval != R0)
    move(oldval, tmp);
  if (fail)
    b(*fail);
}

void MacroAssembler::cmpxchg32(Address addr, Register oldval, Register newval,
                               Register resflag, bool sign, bool retold, bool acquire,
                               bool weak, bool exchange) {
  assert(oldval != resflag, "oldval != resflag");
  assert(newval != resflag, "newval != resflag");
  assert(addr.base() != resflag, "addr.base() != resflag");
  Label again, succ, fail;

  bind(again);
  ll_w(resflag, addr);
  if (!sign)
    lu32i_d(resflag, 0);
  bne(resflag, oldval, fail);
  move(resflag, newval);
  sc_w(resflag, addr);
  if (weak) {
    b(succ);
  } else {
    beqz(resflag, again);
  }
  if (exchange) {
    move(resflag, oldval);
  }
  b(succ);

  bind(fail);
  if (acquire) {
    membar(Assembler::Membar_mask_bits(LoadLoad|LoadStore));
  } else {
    dbar(0x700);
  }
  if (retold && oldval != R0)
    move(oldval, resflag);
  if (!exchange) {
    move(resflag, R0);
  }
  bind(succ);
}

void MacroAssembler::cmpxchg32(Address addr, Register oldval, Register newval, Register tmp,
                               bool sign, bool retold, bool acquire, Label& succ, Label* fail) {
  assert(oldval != tmp, "oldval != tmp");
  assert(newval != tmp, "newval != tmp");
  Label again, neq;

  bind(again);
  ll_w(tmp, addr);
  if (!sign)
    lu32i_d(tmp, 0);
  bne(tmp, oldval, neq);
  move(tmp, newval);
  sc_w(tmp, addr);
  beqz(tmp, again);
  b(succ);

  bind(neq);
  if (acquire) {
    membar(Assembler::Membar_mask_bits(LoadLoad|LoadStore));
  } else {
    dbar(0x700);
  }
  if (retold && oldval != R0)
    move(oldval, tmp);
  if (fail)
    b(*fail);
}

// be sure the three register is different
void MacroAssembler::rem_s(FloatRegister fd, FloatRegister fs, FloatRegister ft, FloatRegister tmp) {
  //TODO: LA
  guarantee(0, "LA not implemented yet");
}

// be sure the three register is different
void MacroAssembler::rem_d(FloatRegister fd, FloatRegister fs, FloatRegister ft, FloatRegister tmp) {
  //TODO: LA
  guarantee(0, "LA not implemented yet");
}

void MacroAssembler::align(int modulus) {
  while (offset() % modulus != 0) nop();
}


void MacroAssembler::verify_FPU(int stack_depth, const char* s) {
  //Unimplemented();
}

static RegSet caller_saved_regset = RegSet::range(A0, A7) + RegSet::range(T0, T8) + RegSet::of(FP, RA) - RegSet::of(SCR1, SCR2);
static FloatRegSet caller_saved_fpu_regset = FloatRegSet::range(F0, F23);

void MacroAssembler::push_call_clobbered_registers_except(RegSet exclude) {
  push(caller_saved_regset - exclude);
  push_fpu(caller_saved_fpu_regset);
}

void MacroAssembler::pop_call_clobbered_registers_except(RegSet exclude) {
  pop_fpu(caller_saved_fpu_regset);
  pop(caller_saved_regset - exclude);
}

void MacroAssembler::push2(Register reg1, Register reg2) {
  addi_d(SP, SP, -16);
  st_d(reg1, SP, 8);
  st_d(reg2, SP, 0);
}

void MacroAssembler::pop2(Register reg1, Register reg2) {
  ld_d(reg1, SP, 8);
  ld_d(reg2, SP, 0);
  addi_d(SP, SP, 16);
}

void MacroAssembler::push(unsigned int bitset) {
  unsigned char regs[31];
  int count = 0;

  bitset >>= 1;
  for (int reg = 1; reg < 31; reg++) {
    if (1 & bitset)
      regs[count++] = reg;
    bitset >>= 1;
  }

  addi_d(SP, SP, -align_up(count, 2) * wordSize);
  for (int i = 0; i < count; i ++)
    st_d(as_Register(regs[i]), SP, i * wordSize);
}

void MacroAssembler::pop(unsigned int bitset) {
  unsigned char regs[31];
  int count = 0;

  bitset >>= 1;
  for (int reg = 1; reg < 31; reg++) {
    if (1 & bitset)
      regs[count++] = reg;
    bitset >>= 1;
  }

  for (int i = 0; i < count; i ++)
    ld_d(as_Register(regs[i]), SP, i * wordSize);
  addi_d(SP, SP, align_up(count, 2) * wordSize);
}

void MacroAssembler::push_fpu(unsigned int bitset) {
  unsigned char regs[32];
  int count = 0;

  if (bitset == 0)
    return;

  for (int reg = 0; reg <= 31; reg++) {
    if (1 & bitset)
      regs[count++] = reg;
    bitset >>= 1;
  }

  addi_d(SP, SP, -align_up(count, 2) * wordSize);
  for (int i = 0; i < count; i++)
    fst_d(as_FloatRegister(regs[i]), SP, i * wordSize);
}

void MacroAssembler::pop_fpu(unsigned int bitset) {
  unsigned char regs[32];
  int count = 0;

  if (bitset == 0)
    return;

  for (int reg = 0; reg <= 31; reg++) {
    if (1 & bitset)
      regs[count++] = reg;
    bitset >>= 1;
  }

  for (int i = 0; i < count; i++)
    fld_d(as_FloatRegister(regs[i]), SP, i * wordSize);
  addi_d(SP, SP, align_up(count, 2) * wordSize);
}

static int vpr_offset(int off) {
  int slots_per_vpr = 0;

  if (UseLASX)
    slots_per_vpr = FloatRegisterImpl::slots_per_lasx_register;
  else if (UseLSX)
    slots_per_vpr = FloatRegisterImpl::slots_per_lsx_register;

  return off * slots_per_vpr * VMRegImpl::stack_slot_size;
}

void MacroAssembler::push_vp(unsigned int bitset) {
  unsigned char regs[32];
  int count = 0;

  if (bitset == 0)
    return;

  for (int reg = 0; reg <= 31; reg++) {
    if (1 & bitset)
      regs[count++] = reg;
    bitset >>= 1;
  }

  addi_d(SP, SP, vpr_offset(-align_up(count, 2)));

  for (int i = 0; i < count; i++) {
    int off = vpr_offset(i);
    if (UseLASX)
      xvst(as_FloatRegister(regs[i]), SP, off);
    else if (UseLSX)
      vst(as_FloatRegister(regs[i]), SP, off);
  }
}

void MacroAssembler::pop_vp(unsigned int bitset) {
  unsigned char regs[32];
  int count = 0;

  if (bitset == 0)
    return;

  for (int reg = 0; reg <= 31; reg++) {
    if (1 & bitset)
      regs[count++] = reg;
    bitset >>= 1;
  }

  for (int i = 0; i < count; i++) {
    int off = vpr_offset(i);
    if (UseLASX)
      xvld(as_FloatRegister(regs[i]), SP, off);
    else if (UseLSX)
      vld(as_FloatRegister(regs[i]), SP, off);
  }

  addi_d(SP, SP, vpr_offset(align_up(count, 2)));
}

void MacroAssembler::load_method_holder(Register holder, Register method) {
  ld_d(holder, Address(method, Method::const_offset()));                      // ConstMethod*
  ld_d(holder, Address(holder, ConstMethod::constants_offset()));             // ConstantPool*
  ld_d(holder, Address(holder, ConstantPool::pool_holder_offset_in_bytes())); // InstanceKlass*
}

void MacroAssembler::load_method_holder_cld(Register rresult, Register rmethod) {
  load_method_holder(rresult, rmethod);
  ld_ptr(rresult, Address(rresult, InstanceKlass::class_loader_data_offset()));
}

// for UseCompressedOops Option
void MacroAssembler::load_klass(Register dst, Register src) {
  if(UseCompressedClassPointers){
    ld_wu(dst, Address(src, oopDesc::klass_offset_in_bytes()));
    decode_klass_not_null(dst);
  } else {
    ld_d(dst, src, oopDesc::klass_offset_in_bytes());
  }
}

void MacroAssembler::store_klass(Register dst, Register src) {
  if(UseCompressedClassPointers){
    encode_klass_not_null(src);
    st_w(src, dst, oopDesc::klass_offset_in_bytes());
  } else {
    st_d(src, dst, oopDesc::klass_offset_in_bytes());
  }
}

void MacroAssembler::load_prototype_header(Register dst, Register src) {
  load_klass(dst, src);
  ld_d(dst, Address(dst, Klass::prototype_header_offset()));
}

void MacroAssembler::store_klass_gap(Register dst, Register src) {
  if (UseCompressedClassPointers) {
    st_w(src, dst, oopDesc::klass_gap_offset_in_bytes());
  }
}

void MacroAssembler::access_load_at(BasicType type, DecoratorSet decorators, Register dst, Address src,
                                    Register tmp1, Register thread_tmp) {
  BarrierSetAssembler* bs = BarrierSet::barrier_set()->barrier_set_assembler();
  decorators = AccessInternal::decorator_fixup(decorators);
  bool as_raw = (decorators & AS_RAW) != 0;
  if (as_raw) {
    bs->BarrierSetAssembler::load_at(this, decorators, type, dst, src, tmp1, thread_tmp);
  } else {
    bs->load_at(this, decorators, type, dst, src, tmp1, thread_tmp);
  }
}

void MacroAssembler::access_store_at(BasicType type, DecoratorSet decorators, Address dst, Register src,
                                     Register tmp1, Register tmp2) {
  BarrierSetAssembler* bs = BarrierSet::barrier_set()->barrier_set_assembler();
  decorators = AccessInternal::decorator_fixup(decorators);
  bool as_raw = (decorators & AS_RAW) != 0;
  if (as_raw) {
    bs->BarrierSetAssembler::store_at(this, decorators, type, dst, src, tmp1, tmp2);
  } else {
    bs->store_at(this, decorators, type, dst, src, tmp1, tmp2);
  }
}

void MacroAssembler::load_heap_oop(Register dst, Address src, Register tmp1,
                                   Register thread_tmp, DecoratorSet decorators) {
  access_load_at(T_OBJECT, IN_HEAP | decorators, dst, src, tmp1, thread_tmp);
}

// Doesn't do verfication, generates fixed size code
void MacroAssembler::load_heap_oop_not_null(Register dst, Address src, Register tmp1,
                                            Register thread_tmp, DecoratorSet decorators) {
  access_load_at(T_OBJECT, IN_HEAP | IS_NOT_NULL | decorators, dst, src, tmp1, thread_tmp);
}

void MacroAssembler::store_heap_oop(Address dst, Register src, Register tmp1,
                                    Register tmp2, DecoratorSet decorators) {
  access_store_at(T_OBJECT, IN_HEAP | decorators, dst, src, tmp1, tmp2);
}

// Used for storing NULLs.
void MacroAssembler::store_heap_oop_null(Address dst) {
  access_store_at(T_OBJECT, IN_HEAP, dst, noreg, noreg, noreg);
}

#ifdef ASSERT
void MacroAssembler::verify_heapbase(const char* msg) {
  assert (UseCompressedOops || UseCompressedClassPointers, "should be compressed");
  assert (Universe::heap() != NULL, "java heap should be initialized");
}
#endif

// Algorithm must match oop.inline.hpp encode_heap_oop.
void MacroAssembler::encode_heap_oop(Register r) {
#ifdef ASSERT
  verify_heapbase("MacroAssembler::encode_heap_oop:heap base corrupted?");
#endif
  verify_oop(r, "broken oop in encode_heap_oop");
  if (CompressedOops::base() == NULL) {
    if (CompressedOops::shift() != 0) {
      assert(LogMinObjAlignmentInBytes == CompressedOops::shift(), "decode alg wrong");
      shr(r, LogMinObjAlignmentInBytes);
    }
    return;
  }

  sub_d(AT, r, S5_heapbase);
  maskeqz(r, AT, r);
  if (CompressedOops::shift() != 0) {
    assert(LogMinObjAlignmentInBytes == CompressedOops::shift(), "decode alg wrong");
    shr(r, LogMinObjAlignmentInBytes);
  }
}

void MacroAssembler::encode_heap_oop(Register dst, Register src) {
#ifdef ASSERT
  verify_heapbase("MacroAssembler::encode_heap_oop:heap base corrupted?");
#endif
  verify_oop(src, "broken oop in encode_heap_oop");
  if (CompressedOops::base() == NULL) {
    if (CompressedOops::shift() != 0) {
      assert(LogMinObjAlignmentInBytes == CompressedOops::shift(), "decode alg wrong");
      srli_d(dst, src, LogMinObjAlignmentInBytes);
    } else {
      if (dst != src) {
        move(dst, src);
      }
    }
    return;
  }

  sub_d(AT, src, S5_heapbase);
  maskeqz(dst, AT, src);
  if (CompressedOops::shift() != 0) {
    assert(LogMinObjAlignmentInBytes == CompressedOops::shift(), "decode alg wrong");
    shr(dst, LogMinObjAlignmentInBytes);
  }
}

void MacroAssembler::encode_heap_oop_not_null(Register r) {
  assert (UseCompressedOops, "should be compressed");
#ifdef ASSERT
  if (CheckCompressedOops) {
    Label ok;
    bne(r, R0, ok);
    stop("null oop passed to encode_heap_oop_not_null");
    bind(ok);
  }
#endif
  verify_oop(r, "broken oop in encode_heap_oop_not_null");
  if (CompressedOops::base() != NULL) {
    sub_d(r, r, S5_heapbase);
  }
  if (CompressedOops::shift() != 0) {
    assert (LogMinObjAlignmentInBytes == CompressedOops::shift(), "decode alg wrong");
    shr(r, LogMinObjAlignmentInBytes);
  }

}

void MacroAssembler::encode_heap_oop_not_null(Register dst, Register src) {
  assert (UseCompressedOops, "should be compressed");
#ifdef ASSERT
  if (CheckCompressedOops) {
    Label ok;
    bne(src, R0, ok);
    stop("null oop passed to encode_heap_oop_not_null2");
    bind(ok);
  }
#endif
  verify_oop(src, "broken oop in encode_heap_oop_not_null2");
  if (CompressedOops::base() == NULL) {
    if (CompressedOops::shift() != 0) {
      assert(LogMinObjAlignmentInBytes == CompressedOops::shift(), "decode alg wrong");
      srli_d(dst, src, LogMinObjAlignmentInBytes);
    } else {
      if (dst != src) {
        move(dst, src);
      }
    }
    return;
  }

  sub_d(dst, src, S5_heapbase);
  if (CompressedOops::shift() != 0) {
    assert(LogMinObjAlignmentInBytes == CompressedOops::shift(), "decode alg wrong");
    shr(dst, LogMinObjAlignmentInBytes);
  }
}

void MacroAssembler::decode_heap_oop(Register r) {
#ifdef ASSERT
  verify_heapbase("MacroAssembler::decode_heap_oop corrupted?");
#endif
  if (CompressedOops::base() == NULL) {
    if (CompressedOops::shift() != 0) {
      assert(LogMinObjAlignmentInBytes == CompressedOops::shift(), "decode alg wrong");
      shl(r, LogMinObjAlignmentInBytes);
    }
    return;
  }

  move(AT, r);
  if (CompressedOops::shift() != 0) {
    assert(LogMinObjAlignmentInBytes == CompressedOops::shift(), "decode alg wrong");
    if (LogMinObjAlignmentInBytes <= 4) {
      alsl_d(r, r, S5_heapbase, LogMinObjAlignmentInBytes - 1);
    } else {
      shl(r, LogMinObjAlignmentInBytes);
      add_d(r, r, S5_heapbase);
    }
  } else {
    add_d(r, r, S5_heapbase);
  }
  maskeqz(r, r, AT);
  verify_oop(r, "broken oop in decode_heap_oop");
}

void MacroAssembler::decode_heap_oop(Register dst, Register src) {
#ifdef ASSERT
  verify_heapbase("MacroAssembler::decode_heap_oop corrupted?");
#endif
  if (CompressedOops::base() == NULL) {
    if (CompressedOops::shift() != 0) {
      assert(LogMinObjAlignmentInBytes == CompressedOops::shift(), "decode alg wrong");
      slli_d(dst, src, LogMinObjAlignmentInBytes);
    } else {
      if (dst != src) {
        move(dst, src);
      }
    }
    return;
  }

  Register cond;
  if (dst == src) {
    cond = AT;
    move(cond, src);
  } else {
    cond = src;
  }
  if (CompressedOops::shift() != 0) {
    assert(LogMinObjAlignmentInBytes == CompressedOops::shift(), "decode alg wrong");
    if (LogMinObjAlignmentInBytes <= 4) {
      alsl_d(dst, src, S5_heapbase, LogMinObjAlignmentInBytes - 1);
    } else {
      slli_d(dst, src, LogMinObjAlignmentInBytes);
      add_d(dst, dst, S5_heapbase);
    }
  } else {
    add_d(dst, src, S5_heapbase);
  }
  maskeqz(dst, dst, cond);
  verify_oop(dst, "broken oop in decode_heap_oop");
}

void MacroAssembler::decode_heap_oop_not_null(Register r) {
  // Note: it will change flags
  assert(UseCompressedOops, "should only be used for compressed headers");
  assert(Universe::heap() != NULL, "java heap should be initialized");
  // Cannot assert, unverified entry point counts instructions (see .ad file)
  // vtableStubs also counts instructions in pd_code_size_limit.
  // Also do not verify_oop as this is called by verify_oop.
  if (CompressedOops::shift() != 0) {
    assert(LogMinObjAlignmentInBytes == CompressedOops::shift(), "decode alg wrong");
    if (CompressedOops::base() != NULL) {
      if (LogMinObjAlignmentInBytes <= 4) {
        alsl_d(r, r, S5_heapbase, LogMinObjAlignmentInBytes - 1);
      } else {
        shl(r, LogMinObjAlignmentInBytes);
        add_d(r, r, S5_heapbase);
      }
    } else {
      shl(r, LogMinObjAlignmentInBytes);
    }
  } else {
    assert(CompressedOops::base() == NULL, "sanity");
  }
}

void MacroAssembler::decode_heap_oop_not_null(Register dst, Register src) {
  assert(UseCompressedOops, "should only be used for compressed headers");
  assert(Universe::heap() != NULL, "java heap should be initialized");
  // Cannot assert, unverified entry point counts instructions (see .ad file)
  // vtableStubs also counts instructions in pd_code_size_limit.
  // Also do not verify_oop as this is called by verify_oop.
  if (CompressedOops::shift() != 0) {
    assert(LogMinObjAlignmentInBytes == CompressedOops::shift(), "decode alg wrong");
    if (CompressedOops::base() != NULL) {
      if (LogMinObjAlignmentInBytes <= 4) {
        alsl_d(dst, src, S5_heapbase, LogMinObjAlignmentInBytes - 1);
      } else {
        slli_d(dst, src, LogMinObjAlignmentInBytes);
        add_d(dst, dst, S5_heapbase);
      }
    } else {
      slli_d(dst, src, LogMinObjAlignmentInBytes);
    }
  } else {
    assert (CompressedOops::base() == NULL, "sanity");
    if (dst != src) {
      move(dst, src);
    }
  }
}

void MacroAssembler::encode_klass_not_null(Register r) {
  if (CompressedKlassPointers::base() != NULL) {
    if (((uint64_t)CompressedKlassPointers::base() & 0xffffffff) == 0
        && CompressedKlassPointers::shift() == 0) {
      bstrpick_d(r, r, 31, 0);
      return;
    }
    assert(r != AT, "Encoding a klass in AT");
    li(AT, (int64_t)CompressedKlassPointers::base());
    sub_d(r, r, AT);
  }
  if (CompressedKlassPointers::shift() != 0) {
    assert (LogKlassAlignmentInBytes == CompressedKlassPointers::shift(), "decode alg wrong");
    shr(r, LogKlassAlignmentInBytes);
  }
}

void MacroAssembler::encode_klass_not_null(Register dst, Register src) {
  if (dst == src) {
    encode_klass_not_null(src);
  } else {
    if (CompressedKlassPointers::base() != NULL) {
      if (((uint64_t)CompressedKlassPointers::base() & 0xffffffff) == 0
          && CompressedKlassPointers::shift() == 0) {
        bstrpick_d(dst, src, 31, 0);
        return;
      }
      li(dst, (int64_t)CompressedKlassPointers::base());
      sub_d(dst, src, dst);
      if (CompressedKlassPointers::shift() != 0) {
        assert (LogKlassAlignmentInBytes == CompressedKlassPointers::shift(), "decode alg wrong");
        shr(dst, LogKlassAlignmentInBytes);
      }
    } else {
      if (CompressedKlassPointers::shift() != 0) {
        assert (LogKlassAlignmentInBytes == CompressedKlassPointers::shift(), "decode alg wrong");
        srli_d(dst, src, LogKlassAlignmentInBytes);
      } else {
        move(dst, src);
      }
    }
  }
}

void MacroAssembler::decode_klass_not_null(Register r) {
  assert(UseCompressedClassPointers, "should only be used for compressed headers");
  assert(r != AT, "Decoding a klass in AT");
  // Cannot assert, unverified entry point counts instructions (see .ad file)
  // vtableStubs also counts instructions in pd_code_size_limit.
  // Also do not verify_oop as this is called by verify_oop.
  if (CompressedKlassPointers::base() != NULL) {
    if (CompressedKlassPointers::shift() == 0) {
      if (((uint64_t)CompressedKlassPointers::base() & 0xffffffff) == 0) {
        lu32i_d(r, (uint64_t)CompressedKlassPointers::base() >> 32);
      } else {
        li(AT, (int64_t)CompressedKlassPointers::base());
        add_d(r, r, AT);
      }
    } else {
      assert(LogKlassAlignmentInBytes == CompressedKlassPointers::shift(), "decode alg wrong");
      assert(LogKlassAlignmentInBytes == Address::times_8, "klass not aligned on 64bits?");
      li(AT, (int64_t)CompressedKlassPointers::base());
      alsl_d(r, r, AT, Address::times_8 - 1);
    }
  } else {
    if (CompressedKlassPointers::shift() != 0) {
      assert(LogKlassAlignmentInBytes == CompressedKlassPointers::shift(), "decode alg wrong");
      shl(r, LogKlassAlignmentInBytes);
    }
  }
}

void MacroAssembler::decode_klass_not_null(Register dst, Register src) {
  assert(UseCompressedClassPointers, "should only be used for compressed headers");
  if (dst == src) {
    decode_klass_not_null(dst);
  } else {
    // Cannot assert, unverified entry point counts instructions (see .ad file)
    // vtableStubs also counts instructions in pd_code_size_limit.
    // Also do not verify_oop as this is called by verify_oop.
    if (CompressedKlassPointers::base() != NULL) {
      if (CompressedKlassPointers::shift() == 0) {
        if (((uint64_t)CompressedKlassPointers::base() & 0xffffffff) == 0) {
          move(dst, src);
          lu32i_d(dst, (uint64_t)CompressedKlassPointers::base() >> 32);
        } else {
          li(dst, (int64_t)CompressedKlassPointers::base());
          add_d(dst, dst, src);
        }
      } else {
        assert(LogKlassAlignmentInBytes == CompressedKlassPointers::shift(), "decode alg wrong");
        assert(LogKlassAlignmentInBytes == Address::times_8, "klass not aligned on 64bits?");
        li(dst, (int64_t)CompressedKlassPointers::base());
        alsl_d(dst, src, dst, Address::times_8 - 1);
      }
    } else {
      if (CompressedKlassPointers::shift() != 0) {
        assert(LogKlassAlignmentInBytes == CompressedKlassPointers::shift(), "decode alg wrong");
        slli_d(dst, src, LogKlassAlignmentInBytes);
      } else {
        move(dst, src);
      }
    }
  }
}

void MacroAssembler::reinit_heapbase() {
  if (UseCompressedOops) {
    if (Universe::heap() != NULL) {
      if (CompressedOops::base() == NULL) {
        move(S5_heapbase, R0);
      } else {
        li(S5_heapbase, (int64_t)CompressedOops::ptrs_base());
      }
    } else {
      li(S5_heapbase, (intptr_t)CompressedOops::ptrs_base_addr());
      ld_d(S5_heapbase, S5_heapbase, 0);
    }
  }
}

void MacroAssembler::check_klass_subtype(Register sub_klass,
                           Register super_klass,
                           Register temp_reg,
                           Label& L_success) {
//implement ind   gen_subtype_check
  Label L_failure;
  check_klass_subtype_fast_path(sub_klass, super_klass, temp_reg,        &L_success, &L_failure, NULL);
  check_klass_subtype_slow_path(sub_klass, super_klass, temp_reg, noreg, &L_success, NULL);
  bind(L_failure);
}

void MacroAssembler::check_klass_subtype_fast_path(Register sub_klass,
                                                   Register super_klass,
                                                   Register temp_reg,
                                                   Label* L_success,
                                                   Label* L_failure,
                                                   Label* L_slow_path,
                                        RegisterOrConstant super_check_offset) {
  assert_different_registers(sub_klass, super_klass, temp_reg);
  bool must_load_sco = (super_check_offset.constant_or_zero() == -1);
  if (super_check_offset.is_register()) {
    assert_different_registers(sub_klass, super_klass,
                               super_check_offset.as_register());
  } else if (must_load_sco) {
    assert(temp_reg != noreg, "supply either a temp or a register offset");
  }

  Label L_fallthrough;
  int label_nulls = 0;
  if (L_success == NULL)   { L_success   = &L_fallthrough; label_nulls++; }
  if (L_failure == NULL)   { L_failure   = &L_fallthrough; label_nulls++; }
  if (L_slow_path == NULL) { L_slow_path = &L_fallthrough; label_nulls++; }
  assert(label_nulls <= 1, "at most one NULL in the batch");

  int sc_offset = in_bytes(Klass::secondary_super_cache_offset());
  int sco_offset = in_bytes(Klass::super_check_offset_offset());
  // If the pointers are equal, we are done (e.g., String[] elements).
  // This self-check enables sharing of secondary supertype arrays among
  // non-primary types such as array-of-interface.  Otherwise, each such
  // type would need its own customized SSA.
  // We move this check to the front of the fast path because many
  // type checks are in fact trivially successful in this manner,
  // so we get a nicely predicted branch right at the start of the check.
  beq(sub_klass, super_klass, *L_success);
  // Check the supertype display:
  if (must_load_sco) {
    ld_wu(temp_reg, super_klass, sco_offset);
    super_check_offset = RegisterOrConstant(temp_reg);
  }
  add_d(AT, sub_klass, super_check_offset.register_or_noreg());
  ld_d(AT, AT, super_check_offset.constant_or_zero());

  // This check has worked decisively for primary supers.
  // Secondary supers are sought in the super_cache ('super_cache_addr').
  // (Secondary supers are interfaces and very deeply nested subtypes.)
  // This works in the same check above because of a tricky aliasing
  // between the super_cache and the primary super display elements.
  // (The 'super_check_addr' can address either, as the case requires.)
  // Note that the cache is updated below if it does not help us find
  // what we need immediately.
  // So if it was a primary super, we can just fail immediately.
  // Otherwise, it's the slow path for us (no success at this point).

  if (super_check_offset.is_register()) {
    beq(super_klass, AT, *L_success);
    addi_d(AT, super_check_offset.as_register(), -sc_offset);
    if (L_failure == &L_fallthrough) {
      beq(AT, R0, *L_slow_path);
    } else {
      bne_far(AT, R0, *L_failure);
      b(*L_slow_path);
    }
  } else if (super_check_offset.as_constant() == sc_offset) {
    // Need a slow path; fast failure is impossible.
    if (L_slow_path == &L_fallthrough) {
      beq(super_klass, AT, *L_success);
    } else {
      bne(super_klass, AT, *L_slow_path);
      b(*L_success);
    }
  } else {
    // No slow path; it's a fast decision.
    if (L_failure == &L_fallthrough) {
      beq(super_klass, AT, *L_success);
    } else {
      bne_far(super_klass, AT, *L_failure);
      b(*L_success);
    }
  }

  bind(L_fallthrough);
}

void MacroAssembler::check_klass_subtype_slow_path(Register sub_klass,
                                                   Register super_klass,
                                                   Register temp_reg,
                                                   Register temp2_reg,
                                                   Label* L_success,
                                                   Label* L_failure,
                                                   bool set_cond_codes) {
  if (temp2_reg == noreg)
    temp2_reg = TSR;
  assert_different_registers(sub_klass, super_klass, temp_reg, temp2_reg);
#define IS_A_TEMP(reg) ((reg) == temp_reg || (reg) == temp2_reg)

  Label L_fallthrough;
  int label_nulls = 0;
  if (L_success == NULL)   { L_success   = &L_fallthrough; label_nulls++; }
  if (L_failure == NULL)   { L_failure   = &L_fallthrough; label_nulls++; }
  assert(label_nulls <= 1, "at most one NULL in the batch");

  // a couple of useful fields in sub_klass:
  int ss_offset = in_bytes(Klass::secondary_supers_offset());
  int sc_offset = in_bytes(Klass::secondary_super_cache_offset());
  Address secondary_supers_addr(sub_klass, ss_offset);
  Address super_cache_addr(     sub_klass, sc_offset);

  // Do a linear scan of the secondary super-klass chain.
  // This code is rarely used, so simplicity is a virtue here.
  // The repne_scan instruction uses fixed registers, which we must spill.
  // Don't worry too much about pre-existing connections with the input regs.

#ifndef PRODUCT
  int* pst_counter = &SharedRuntime::_partial_subtype_ctr;
  ExternalAddress pst_counter_addr((address) pst_counter);
#endif //PRODUCT

  // We will consult the secondary-super array.
  ld_d(temp_reg, secondary_supers_addr);
  // Load the array length.
  ld_w(temp2_reg, Address(temp_reg, Array<Klass*>::length_offset_in_bytes()));
  // Skip to start of data.
  addi_d(temp_reg, temp_reg, Array<Klass*>::base_offset_in_bytes());

  Label Loop, subtype;
  bind(Loop);
  beq(temp2_reg, R0, *L_failure);
  ld_d(AT, temp_reg, 0);
  addi_d(temp_reg, temp_reg, 1 * wordSize);
  beq(AT, super_klass, subtype);
  addi_d(temp2_reg, temp2_reg, -1);
  b(Loop);

  bind(subtype);
  st_d(super_klass, super_cache_addr);
  if (L_success != &L_fallthrough) {
    b(*L_success);
  }

  // Success.  Cache the super we found and proceed in triumph.
#undef IS_A_TEMP

  bind(L_fallthrough);
}

void MacroAssembler::clinit_barrier(Register klass, Register scratch, Label* L_fast_path, Label* L_slow_path) {
  Register rthread = TREG;
#ifndef OPT_THREAD
  get_thread(rthread);
#endif

  assert(L_fast_path != NULL || L_slow_path != NULL, "at least one is required");
  assert_different_registers(klass, rthread, scratch);

  Label L_fallthrough;
  if (L_fast_path == NULL) {
    L_fast_path = &L_fallthrough;
  } else if (L_slow_path == NULL) {
    L_slow_path = &L_fallthrough;
  }

  // Fast path check: class is fully initialized
  ld_b(scratch, Address(klass, InstanceKlass::init_state_offset()));
  addi_d(scratch, scratch, -InstanceKlass::fully_initialized);
  beqz(scratch, *L_fast_path);

  // Fast path check: current thread is initializer thread
  ld_d(scratch, Address(klass, InstanceKlass::init_thread_offset()));
  if (L_slow_path == &L_fallthrough) {
    beq(rthread, scratch, *L_fast_path);
    bind(*L_slow_path);
  } else if (L_fast_path == &L_fallthrough) {
    bne(rthread, scratch, *L_slow_path);
    bind(*L_fast_path);
  } else {
    Unimplemented();
  }
}

void MacroAssembler::get_vm_result(Register oop_result, Register java_thread) {
  ld_d(oop_result, Address(java_thread, JavaThread::vm_result_offset()));
  st_d(R0, Address(java_thread, JavaThread::vm_result_offset()));
  verify_oop(oop_result, "broken oop in call_VM_base");
}

void MacroAssembler::get_vm_result_2(Register metadata_result, Register java_thread) {
  ld_d(metadata_result, Address(java_thread, JavaThread::vm_result_2_offset()));
  st_d(R0, Address(java_thread, JavaThread::vm_result_2_offset()));
}

Address MacroAssembler::argument_address(RegisterOrConstant arg_slot,
                                         int extra_slot_offset) {
  // cf. TemplateTable::prepare_invoke(), if (load_receiver).
  int stackElementSize = Interpreter::stackElementSize;
  int offset = Interpreter::expr_offset_in_bytes(extra_slot_offset+0);
#ifdef ASSERT
  int offset1 = Interpreter::expr_offset_in_bytes(extra_slot_offset+1);
  assert(offset1 - offset == stackElementSize, "correct arithmetic");
#endif
  Register             scale_reg    = NOREG;
  Address::ScaleFactor scale_factor = Address::no_scale;
  if (arg_slot.is_constant()) {
    offset += arg_slot.as_constant() * stackElementSize;
  } else {
    scale_reg    = arg_slot.as_register();
    scale_factor = Address::times_8;
  }
  // We don't push RA on stack in prepare_invoke.
  //  offset += wordSize;           // return PC is on stack
  if(scale_reg==NOREG) return Address(SP, offset);
  else {
  alsl_d(scale_reg, scale_reg, SP, scale_factor - 1);
  return Address(scale_reg, offset);
  }
}

SkipIfEqual::~SkipIfEqual() {
  _masm->bind(_label);
}

void MacroAssembler::load_sized_value(Register dst, Address src, size_t size_in_bytes, bool is_signed, Register dst2) {
  switch (size_in_bytes) {
  case  8:  ld_d(dst, src); break;
  case  4:  ld_w(dst, src); break;
  case  2:  is_signed ? ld_h(dst, src) : ld_hu(dst, src); break;
  case  1:  is_signed ? ld_b( dst, src) : ld_bu( dst, src); break;
  default:  ShouldNotReachHere();
  }
}

void MacroAssembler::store_sized_value(Address dst, Register src, size_t size_in_bytes, Register src2) {
  switch (size_in_bytes) {
  case  8:  st_d(src, dst); break;
  case  4:  st_w(src, dst); break;
  case  2:  st_h(src, dst); break;
  case  1:  st_b(src, dst); break;
  default:  ShouldNotReachHere();
  }
}

// Look up the method for a megamorphic invokeinterface call.
// The target method is determined by <intf_klass, itable_index>.
// The receiver klass is in recv_klass.
// On success, the result will be in method_result, and execution falls through.
// On failure, execution transfers to the given label.
void MacroAssembler::lookup_interface_method(Register recv_klass,
                                             Register intf_klass,
                                             RegisterOrConstant itable_index,
                                             Register method_result,
                                             Register scan_temp,
                                             Label& L_no_such_interface,
                                             bool return_method) {
  assert_different_registers(recv_klass, intf_klass, scan_temp, AT);
  assert_different_registers(method_result, intf_klass, scan_temp, AT);
  assert(recv_klass != method_result || !return_method,
         "recv_klass can be destroyed when method isn't needed");

  assert(itable_index.is_constant() || itable_index.as_register() == method_result,
         "caller must use same register for non-constant itable index as for method");

  // Compute start of first itableOffsetEntry (which is at the end of the vtable)
  int vtable_base = in_bytes(Klass::vtable_start_offset());
  int itentry_off = itableMethodEntry::method_offset_in_bytes();
  int scan_step   = itableOffsetEntry::size() * wordSize;
  int vte_size    = vtableEntry::size() * wordSize;
  Address::ScaleFactor times_vte_scale = Address::times_ptr;
  assert(vte_size == wordSize, "else adjust times_vte_scale");

  ld_w(scan_temp, Address(recv_klass, Klass::vtable_length_offset()));

  // %%% Could store the aligned, prescaled offset in the klassoop.
  alsl_d(scan_temp, scan_temp, recv_klass, times_vte_scale - 1);
  addi_d(scan_temp, scan_temp, vtable_base);

  if (return_method) {
    // Adjust recv_klass by scaled itable_index, so we can free itable_index.
    if (itable_index.is_constant()) {
      li(AT, (itable_index.as_constant() * itableMethodEntry::size() * wordSize) + itentry_off);
      add_d(recv_klass, recv_klass, AT);
    } else {
      assert(itableMethodEntry::size() * wordSize == wordSize, "adjust the scaling in the code below");
      alsl_d(AT, itable_index.as_register(), recv_klass, (int)Address::times_ptr - 1);
      addi_d(recv_klass, AT, itentry_off);
    }
  }

  Label search, found_method;

  ld_d(method_result, Address(scan_temp, itableOffsetEntry::interface_offset_in_bytes()));
  beq(intf_klass, method_result, found_method);

  bind(search);
  // Check that the previous entry is non-null.  A null entry means that
  // the receiver class doesn't implement the interface, and wasn't the
  // same as when the caller was compiled.
  beqz(method_result, L_no_such_interface);
  addi_d(scan_temp, scan_temp, scan_step);
  ld_d(method_result, Address(scan_temp, itableOffsetEntry::interface_offset_in_bytes()));
  bne(intf_klass, method_result, search);

  bind(found_method);
  if (return_method) {
    // Got a hit.
    ld_wu(scan_temp, Address(scan_temp, itableOffsetEntry::offset_offset_in_bytes()));
    ldx_d(method_result, recv_klass, scan_temp);
  }
}

// virtual method calling
void MacroAssembler::lookup_virtual_method(Register recv_klass,
                                           RegisterOrConstant vtable_index,
                                           Register method_result) {
  const int base = in_bytes(Klass::vtable_start_offset());
  assert(vtableEntry::size() * wordSize == wordSize, "else adjust the scaling in the code below");

  if (vtable_index.is_constant()) {
    li(AT, vtable_index.as_constant());
    alsl_d(AT, AT, recv_klass, Address::times_ptr - 1);
  } else {
    alsl_d(AT, vtable_index.as_register(), recv_klass, Address::times_ptr - 1);
  }

  ld_d(method_result, AT, base + vtableEntry::method_offset_in_bytes());
}

void MacroAssembler::load_byte_map_base(Register reg) {
  CardTable::CardValue* byte_map_base =
    ((CardTableBarrierSet*)(BarrierSet::barrier_set()))->card_table()->byte_map_base();

  // Strictly speaking the byte_map_base isn't an address at all, and it might
  // even be negative. It is thus materialised as a constant.
  li(reg, (uint64_t)byte_map_base);
}

void MacroAssembler::clear_jweak_tag(Register possibly_jweak) {
  const int32_t inverted_jweak_mask = ~static_cast<int32_t>(JNIHandles::weak_tag_mask);
  STATIC_ASSERT(inverted_jweak_mask == -2); // otherwise check this code
  // The inverted mask is sign-extended
  li(AT, inverted_jweak_mask);
  andr(possibly_jweak, AT, possibly_jweak);
}

void MacroAssembler::resolve_jobject(Register value,
                                     Register thread,
                                     Register tmp) {
  assert_different_registers(value, thread, tmp);
  Label done, not_weak;
  beq(value, R0, done);                // Use NULL as-is.
  li(AT, JNIHandles::weak_tag_mask); // Test for jweak tag.
  andr(AT, value, AT);
  beq(AT, R0, not_weak);
  // Resolve jweak.
  access_load_at(T_OBJECT, IN_NATIVE | ON_PHANTOM_OOP_REF,
                 value, Address(value, -JNIHandles::weak_tag_value), tmp, thread);
  verify_oop(value);
  b(done);
  bind(not_weak);
  // Resolve (untagged) jobject.
  access_load_at(T_OBJECT, IN_NATIVE, value, Address(value, 0), tmp, thread);
  verify_oop(value);
  bind(done);
}

void MacroAssembler::lea(Register rd, Address src) {
  Register dst   = rd;
  Register base  = src.base();
  Register index = src.index();

  int scale = src.scale();
  int disp  = src.disp();

  if (index == noreg) {
    if (is_simm(disp, 12)) {
      addi_d(dst, base, disp);
    } else {
      lu12i_w(AT, split_low20(disp >> 12));
      if (split_low12(disp))
        ori(AT, AT, split_low12(disp));
      add_d(dst, base, AT);
    }
  } else {
    if (scale == 0) {
      if (is_simm(disp, 12)) {
        add_d(AT, base, index);
        addi_d(dst, AT, disp);
      } else {
        lu12i_w(AT, split_low20(disp >> 12));
        if (split_low12(disp))
          ori(AT, AT, split_low12(disp));
        add_d(AT, base, AT);
        add_d(dst, AT, index);
      }
    } else {
      if (is_simm(disp, 12)) {
        alsl_d(AT, index, base, scale - 1);
        addi_d(dst, AT, disp);
      } else {
        lu12i_w(AT, split_low20(disp >> 12));
        if (split_low12(disp))
          ori(AT, AT, split_low12(disp));
        add_d(AT, AT, base);
        alsl_d(dst, index, AT, scale - 1);
      }
    }
  }
}

void MacroAssembler::lea(Register dst, AddressLiteral adr) {
  code_section()->relocate(pc(), adr.rspec());
  pcaddi(dst, (adr.target() - pc()) >> 2);
}

int MacroAssembler::patched_branch(int dest_pos, int inst, int inst_pos) {
  int v = (dest_pos - inst_pos) >> 2;
  switch(high(inst, 6)) {
  case beq_op:
  case bne_op:
  case blt_op:
  case bge_op:
  case bltu_op:
  case bgeu_op:
    assert(is_simm16(v), "must be simm16");
#ifndef PRODUCT
    if(!is_simm16(v))
    {
      tty->print_cr("must be simm16");
      tty->print_cr("Inst: %x", inst);
    }
#endif

    inst &= 0xfc0003ff;
    inst |= ((v & 0xffff) << 10);
    break;
  case beqz_op:
  case bnez_op:
  case bccondz_op:
    assert(is_simm(v, 21), "must be simm21");
#ifndef PRODUCT
    if(!is_simm(v, 21))
    {
      tty->print_cr("must be simm21");
      tty->print_cr("Inst: %x", inst);
    }
#endif

    inst &= 0xfc0003e0;
    inst |= ( ((v & 0xffff) << 10) | ((v >> 16) & 0x1f) );
    break;
  case b_op:
  case bl_op:
    assert(is_simm(v, 26), "must be simm26");
#ifndef PRODUCT
    if(!is_simm(v, 26))
    {
      tty->print_cr("must be simm26");
      tty->print_cr("Inst: %x", inst);
    }
#endif

    inst &= 0xfc000000;
    inst |= ( ((v & 0xffff) << 10) | ((v >> 16) & 0x3ff) );
    break;
  default:
    ShouldNotReachHere();
    break;
  }
  return inst;
}

void MacroAssembler::cmp_cmov(Register  op1,
                              Register  op2,
                              Register  dst,
                              Register  src1,
                              Register  src2,
                              CMCompare cmp,
                              bool      is_signed) {
  switch (cmp) {
    case EQ:
      sub_d(AT, op1, op2);
      if (dst == src2) {
        masknez(dst, src2, AT);
        maskeqz(AT, src1, AT);
      } else {
        maskeqz(dst, src1, AT);
        masknez(AT, src2, AT);
      }
      break;

    case NE:
      sub_d(AT, op1, op2);
      if (dst == src2) {
        maskeqz(dst, src2, AT);
        masknez(AT, src1, AT);
      } else {
        masknez(dst, src1, AT);
        maskeqz(AT, src2, AT);
      }
      break;

    case GT:
      if (is_signed) {
        slt(AT, op2, op1);
      } else {
        sltu(AT, op2, op1);
      }
      if(dst == src2) {
        maskeqz(dst, src2, AT);
        masknez(AT, src1, AT);
      } else {
        masknez(dst, src1, AT);
        maskeqz(AT, src2, AT);
      }
      break;
    case GE:
      if (is_signed) {
        slt(AT, op1, op2);
      } else {
        sltu(AT, op1, op2);
      }
      if(dst == src2) {
        masknez(dst, src2, AT);
        maskeqz(AT, src1, AT);
      } else {
        maskeqz(dst, src1, AT);
        masknez(AT, src2, AT);
      }
      break;

    case LT:
      if (is_signed) {
        slt(AT, op1, op2);
      } else {
        sltu(AT, op1, op2);
      }
      if(dst == src2) {
        maskeqz(dst, src2, AT);
        masknez(AT, src1, AT);
      } else {
        masknez(dst, src1, AT);
        maskeqz(AT, src2, AT);
      }
      break;
    case LE:
      if (is_signed) {
        slt(AT, op2, op1);
      } else {
        sltu(AT, op2, op1);
      }
      if(dst == src2) {
        masknez(dst, src2, AT);
        maskeqz(AT, src1, AT);
      } else {
        maskeqz(dst, src1, AT);
        masknez(AT, src2, AT);
      }
      break;
    default:
      Unimplemented();
  }
  OR(dst, dst, AT);
}

void MacroAssembler::cmp_cmov(Register  op1,
                              Register  op2,
                              Register  dst,
                              Register  src,
                              CMCompare cmp,
                              bool      is_signed) {
  switch (cmp) {
    case EQ:
      sub_d(AT, op1, op2);
      maskeqz(dst, dst, AT);
      masknez(AT, src, AT);
      break;

    case NE:
      sub_d(AT, op1, op2);
      masknez(dst, dst, AT);
      maskeqz(AT, src, AT);
      break;

    case GT:
      if (is_signed) {
        slt(AT, op2, op1);
      } else {
        sltu(AT, op2, op1);
      }
      masknez(dst, dst, AT);
      maskeqz(AT, src, AT);
      break;

    case GE:
      if (is_signed) {
        slt(AT, op1, op2);
      } else {
        sltu(AT, op1, op2);
      }
      maskeqz(dst, dst, AT);
      masknez(AT, src, AT);
      break;

    case LT:
      if (is_signed) {
        slt(AT, op1, op2);
      } else {
        sltu(AT, op1, op2);
      }
      masknez(dst, dst, AT);
      maskeqz(AT, src, AT);
      break;

    case LE:
      if (is_signed) {
        slt(AT, op2, op1);
      } else {
        sltu(AT, op2, op1);
      }
      maskeqz(dst, dst, AT);
      masknez(AT, src, AT);
      break;

    default:
      Unimplemented();
  }
  OR(dst, dst, AT);
}


void MacroAssembler::cmp_cmov(FloatRegister op1,
                              FloatRegister op2,
                              Register      dst,
                              Register      src,
                              FloatRegister tmp1,
                              FloatRegister tmp2,
                              CMCompare     cmp,
                              bool          is_float) {
  movgr2fr_d(tmp1, dst);
  movgr2fr_d(tmp2, src);

  switch(cmp) {
    case EQ:
      if (is_float) {
        fcmp_ceq_s(FCC0, op1, op2);
      } else {
        fcmp_ceq_d(FCC0, op1, op2);
      }
      fsel(tmp1, tmp1, tmp2, FCC0);
      break;

    case NE:
      if (is_float) {
        fcmp_ceq_s(FCC0, op1, op2);
      } else {
        fcmp_ceq_d(FCC0, op1, op2);
      }
      fsel(tmp1, tmp2, tmp1, FCC0);
      break;

    case GT:
      if (is_float) {
        fcmp_cule_s(FCC0, op1, op2);
      } else {
        fcmp_cule_d(FCC0, op1, op2);
      }
      fsel(tmp1, tmp2, tmp1, FCC0);
      break;

    case GE:
      if (is_float) {
        fcmp_cult_s(FCC0, op1, op2);
      } else {
        fcmp_cult_d(FCC0, op1, op2);
      }
      fsel(tmp1, tmp2, tmp1, FCC0);
      break;

    case LT:
      if (is_float) {
        fcmp_cult_s(FCC0, op1, op2);
      } else {
        fcmp_cult_d(FCC0, op1, op2);
      }
      fsel(tmp1, tmp1, tmp2, FCC0);
      break;

    case LE:
      if (is_float) {
        fcmp_cule_s(FCC0, op1, op2);
      } else {
        fcmp_cule_d(FCC0, op1, op2);
      }
      fsel(tmp1, tmp1, tmp2, FCC0);
      break;

    default:
      Unimplemented();
  }

  movfr2gr_d(dst, tmp1);
}

void MacroAssembler::cmp_cmov(FloatRegister op1,
                              FloatRegister op2,
                              FloatRegister dst,
                              FloatRegister src,
                              CMCompare     cmp,
                              bool          is_float) {
  switch(cmp) {
    case EQ:
      if (!is_float) {
        fcmp_ceq_d(FCC0, op1, op2);
      } else {
        fcmp_ceq_s(FCC0, op1, op2);
      }
      fsel(dst, dst, src, FCC0);
      break;

    case NE:
      if (!is_float) {
        fcmp_ceq_d(FCC0, op1, op2);
      } else {
        fcmp_ceq_s(FCC0, op1, op2);
      }
      fsel(dst, src, dst, FCC0);
      break;

    case GT:
      if (!is_float) {
        fcmp_cule_d(FCC0, op1, op2);
      } else {
        fcmp_cule_s(FCC0, op1, op2);
      }
      fsel(dst, src, dst, FCC0);
      break;

    case GE:
      if (!is_float) {
        fcmp_cult_d(FCC0, op1, op2);
      } else {
        fcmp_cult_s(FCC0, op1, op2);
      }
      fsel(dst, src, dst, FCC0);
      break;

    case LT:
      if (!is_float) {
        fcmp_cult_d(FCC0, op1, op2);
      } else {
        fcmp_cult_s(FCC0, op1, op2);
      }
      fsel(dst, dst, src, FCC0);
      break;

    case LE:
      if (!is_float) {
        fcmp_cule_d(FCC0, op1, op2);
      } else {
        fcmp_cule_s(FCC0, op1, op2);
      }
      fsel(dst, dst, src, FCC0);
      break;

    default:
      Unimplemented();
  }
}

void MacroAssembler::cmp_cmov(Register      op1,
                              Register      op2,
                              FloatRegister dst,
                              FloatRegister src,
                              FloatRegister tmp1,
                              FloatRegister tmp2,
                              CMCompare     cmp) {
  movgr2fr_w(tmp1, R0);

  switch (cmp) {
    case EQ:
      sub_d(AT, op1, op2);
      movgr2fr_w(tmp2, AT);
      fcmp_ceq_s(FCC0, tmp1, tmp2);
      fsel(dst, dst, src, FCC0);
      break;

    case NE:
      sub_d(AT, op1, op2);
      movgr2fr_w(tmp2, AT);
      fcmp_ceq_s(FCC0, tmp1, tmp2);
      fsel(dst, src, dst, FCC0);
      break;

    case GT:
      slt(AT, op2, op1);
      movgr2fr_w(tmp2, AT);
      fcmp_ceq_s(FCC0, tmp1, tmp2);
      fsel(dst, src, dst, FCC0);
      break;

    case GE:
      slt(AT, op1, op2);
      movgr2fr_w(tmp2, AT);
      fcmp_ceq_s(FCC0, tmp1, tmp2);
      fsel(dst, dst, src, FCC0);
      break;

    case LT:
      slt(AT, op1, op2);
      movgr2fr_w(tmp2, AT);
      fcmp_ceq_s(FCC0, tmp1, tmp2);
      fsel(dst, src, dst, FCC0);
      break;

    case LE:
      slt(AT, op2, op1);
      movgr2fr_w(tmp2, AT);
      fcmp_ceq_s(FCC0, tmp1, tmp2);
      fsel(dst, dst, src, FCC0);
      break;

    default:
      Unimplemented();
  }
}

void MacroAssembler::membar(Membar_mask_bits hint){
  address prev = pc() - NativeInstruction::sync_instruction_size;
  address last = code()->last_insn();
  if (last != NULL && ((NativeInstruction*)last)->is_sync() && prev == last) {
    code()->set_last_insn(NULL);
    NativeMembar *membar = (NativeMembar*)prev;
    // merged membar
    // e.g. LoadLoad and LoadLoad|LoadStore to LoadLoad|LoadStore
    membar->set_hint(membar->get_hint() & (~hint & 0xF));
    block_comment("merged membar");
  } else {
    code()->set_last_insn(pc());
    Assembler::membar(hint);
  }
}

/**
 * Emits code to update CRC-32 with a byte value according to constants in table
 *
 * @param [in,out]crc   Register containing the crc.
 * @param [in]val       Register containing the byte to fold into the CRC.
 * @param [in]table     Register containing the table of crc constants.
 *
 * uint32_t crc;
 * val = crc_table[(val ^ crc) & 0xFF];
 * crc = val ^ (crc >> 8);
**/
void MacroAssembler::update_byte_crc32(Register crc, Register val, Register table) {
  xorr(val, val, crc);
  andi(val, val, 0xff);
  ld_w(val, Address(table, val, Address::times_4, 0));
  srli_w(crc, crc, 8);
  xorr(crc, val, crc);
}

/**
 * @param crc   register containing existing CRC (32-bit)
 * @param buf   register pointing to input byte buffer (byte*)
 * @param len   register containing number of bytes
 * @param tmp   scratch register
**/
void MacroAssembler::kernel_crc32(Register crc, Register buf, Register len, Register tmp) {
  Label CRC_by64_loop, CRC_by4_loop, CRC_by1_loop, CRC_less64, CRC_by64_pre, CRC_by32_loop, CRC_less32, L_exit;
  assert_different_registers(crc, buf, len, tmp);

    nor(crc, crc, R0);

    addi_d(len, len, -64);
    bge(len, R0, CRC_by64_loop);
    addi_d(len, len, 64-4);
    bge(len, R0, CRC_by4_loop);
    addi_d(len, len, 4);
    blt(R0, len, CRC_by1_loop);
    b(L_exit);

  bind(CRC_by64_loop);
    ld_d(tmp, buf, 0);
    crc_w_d_w(crc, tmp, crc);
    ld_d(tmp, buf, 8);
    crc_w_d_w(crc, tmp, crc);
    ld_d(tmp, buf, 16);
    crc_w_d_w(crc, tmp, crc);
    ld_d(tmp, buf, 24);
    crc_w_d_w(crc, tmp, crc);
    ld_d(tmp, buf, 32);
    crc_w_d_w(crc, tmp, crc);
    ld_d(tmp, buf, 40);
    crc_w_d_w(crc, tmp, crc);
    ld_d(tmp, buf, 48);
    crc_w_d_w(crc, tmp, crc);
    ld_d(tmp, buf, 56);
    crc_w_d_w(crc, tmp, crc);
    addi_d(buf, buf, 64);
    addi_d(len, len, -64);
    bge(len, R0, CRC_by64_loop);
    addi_d(len, len, 64-4);
    bge(len, R0, CRC_by4_loop);
    addi_d(len, len, 4);
    blt(R0, len, CRC_by1_loop);
    b(L_exit);

  bind(CRC_by4_loop);
    ld_w(tmp, buf, 0);
    crc_w_w_w(crc, tmp, crc);
    addi_d(buf, buf, 4);
    addi_d(len, len, -4);
    bge(len, R0, CRC_by4_loop);
    addi_d(len, len, 4);
    bge(R0, len, L_exit);

  bind(CRC_by1_loop);
    ld_b(tmp, buf, 0);
    crc_w_b_w(crc, tmp, crc);
    addi_d(buf, buf, 1);
    addi_d(len, len, -1);
    blt(R0, len, CRC_by1_loop);

  bind(L_exit);
    nor(crc, crc, R0);
}

/**
 * @param crc   register containing existing CRC (32-bit)
 * @param buf   register pointing to input byte buffer (byte*)
 * @param len   register containing number of bytes
 * @param tmp   scratch register
**/
void MacroAssembler::kernel_crc32c(Register crc, Register buf, Register len, Register tmp) {
  Label CRC_by64_loop, CRC_by4_loop, CRC_by1_loop, CRC_less64, CRC_by64_pre, CRC_by32_loop, CRC_less32, L_exit;
  assert_different_registers(crc, buf, len, tmp);

    addi_d(len, len, -64);
    bge(len, R0, CRC_by64_loop);
    addi_d(len, len, 64-4);
    bge(len, R0, CRC_by4_loop);
    addi_d(len, len, 4);
    blt(R0, len, CRC_by1_loop);
    b(L_exit);

  bind(CRC_by64_loop);
    ld_d(tmp, buf, 0);
    crcc_w_d_w(crc, tmp, crc);
    ld_d(tmp, buf, 8);
    crcc_w_d_w(crc, tmp, crc);
    ld_d(tmp, buf, 16);
    crcc_w_d_w(crc, tmp, crc);
    ld_d(tmp, buf, 24);
    crcc_w_d_w(crc, tmp, crc);
    ld_d(tmp, buf, 32);
    crcc_w_d_w(crc, tmp, crc);
    ld_d(tmp, buf, 40);
    crcc_w_d_w(crc, tmp, crc);
    ld_d(tmp, buf, 48);
    crcc_w_d_w(crc, tmp, crc);
    ld_d(tmp, buf, 56);
    crcc_w_d_w(crc, tmp, crc);
    addi_d(buf, buf, 64);
    addi_d(len, len, -64);
    bge(len, R0, CRC_by64_loop);
    addi_d(len, len, 64-4);
    bge(len, R0, CRC_by4_loop);
    addi_d(len, len, 4);
    blt(R0, len, CRC_by1_loop);
    b(L_exit);

  bind(CRC_by4_loop);
    ld_w(tmp, buf, 0);
    crcc_w_w_w(crc, tmp, crc);
    addi_d(buf, buf, 4);
    addi_d(len, len, -4);
    bge(len, R0, CRC_by4_loop);
    addi_d(len, len, 4);
    bge(R0, len, L_exit);

  bind(CRC_by1_loop);
    ld_b(tmp, buf, 0);
    crcc_w_b_w(crc, tmp, crc);
    addi_d(buf, buf, 1);
    addi_d(len, len, -1);
    blt(R0, len, CRC_by1_loop);

  bind(L_exit);
}

// This method checks if provided byte array contains byte with highest bit set.
void MacroAssembler::has_negatives(Register ary1, Register len, Register result) {
    Label Loop, End, Nega, Done;

    orr(result, R0, R0);
    bge(R0, len, Done);

    li(AT, 0x8080808080808080);

    addi_d(len, len, -8);
    blt(len, R0, End);

  bind(Loop);
    ld_d(result, ary1, 0);
    andr(result, result, AT);
    bnez(result, Nega);
    beqz(len, Done);
    addi_d(len, len, -8);
    addi_d(ary1, ary1, 8);
    bge(len, R0, Loop);

  bind(End);
    ld_d(result, ary1, 0);
    slli_d(len, len, 3);
    sub_d(len, R0, len);
    sll_d(result, result, len);
    andr(result, result, AT);
    beqz(result, Done);

  bind(Nega);
    ori(result, R0, 1);

  bind(Done);
}

// Compress char[] to byte[]. len must be positive int.
// jtreg: TestStringIntrinsicRangeChecks.java
void MacroAssembler::char_array_compress(Register src, Register dst,
                                         Register len, Register result,
                                         Register tmp1, Register tmp2,
                                         Register tmp3) {
  Label Loop, Done, Once, Fail;

  move(result, len);
  bge(R0, result, Done);

  srli_w(AT, len, 2);
  andi(len, len, 3);

  li(tmp3, 0xff00ff00ff00ff00);

  bind(Loop);
    beqz(AT, Once);
    ld_d(tmp1, src, 0);
    andr(tmp2, tmp3, tmp1);          // not latin-1, stop here
    bnez(tmp2, Fail);

    // 0x00a100b200c300d4 -> 0x00000000a1b2c3d4
    srli_d(tmp2, tmp1, 8);
    orr(tmp2, tmp2, tmp1);           // 0x00a1a1b2b2c3c3d4
    bstrpick_d(tmp1, tmp2, 47, 32);  // 0x0000a1b2
    slli_d(tmp1, tmp1, 16);          // 0xa1b20000
    bstrins_d(tmp1, tmp2, 15, 0);    // 0xa1b2c3d4

    st_w(tmp1, dst, 0);
    addi_w(AT, AT, -1);
    addi_d(dst, dst, 4);
    addi_d(src, src, 8);
    b(Loop);

  bind(Once);
    beqz(len, Done);
    ld_d(AT, src, 0);

    bstrpick_d(tmp1, AT, 15, 0);
    andr(tmp2, tmp3, tmp1);
    bnez(tmp2, Fail);
    st_b(tmp1, dst, 0);
    addi_w(len, len, -1);

    beqz(len, Done);
    bstrpick_d(tmp1, AT, 31, 16);
    andr(tmp2, tmp3, tmp1);
    bnez(tmp2, Fail);
    st_b(tmp1, dst, 1);
    addi_w(len, len, -1);

    beqz(len, Done);
    bstrpick_d(tmp1, AT, 47, 32);
    andr(tmp2, tmp3, tmp1);
    bnez(tmp2, Fail);
    st_b(tmp1, dst, 2);
    b(Done);

  bind(Fail);
    move(result, R0);

  bind(Done);
}

// Inflate byte[] to char[]. len must be positive int.
// jtreg:test/jdk/sun/nio/cs/FindDecoderBugs.java
void MacroAssembler::byte_array_inflate(Register src, Register dst, Register len,
                                        Register tmp1, Register tmp2) {
  Label Loop, Once, Done;

  bge(R0, len, Done);

  srli_w(AT, len, 2);
  andi(len, len, 3);

  bind(Loop);
    beqz(AT, Once);
    ld_wu(tmp1, src, 0);

    // 0x00000000a1b2c3d4 -> 0x00a100b200c300d4
    bstrpick_d(tmp2, tmp1, 7, 0);
    srli_d(tmp1, tmp1, 8);
    bstrins_d(tmp2, tmp1, 23, 16);
    srli_d(tmp1, tmp1, 8);
    bstrins_d(tmp2, tmp1, 39, 32);
    srli_d(tmp1, tmp1, 8);
    bstrins_d(tmp2, tmp1, 55, 48);

    st_d(tmp2, dst, 0);
    addi_w(AT, AT, -1);
    addi_d(dst, dst, 8);
    addi_d(src, src, 4);
    b(Loop);

  bind(Once);
    beqz(len, Done);
    ld_wu(tmp1, src, 0);

    bstrpick_d(tmp2, tmp1, 7, 0);
    st_h(tmp2, dst, 0);
    addi_w(len, len, -1);

    beqz(len, Done);
    bstrpick_d(tmp2, tmp1, 15, 8);
    st_h(tmp2, dst, 2);
    addi_w(len, len, -1);

    beqz(len, Done);
    bstrpick_d(tmp2, tmp1, 23, 16);
    st_h(tmp2, dst, 4);

  bind(Done);
}

// Intrinsic for
//
// - java.lang.StringCoding::implEncodeISOArray
// - java.lang.StringCoding::implEncodeAsciiArray
//
// This version always returns the number of characters copied.
void MacroAssembler::encode_iso_array(Register src, Register dst,
                                      Register len, Register result,
                                      Register tmp1, Register tmp2,
                                      Register tmp3, bool ascii) {
  Label Loop, Done, Once;

  move(result, R0);                  // init in case of bad value
  bge(R0, len, Done);

  srai_w(AT, len, 2);

  li(tmp3, ascii ? 0xff80ff80ff80ff80 : 0xff00ff00ff00ff00);

  bind(Loop);
    beqz(AT, Once);
    ld_d(tmp1, src, 0);
    andr(tmp2, tmp3, tmp1);          // not latin-1, stop here
    bnez(tmp2, Once);

    // 0x00a100b200c300d4 -> 0x00000000a1b2c3d4
    srli_d(tmp2, tmp1, 8);
    orr(tmp2, tmp2, tmp1);           // 0x00a1a1b2b2c3c3d4
    bstrpick_d(tmp1, tmp2, 47, 32);  // 0x0000a1b2
    slli_d(tmp1, tmp1, 16);          // 0xa1b20000
    bstrins_d(tmp1, tmp2, 15, 0);    // 0xa1b2c3d4

    stx_w(tmp1, dst, result);
    addi_w(AT, AT, -1);
    addi_d(src, src, 8);
    addi_w(result, result, 4);
    b(Loop);

  bind(Once);
    beq(len, result, Done);
    ld_hu(tmp1, src, 0);
    andr(tmp2, tmp3, tmp1);          // not latin-1, stop here
    bnez(tmp2, Done);
    stx_b(tmp1, dst, result);
    addi_d(src, src, 2);
    addi_w(result, result, 1);
    b(Once);

  bind(Done);
}

// Code for BigInteger::mulAdd intrinsic
// out     = A0
// in      = A1
// offset  = A2  (already out.length-offset)
// len     = A3
// k       = A4
//
// pseudo code from java implementation:
// long kLong = k & LONG_MASK;
// carry = 0;
// offset = out.length-offset - 1;
// for (int j = len - 1; j >= 0; j--) {
//     product = (in[j] & LONG_MASK) * kLong + (out[offset] & LONG_MASK) + carry;
//     out[offset--] = (int)product;
//     carry = product >>> 32;
// }
// return (int)carry;
void MacroAssembler::mul_add(Register out, Register in, Register offset,
                             Register len, Register k) {
  Label L_tail_loop, L_unroll, L_end;

  move(SCR2, out);
  move(out, R0); // should clear out
  bge(R0, len, L_end);

  alsl_d(offset, offset, SCR2, LogBytesPerInt - 1);
  alsl_d(in, len, in, LogBytesPerInt - 1);

  const int unroll = 16;
  li(SCR2, unroll);
  blt(len, SCR2, L_tail_loop);

  bind(L_unroll);

    addi_d(in, in, -unroll * BytesPerInt);
    addi_d(offset, offset, -unroll * BytesPerInt);

    for (int i = unroll - 1; i >= 0; i--) {
      ld_wu(SCR1, in, i * BytesPerInt);
      mulw_d_wu(SCR1, SCR1, k);
      add_d(out, out, SCR1); // out as scratch
      ld_wu(SCR1, offset, i * BytesPerInt);
      add_d(SCR1, SCR1, out);
      st_w(SCR1, offset, i * BytesPerInt);
      srli_d(out, SCR1, 32); // keep carry
    }

    sub_w(len, len, SCR2);
    bge(len, SCR2, L_unroll);

  bge(R0, len, L_end); // check tail

  bind(L_tail_loop);

    addi_d(in, in, -BytesPerInt);
    ld_wu(SCR1, in, 0);
    mulw_d_wu(SCR1, SCR1, k);
    add_d(out, out, SCR1); // out as scratch

    addi_d(offset, offset, -BytesPerInt);
    ld_wu(SCR1, offset, 0);
    add_d(SCR1, SCR1, out);
    st_w(SCR1, offset, 0);

    srli_d(out, SCR1, 32); // keep carry

    addi_w(len, len, -1);
    blt(R0, len, L_tail_loop);

  bind(L_end);
}

#ifndef PRODUCT
void MacroAssembler::verify_cross_modify_fence_not_required() {
  if (VerifyCrossModifyFence) {
    // Check if thread needs a cross modify fence.
    ld_bu(SCR1, Address(TREG, in_bytes(JavaThread::requires_cross_modify_fence_offset())));
    Label fence_not_required;
    beqz(SCR1, fence_not_required);
    // If it does then fail.
    move(A0, TREG);
    call(CAST_FROM_FN_PTR(address, JavaThread::verify_cross_modify_fence_failure));
    bind(fence_not_required);
  }
}
#endif
