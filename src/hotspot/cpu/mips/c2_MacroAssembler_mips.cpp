/*
 * Copyright (c) 2020, Oracle and/or its affiliates. All rights reserved.
 * Copyright (c) 2021, Loongson Technology. All rights reserved.
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
#include "asm/assembler.hpp"
#include "asm/assembler.inline.hpp"
#include "opto/c2_MacroAssembler.hpp"
#include "opto/intrinsicnode.hpp"
#include "runtime/biasedLocking.hpp"
#include "runtime/objectMonitor.hpp"
#include "vmreg_mips.inline.hpp"

// Fast_Lock and Fast_Unlock used by C2

// Because the transitions from emitted code to the runtime
// monitorenter/exit helper stubs are so slow it's critical that
// we inline both the stack-locking fast-path and the inflated fast path.
//
// See also: cmpFastLock and cmpFastUnlock.
//
// What follows is a specialized inline transliteration of the code
// in slow_enter() and slow_exit().  If we're concerned about I$ bloat
// another option would be to emit TrySlowEnter and TrySlowExit methods
// at startup-time.  These methods would accept arguments as
// (Obj, Self, box, Scratch) and return success-failure
// indications in the icc.ZFlag.  Fast_Lock and Fast_Unlock would simply
// marshal the arguments and emit calls to TrySlowEnter and TrySlowExit.
// In practice, however, the # of lock sites is bounded and is usually small.
// Besides the call overhead, TrySlowEnter and TrySlowExit might suffer
// if the processor uses simple bimodal branch predictors keyed by EIP
// Since the helper routines would be called from multiple synchronization
// sites.
//
// An even better approach would be write "MonitorEnter()" and "MonitorExit()"
// in java - using j.u.c and unsafe - and just bind the lock and unlock sites
// to those specialized methods.  That'd give us a mostly platform-independent
// implementation that the JITs could optimize and inline at their pleasure.
// Done correctly, the only time we'd need to cross to native could would be
// to park() or unpark() threads.  We'd also need a few more unsafe operators
// to (a) prevent compiler-JIT reordering of non-volatile accesses, and
// (b) explicit barriers or fence operations.
//
// TODO:
//
// *  Arrange for C2 to pass "Self" into Fast_Lock and Fast_Unlock in one of the registers (scr).
//    This avoids manifesting the Self pointer in the Fast_Lock and Fast_Unlock terminals.
//    Given TLAB allocation, Self is usually manifested in a register, so passing it into
//    the lock operators would typically be faster than reifying Self.
//
// *  Ideally I'd define the primitives as:
//       fast_lock   (nax Obj, nax box, res, tmp, nax scr) where tmp and scr are KILLED.
//       fast_unlock (nax Obj, box, res, nax tmp) where tmp are KILLED
//    Unfortunately ADLC bugs prevent us from expressing the ideal form.
//    Instead, we're stuck with a rather awkward and brittle register assignments below.
//    Furthermore the register assignments are overconstrained, possibly resulting in
//    sub-optimal code near the synchronization site.
//
// *  Eliminate the sp-proximity tests and just use "== Self" tests instead.
//    Alternately, use a better sp-proximity test.
//
// *  Currently ObjectMonitor._Owner can hold either an sp value or a (THREAD *) value.
//    Either one is sufficient to uniquely identify a thread.
//    TODO: eliminate use of sp in _owner and use get_thread(tr) instead.
//
// *  Intrinsify notify() and notifyAll() for the common cases where the
//    object is locked by the calling thread but the waitlist is empty.
//    avoid the expensive JNI call to JVM_Notify() and JVM_NotifyAll().
//
// *  use jccb and jmpb instead of jcc and jmp to improve code density.
//    But beware of excessive branch density on AMD Opterons.
//
// *  Both Fast_Lock and Fast_Unlock set the ICC.ZF to indicate success
//    or failure of the fast-path.  If the fast-path fails then we pass
//    control to the slow-path, typically in C.  In Fast_Lock and
//    Fast_Unlock we often branch to DONE_LABEL, just to find that C2
//    will emit a conditional branch immediately after the node.
//    So we have branches to branches and lots of ICC.ZF games.
//    Instead, it might be better to have C2 pass a "FailureLabel"
//    into Fast_Lock and Fast_Unlock.  In the case of success, control
//    will drop through the node.  ICC.ZF is undefined at exit.
//    In the case of failure, the node will branch directly to the
//    FailureLabel

// obj: object to lock
// box: on-stack box address (displaced header location)
// tmp: tmp -- KILLED
// scr: tmp -- KILLED
void C2_MacroAssembler::fast_lock(Register objReg, Register boxReg, Register resReg,
                                  Register tmpReg, Register scrReg) {
  Label IsInflated, DONE, DONE_SET;

  // Ensure the register assignents are disjoint
  guarantee(objReg != boxReg, "");
  guarantee(objReg != tmpReg, "");
  guarantee(objReg != scrReg, "");
  guarantee(boxReg != tmpReg, "");
  guarantee(boxReg != scrReg, "");

  block_comment("FastLock");

  if (PrintBiasedLockingStatistics) {
    atomic_inc32((address)BiasedLocking::total_entry_count_addr(), 1, tmpReg, scrReg);
  }

  // Possible cases that we'll encounter in fast_lock
  // ------------------------------------------------
  // * Inflated
  //    -- unlocked
  //    -- Locked
  //       = by self
  //       = by other
  // * biased
  //    -- by Self
  //    -- by other
  // * neutral
  // * stack-locked
  //    -- by self
  //       = sp-proximity test hits
  //       = sp-proximity test generates false-negative
  //    -- by other
  //

  if (DiagnoseSyncOnValueBasedClasses != 0) {
    load_klass(tmpReg, objReg);
    lw(tmpReg, Address(tmpReg, Klass::access_flags_offset()));
    move(AT, JVM_ACC_IS_VALUE_BASED_CLASS);
    andr(AT, tmpReg, AT);
    sltiu(scrReg, AT, 1);
    beq(scrReg, R0, DONE_SET);
    delayed()->nop();
   }

  // TODO: optimize away redundant LDs of obj->mark and improve the markword triage
  // order to reduce the number of conditional branches in the most common cases.
  // Beware -- there's a subtle invariant that fetch of the markword
  // at [FETCH], below, will never observe a biased encoding (*101b).
  // If this invariant is not held we risk exclusion (safety) failure.
  if (UseBiasedLocking && !UseOptoBiasInlining) {
    Label succ, fail;
    biased_locking_enter(boxReg, objReg, tmpReg, scrReg, false, succ, NULL);
    b(fail);
    delayed()->nop();
    bind(succ);
    b(DONE);
    delayed()->ori(resReg, R0, 1);
    bind(fail);
  }

  ld(tmpReg, Address(objReg, 0)); //Fetch the markword of the object.
  andi(AT, tmpReg, markWord::monitor_value);
  bne(AT, R0, IsInflated); // inflated vs stack-locked|neutral|bias
  delayed()->nop();

  // Attempt stack-locking ...
  ori(tmpReg, tmpReg, markWord::unlocked_value);
  sd(tmpReg, Address(boxReg, 0)); // Anticipate successful CAS

  if (PrintBiasedLockingStatistics) {
    Label SUCC, FAIL;
    cmpxchg(Address(objReg, 0), tmpReg, boxReg, scrReg, true, false, SUCC, &FAIL); // Updates tmpReg
    bind(SUCC);
    atomic_inc32((address)BiasedLocking::fast_path_entry_count_addr(), 1, AT, scrReg);
    b(DONE);
    delayed()->ori(resReg, R0, 1);
    bind(FAIL);
  } else {
    // If cmpxchg is succ, then scrReg = 1
    cmpxchg(Address(objReg, 0), tmpReg, boxReg, scrReg, true, false, DONE_SET); // Updates tmpReg
  }

  // Recursive locking
  // The object is stack-locked: markword contains stack pointer to BasicLock.
  // Locked by current thread if difference with current SP is less than one page.
  dsubu(tmpReg, tmpReg, SP);
  li(AT, 7 - os::vm_page_size());
  andr(tmpReg, tmpReg, AT);
  sd(tmpReg, Address(boxReg, 0));

  if (PrintBiasedLockingStatistics) {
    Label L;
    // tmpReg == 0 => BiasedLocking::_fast_path_entry_count++
    bne(tmpReg, R0, L);
    delayed()->nop();
    atomic_inc32((address)BiasedLocking::fast_path_entry_count_addr(), 1, AT, scrReg);
    bind(L);
  }

  b(DONE);
  delayed()->sltiu(resReg, tmpReg, 1); // resReg = (tmpReg == 0) ? 1 : 0

  bind(IsInflated);
  // The object's monitor m is unlocked iff m->owner == NULL,
  // otherwise m->owner may contain a thread or a stack address.

  // TODO: someday avoid the ST-before-CAS penalty by
  // relocating (deferring) the following ST.
  // We should also think about trying a CAS without having
  // fetched _owner.  If the CAS is successful we may
  // avoid an RTO->RTS upgrade on the $line.
  // Without cast to int32_t a movptr will destroy r10 which is typically obj
  li(AT, (int32_t)intptr_t(markWord::unused_mark().value()));
  sd(AT, Address(boxReg, 0));

  ld(AT, Address(tmpReg, ObjectMonitor::owner_offset_in_bytes() - 2));
  // if (m->owner != 0) => AT = 0, goto slow path.
  bne(AT, R0, DONE_SET);
  delayed()->ori(scrReg, R0, 0);

#ifndef OPT_THREAD
  get_thread(TREG);
#endif
  // It's inflated and appears unlocked
  cmpxchg(Address(tmpReg, ObjectMonitor::owner_offset_in_bytes() - 2), R0, TREG, scrReg, false, false) ;
  // Intentional fall-through into DONE ...

  bind(DONE_SET);
  move(resReg, scrReg);

  // DONE is a hot target - we'd really like to place it at the
  // start of cache line by padding with NOPs.
  // See the AMD and Intel software optimization manuals for the
  // most efficient "long" NOP encodings.
  // Unfortunately none of our alignment mechanisms suffice.
  bind(DONE);
  // At DONE the resReg is set as follows ...
  // Fast_Unlock uses the same protocol.
  // resReg == 1 -> Success
  // resREg == 0 -> Failure - force control through the slow-path
}

// obj: object to unlock
// box: box address (displaced header location), killed.
// tmp: killed tmp; cannot be obj nor box.
//
// Some commentary on balanced locking:
//
// Fast_Lock and Fast_Unlock are emitted only for provably balanced lock sites.
// Methods that don't have provably balanced locking are forced to run in the
// interpreter - such methods won't be compiled to use fast_lock and fast_unlock.
// The interpreter provides two properties:
// I1:  At return-time the interpreter automatically and quietly unlocks any
//      objects acquired the current activation (frame).  Recall that the
//      interpreter maintains an on-stack list of locks currently held by
//      a frame.
// I2:  If a method attempts to unlock an object that is not held by the
//      the frame the interpreter throws IMSX.
//
// Lets say A(), which has provably balanced locking, acquires O and then calls B().
// B() doesn't have provably balanced locking so it runs in the interpreter.
// Control returns to A() and A() unlocks O.  By I1 and I2, above, we know that O
// is still locked by A().
//
// The only other source of unbalanced locking would be JNI.  The "Java Native Interface:
// Programmer's Guide and Specification" claims that an object locked by jni_monitorenter
// should not be unlocked by "normal" java-level locking and vice-versa.  The specification
// doesn't specify what will occur if a program engages in such mixed-mode locking, however.

void C2_MacroAssembler::fast_unlock(Register objReg, Register boxReg, Register resReg,
                                    Register tmpReg, Register scrReg) {
  Label DONE, DONE_SET, Stacked, Inflated;

  guarantee(objReg != boxReg, "");
  guarantee(objReg != tmpReg, "");
  guarantee(objReg != scrReg, "");
  guarantee(boxReg != tmpReg, "");
  guarantee(boxReg != scrReg, "");

  block_comment("FastUnlock");

  // Critically, the biased locking test must have precedence over
  // and appear before the (box->dhw == 0) recursive stack-lock test.
  if (UseBiasedLocking && !UseOptoBiasInlining) {
    Label succ, fail;
    biased_locking_exit(objReg, tmpReg, succ);
    b(fail);
    delayed()->nop();
    bind(succ);
    b(DONE);
    delayed()->ori(resReg, R0, 1);
    bind(fail);
  }

  ld(tmpReg, Address(boxReg, 0)); // Examine the displaced header
  beq(tmpReg, R0, DONE_SET); // 0 indicates recursive stack-lock
  delayed()->sltiu(AT, tmpReg, 1);

  ld(tmpReg, Address(objReg, 0)); // Examine the object's markword
  andi(AT, tmpReg, markWord::monitor_value);
  beq(AT, R0, Stacked); // Inflated?
  delayed()->nop();

  bind(Inflated);
  // It's inflated.
  // Despite our balanced locking property we still check that m->_owner == Self
  // as java routines or native JNI code called by this thread might
  // have released the lock.
  // Refer to the comments in synchronizer.cpp for how we might encode extra
  // state in _succ so we can avoid fetching EntryList|cxq.
  //
  // I'd like to add more cases in fast_lock() and fast_unlock() --
  // such as recursive enter and exit -- but we have to be wary of
  // I$ bloat, T$ effects and BP$ effects.
  //
  // If there's no contention try a 1-0 exit.  That is, exit without
  // a costly MEMBAR or CAS.  See synchronizer.cpp for details on how
  // we detect and recover from the race that the 1-0 exit admits.
  //
  // Conceptually Fast_Unlock() must execute a STST|LDST "release" barrier
  // before it STs null into _owner, releasing the lock.  Updates
  // to data protected by the critical section must be visible before
  // we drop the lock (and thus before any other thread could acquire
  // the lock and observe the fields protected by the lock).
#ifndef OPT_THREAD
  get_thread(TREG);
#endif

  // It's inflated
  ld(scrReg, Address(tmpReg, ObjectMonitor::owner_offset_in_bytes() - 2)) ;
  xorr(scrReg, scrReg, TREG);

  ld(AT, Address(tmpReg, ObjectMonitor::recursions_offset_in_bytes() - 2)) ;
  orr(scrReg, scrReg, AT);

  bne(scrReg, R0, DONE_SET);
  delayed()->ori(AT, R0, 0);

  ld(scrReg, Address(tmpReg, ObjectMonitor::cxq_offset_in_bytes() - 2));
  ld(AT, Address(tmpReg, ObjectMonitor::EntryList_offset_in_bytes() - 2));
  orr(scrReg, scrReg, AT);

  bne(scrReg, R0, DONE_SET);
  delayed()->ori(AT, R0, 0);

  sync();
  sd(R0, Address(tmpReg, ObjectMonitor::owner_offset_in_bytes() - 2));
  b(DONE);
  delayed()->ori(resReg, R0, 1);

  bind(Stacked);
  ld(tmpReg, Address(boxReg, 0));
  cmpxchg(Address(objReg, 0), boxReg, tmpReg, AT, false, false);

  bind(DONE_SET);
  move(resReg, AT);

  bind(DONE);
}

void C2_MacroAssembler::beq_long(Register rs, Register rt, Label& L) {
  Label not_taken;

  bne(rs, rt, not_taken);
  delayed()->nop();

  jmp_far(L);

  bind(not_taken);
}

void C2_MacroAssembler::bne_long(Register rs, Register rt, Label& L) {
  Label not_taken;

  beq(rs, rt, not_taken);
  delayed()->nop();

  jmp_far(L);

  bind(not_taken);
}

void C2_MacroAssembler::bc1t_long(Label& L) {
  Label not_taken;

  bc1f(not_taken);
  delayed()->nop();

  jmp_far(L);

  bind(not_taken);
}

void C2_MacroAssembler::bc1f_long(Label& L) {
  Label not_taken;

  bc1t(not_taken);
  delayed()->nop();

  jmp_far(L);

  bind(not_taken);
}

// Compare strings, used for char[] and byte[].
void C2_MacroAssembler::string_compare(Register str1, Register str2,
                                    Register cnt1, Register cnt2, Register result,
                                    int ae) {
  Label L, Loop, haveResult, done;

  bool isLL = ae == StrIntrinsicNode::LL;
  bool isLU = ae == StrIntrinsicNode::LU;
  bool isUL = ae == StrIntrinsicNode::UL;

  bool str1_isL = isLL || isLU;
  bool str2_isL = isLL || isUL;

  if (!str1_isL) srl(cnt1, cnt1, 1);
  if (!str2_isL) srl(cnt2, cnt2, 1);

  // compute the and difference of lengths (in result)
  subu(result, cnt1, cnt2); // result holds the difference of two lengths

  // compute the shorter length (in cnt1)
  slt(AT, cnt2, cnt1);
  movn(cnt1, cnt2, AT);

  // Now the shorter length is in cnt1 and cnt2 can be used as a tmp register
  bind(Loop);                        // Loop begin
  beq(cnt1, R0, done);
  if (str1_isL) {
    delayed()->lbu(AT, str1, 0);
  } else {
    delayed()->lhu(AT, str1, 0);
  }

  // compare current character
  if (str2_isL) {
    lbu(cnt2, str2, 0);
  } else {
    lhu(cnt2, str2, 0);
  }
  bne(AT, cnt2, haveResult);
  delayed()->addiu(str1, str1, str1_isL ? 1 : 2);
  addiu(str2, str2, str2_isL ? 1 : 2);
  b(Loop);
  delayed()->addiu(cnt1, cnt1, -1);   // Loop end

  bind(haveResult);
  subu(result, AT, cnt2);

  bind(done);
}

// Compare char[] or byte[] arrays or substrings.
void C2_MacroAssembler::arrays_equals(Register str1, Register str2,
                                   Register cnt, Register tmp, Register result,
                                   bool is_char) {
  Label Loop, True, False;

  beq(str1, str2, True);  // same char[] ?
  delayed()->daddiu(result, R0, 1);

  beq(cnt, R0, True);
  delayed()->nop(); // count == 0

  bind(Loop);

  // compare current character
  if (is_char) {
    lhu(AT, str1, 0);
    lhu(tmp, str2, 0);
  } else {
    lbu(AT, str1, 0);
    lbu(tmp, str2, 0);
  }
  bne(AT, tmp, False);
  delayed()->addiu(str1, str1, is_char ? 2 : 1);
  addiu(cnt, cnt, -1);
  bne(cnt, R0, Loop);
  delayed()->addiu(str2, str2, is_char ? 2 : 1);

  b(True);
  delayed()->nop();

  bind(False);
  daddiu(result, R0, 0);

  bind(True);
}

void C2_MacroAssembler::gs_loadstore(Register reg, Register base, Register index, int disp, int type) {
  switch (type) {
    case STORE_BYTE:
      gssbx(reg, base, index, disp);
      break;
    case STORE_CHAR:
    case STORE_SHORT:
      gsshx(reg, base, index, disp);
      break;
    case STORE_INT:
      gsswx(reg, base, index, disp);
      break;
    case STORE_LONG:
      gssdx(reg, base, index, disp);
      break;
    case LOAD_BYTE:
      gslbx(reg, base, index, disp);
      break;
    case LOAD_SHORT:
      gslhx(reg, base, index, disp);
      break;
    case LOAD_INT:
      gslwx(reg, base, index, disp);
      break;
    case LOAD_LONG:
      gsldx(reg, base, index, disp);
      break;
    default:
      ShouldNotReachHere();
  }
}

void C2_MacroAssembler::gs_loadstore(FloatRegister reg, Register base, Register index, int disp, int type) {
  switch (type) {
    case STORE_FLOAT:
      gsswxc1(reg, base, index, disp);
      break;
    case STORE_DOUBLE:
      gssdxc1(reg, base, index, disp);
      break;
    case LOAD_FLOAT:
      gslwxc1(reg, base, index, disp);
      break;
    case LOAD_DOUBLE:
      gsldxc1(reg, base, index, disp);
      break;
    default:
      ShouldNotReachHere();
  }
}

void C2_MacroAssembler::loadstore(Register reg, Register base, int disp, int type) {
  switch (type) {
    case STORE_BYTE:
      sb(reg, base, disp);
      break;
    case STORE_CHAR:
    case STORE_SHORT:
      sh(reg, base, disp);
      break;
    case STORE_INT:
      sw(reg, base, disp);
      break;
    case STORE_LONG:
      sd(reg, base, disp);
      break;
    case LOAD_BYTE:
      lb(reg, base, disp);
      break;
    case LOAD_U_BYTE:
      lbu(reg, base, disp);
      break;
    case LOAD_SHORT:
      lh(reg, base, disp);
      break;
    case LOAD_U_SHORT:
      lhu(reg, base, disp);
      break;
    case LOAD_INT:
      lw(reg, base, disp);
      break;
    case LOAD_U_INT:
      lwu(reg, base, disp);
      break;
    case LOAD_LONG:
      ld(reg, base, disp);
      break;
    case LOAD_LINKED_LONG:
      lld(reg, base, disp);
      break;
     default:
       ShouldNotReachHere();
    }
}

void C2_MacroAssembler::loadstore(FloatRegister reg, Register base, int disp, int type) {
  switch (type) {
    case STORE_FLOAT:
      swc1(reg, base, disp);
      break;
    case STORE_DOUBLE:
      sdc1(reg, base, disp);
      break;
    case LOAD_FLOAT:
      lwc1(reg, base, disp);
      break;
    case LOAD_DOUBLE:
      ldc1(reg, base, disp);
      break;
     default:
       ShouldNotReachHere();
    }
}
