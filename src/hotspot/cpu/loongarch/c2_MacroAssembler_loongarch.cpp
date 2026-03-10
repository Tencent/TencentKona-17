/*
 * Copyright (c) 2020, Oracle and/or its affiliates. All rights reserved.
 * Copyright (c) 2021, 2023, Loongson Technology. All rights reserved.
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
#include "opto/subnode.hpp"
#include "runtime/biasedLocking.hpp"
#include "runtime/objectMonitor.hpp"
#include "runtime/stubRoutines.hpp"

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
    ld_w(tmpReg, Address(tmpReg, Klass::access_flags_offset()));
    li(AT, JVM_ACC_IS_VALUE_BASED_CLASS);
    andr(AT, tmpReg, AT);
    sltui(scrReg, AT, 1);
    beqz(scrReg, DONE_SET);
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
    bind(succ);
    li(resReg, 1);
    b(DONE);
    bind(fail);
  }

  ld_d(tmpReg, Address(objReg, 0)); //Fetch the markword of the object.
  andi(AT, tmpReg, markWord::monitor_value);
  bnez(AT, IsInflated); // inflated vs stack-locked|neutral|bias

  // Attempt stack-locking ...
  ori(tmpReg, tmpReg, markWord::unlocked_value);
  st_d(tmpReg, Address(boxReg, 0)); // Anticipate successful CAS

  if (PrintBiasedLockingStatistics) {
    Label SUCC, FAIL;
    cmpxchg(Address(objReg, 0), tmpReg, boxReg, scrReg, true, true /* acquire */, SUCC, &FAIL); // Updates tmpReg
    bind(SUCC);
    atomic_inc32((address)BiasedLocking::fast_path_entry_count_addr(), 1, AT, scrReg);
    li(resReg, 1);
    b(DONE);
    bind(FAIL);
  } else {
    // If cmpxchg is succ, then scrReg = 1
    cmpxchg(Address(objReg, 0), tmpReg, boxReg, scrReg, true, true /* acquire */, DONE_SET); // Updates tmpReg
  }

  // Recursive locking
  // The object is stack-locked: markword contains stack pointer to BasicLock.
  // Locked by current thread if difference with current SP is less than one page.
  sub_d(tmpReg, tmpReg, SP);
  li(AT, 7 - os::vm_page_size());
  andr(tmpReg, tmpReg, AT);
  st_d(tmpReg, Address(boxReg, 0));

  if (PrintBiasedLockingStatistics) {
    Label L;
    // tmpReg == 0 => BiasedLocking::_fast_path_entry_count++
    bnez(tmpReg, L);
    atomic_inc32((address)BiasedLocking::fast_path_entry_count_addr(), 1, AT, scrReg);
    bind(L);
  }

  sltui(resReg, tmpReg, 1); // resReg = (tmpReg == 0) ? 1 : 0
  b(DONE);

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
  st_d(AT, Address(boxReg, 0));

  ld_d(AT, Address(tmpReg, ObjectMonitor::owner_offset_in_bytes() - 2));
  // if (m->owner != 0) => AT = 0, goto slow path.
  move(scrReg, R0);
  bnez(AT, DONE_SET);

#ifndef OPT_THREAD
  get_thread(TREG);
#endif
  // It's inflated and appears unlocked
  addi_d(tmpReg, tmpReg, ObjectMonitor::owner_offset_in_bytes() - 2);
  cmpxchg(Address(tmpReg, 0), R0, TREG, scrReg, false, true /* acquire */);
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
    bind(succ);
    li(resReg, 1);
    b(DONE);
    bind(fail);
  }

  ld_d(tmpReg, Address(boxReg, 0)); // Examine the displaced header
  sltui(AT, tmpReg, 1);
  beqz(tmpReg, DONE_SET); // 0 indicates recursive stack-lock

  ld_d(tmpReg, Address(objReg, 0)); // Examine the object's markword
  andi(AT, tmpReg, markWord::monitor_value);
  beqz(AT, Stacked); // Inflated?

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
  ld_d(scrReg, Address(tmpReg, ObjectMonitor::owner_offset_in_bytes() - 2));
  xorr(scrReg, scrReg, TREG);

  ld_d(AT, Address(tmpReg, ObjectMonitor::recursions_offset_in_bytes() - 2));
  orr(scrReg, scrReg, AT);

  move(AT, R0);
  bnez(scrReg, DONE_SET);

  ld_d(scrReg, Address(tmpReg, ObjectMonitor::cxq_offset_in_bytes() - 2));
  ld_d(AT, Address(tmpReg, ObjectMonitor::EntryList_offset_in_bytes() - 2));
  orr(scrReg, scrReg, AT);

  move(AT, R0);
  bnez(scrReg, DONE_SET);

  membar(Assembler::Membar_mask_bits(LoadStore|StoreStore));
  st_d(R0, Address(tmpReg, ObjectMonitor::owner_offset_in_bytes() - 2));
  li(resReg, 1);
  b(DONE);

  bind(Stacked);
  ld_d(tmpReg, Address(boxReg, 0));
  cmpxchg(Address(objReg, 0), boxReg, tmpReg, AT, false, true /* acquire */);

  bind(DONE_SET);
  move(resReg, AT);

  bind(DONE);
}

void C2_MacroAssembler::beq_long(Register rs, Register rt, Label& L) {
  Label not_taken;

  bne(rs, rt, not_taken);

  jmp_far(L);

  bind(not_taken);
}

void C2_MacroAssembler::bne_long(Register rs, Register rt, Label& L) {
  Label not_taken;

  beq(rs, rt, not_taken);

  jmp_far(L);

  bind(not_taken);
}

void C2_MacroAssembler::blt_long(Register rs, Register rt, Label& L, bool is_signed) {
  Label not_taken;
  if (is_signed) {
    bge(rs, rt, not_taken);
  } else {
    bgeu(rs, rt, not_taken);
  }
  jmp_far(L);
  bind(not_taken);
}

void C2_MacroAssembler::bge_long(Register rs, Register rt, Label& L, bool is_signed) {
  Label not_taken;
  if (is_signed) {
    blt(rs, rt, not_taken);
  } else {
    bltu(rs, rt, not_taken);
  }
  jmp_far(L);
  bind(not_taken);
}

void C2_MacroAssembler::bc1t_long(Label& L) {
  Label not_taken;

  bceqz(FCC0, not_taken);

  jmp_far(L);

  bind(not_taken);
}

void C2_MacroAssembler::bc1f_long(Label& L) {
  Label not_taken;

  bcnez(FCC0, not_taken);

  jmp_far(L);

  bind(not_taken);
}

typedef void (MacroAssembler::* load_chr_insn)(Register rd, Address adr);

void C2_MacroAssembler::string_indexof(Register haystack, Register needle,
                                       Register haystack_len, Register needle_len,
                                       Register result, int ae)
{
  assert(ae != StrIntrinsicNode::LU, "Invalid encoding");

  Label LINEARSEARCH, LINEARSTUB, DONE, NOMATCH;

  bool isLL = ae == StrIntrinsicNode::LL;

  bool needle_isL = ae == StrIntrinsicNode::LL || ae == StrIntrinsicNode::UL;
  bool haystack_isL = ae == StrIntrinsicNode::LL || ae == StrIntrinsicNode::LU;

  int needle_chr_size = needle_isL ? 1 : 2;
  int haystack_chr_size = haystack_isL ? 1 : 2;

  Address::ScaleFactor needle_chr_shift = needle_isL ? Address::no_scale
                                                     : Address::times_2;
  Address::ScaleFactor haystack_chr_shift = haystack_isL ? Address::no_scale
                                                         : Address::times_2;

  load_chr_insn needle_load_1chr = needle_isL ? (load_chr_insn)&MacroAssembler::ld_bu
                                              : (load_chr_insn)&MacroAssembler::ld_hu;
  load_chr_insn haystack_load_1chr = haystack_isL ? (load_chr_insn)&MacroAssembler::ld_bu
                                                  : (load_chr_insn)&MacroAssembler::ld_hu;

  // Note, inline_string_indexOf() generates checks:
  // if (pattern.count > src.count) return -1;
  // if (pattern.count == 0) return 0;

  // We have two strings, a source string in haystack, haystack_len and a pattern string
  // in needle, needle_len. Find the first occurrence of pattern in source or return -1.

  // For larger pattern and source we use a simplified Boyer Moore algorithm.
  // With a small pattern and source we use linear scan.

  // needle_len >= 8 && needle_len < 256 && needle_len < haystack_len/4, use bmh algorithm.

  // needle_len < 8, use linear scan
  li(AT, 8);
  blt(needle_len, AT, LINEARSEARCH);

  // needle_len >= 256, use linear scan
  li(AT, 256);
  bge(needle_len, AT, LINEARSTUB);

  // needle_len >= haystack_len/4, use linear scan
  srli_d(AT, haystack_len, 2);
  bge(needle_len, AT, LINEARSTUB);

  // Boyer-Moore-Horspool introduction:
  // The Boyer Moore alogorithm is based on the description here:-
  //
  // http://en.wikipedia.org/wiki/Boyer%E2%80%93Moore_string_search_algorithm
  //
  // This describes and algorithm with 2 shift rules. The 'Bad Character' rule
  // and the 'Good Suffix' rule.
  //
  // These rules are essentially heuristics for how far we can shift the
  // pattern along the search string.
  //
  // The implementation here uses the 'Bad Character' rule only because of the
  // complexity of initialisation for the 'Good Suffix' rule.
  //
  // This is also known as the Boyer-Moore-Horspool algorithm:
  //
  // http://en.wikipedia.org/wiki/Boyer-Moore-Horspool_algorithm
  //
  // #define ASIZE 256
  //
  //    int bm(unsigned char *pattern, int m, unsigned char *src, int n) {
  //      int i, j;
  //      unsigned c;
  //      unsigned char bc[ASIZE];
  //
  //      /* Preprocessing */
  //      for (i = 0; i < ASIZE; ++i)
  //        bc[i] = m;
  //      for (i = 0; i < m - 1; ) {
  //        c = pattern[i];
  //        ++i;
  //        // c < 256 for Latin1 string, so, no need for branch
  //        #ifdef PATTERN_STRING_IS_LATIN1
  //        bc[c] = m - i;
  //        #else
  //        if (c < ASIZE) bc[c] = m - i;
  //        #endif
  //      }
  //
  //      /* Searching */
  //      j = 0;
  //      while (j <= n - m) {
  //        c = src[i+j];
  //        if (pattern[m-1] == c)
  //          int k;
  //          for (k = m - 2; k >= 0 && pattern[k] == src[k + j]; --k);
  //          if (k < 0) return j;
  //          // c < 256 for Latin1 string, so, no need for branch
  //          #ifdef SOURCE_STRING_IS_LATIN1_AND_PATTERN_STRING_IS_LATIN1
  //          // LL case: (c< 256) always true. Remove branch
  //          j += bc[pattern[j+m-1]];
  //          #endif
  //          #ifdef SOURCE_STRING_IS_UTF_AND_PATTERN_STRING_IS_UTF
  //          // UU case: need if (c<ASIZE) check. Skip 1 character if not.
  //          if (c < ASIZE)
  //            j += bc[pattern[j+m-1]];
  //          else
  //            j += 1
  //          #endif
  //          #ifdef SOURCE_IS_UTF_AND_PATTERN_IS_LATIN1
  //          // UL case: need if (c<ASIZE) check. Skip <pattern length> if not.
  //          if (c < ASIZE)
  //            j += bc[pattern[j+m-1]];
  //          else
  //            j += m
  //          #endif
  //      }
  //      return -1;
  //    }

  Label BCLOOP, BCSKIP, BMLOOPSTR2, BMLOOPSTR1, BMSKIP, BMADV, BMMATCH,
        BMLOOPSTR1_LASTCMP, BMLOOPSTR1_CMP, BMLOOPSTR1_AFTER_LOAD;

  Register haystack_end = haystack_len;
  Register result_tmp = result;

  Register nlen_tmp = T0; // needle len tmp
  Register skipch = T1;
  Register last_byte = T2;
  Register last_dword = T3;
  Register orig_haystack = T4;
  Register ch1 = T5;
  Register ch2 = T6;

  RegSet spilled_regs = RegSet::range(T0, T6);

  push(spilled_regs);

  // pattern length is >=8, so, we can read at least 1 register for cases when
  // UTF->Latin1 conversion is not needed(8 LL or 4UU) and half register for
  // UL case. We'll re-read last character in inner pre-loop code to have
  // single outer pre-loop load
  const int first_step = isLL ? 7 : 3;

  const int ASIZE = 256;

  addi_d(SP, SP, -ASIZE);

  // init BC offset table with default value: needle_len
  //
  // for (i = 0; i < ASIZE; ++i)
  //   bc[i] = m;
  if (UseLASX) {
    xvreplgr2vr_b(fscratch, needle_len);

    for (int i = 0; i < ASIZE; i += 32) {
      xvst(fscratch, SP, i);
    }
  } else if (UseLSX) {
    vreplgr2vr_b(fscratch, needle_len);

    for (int i = 0; i < ASIZE; i += 16) {
      vst(fscratch, SP, i);
    }
  } else {
    move(AT, needle_len);
    bstrins_d(AT, AT, 15, 8);
    bstrins_d(AT, AT, 31, 16);
    bstrins_d(AT, AT, 63, 32);

    for (int i = 0; i < ASIZE; i += 8) {
      st_d(AT, SP, i);
    }
  }

  sub_d(nlen_tmp, haystack_len, needle_len);
  lea(haystack_end, Address(haystack, nlen_tmp, haystack_chr_shift, 0));
  addi_d(ch2, needle_len, -1); // bc offset init value
  move(nlen_tmp, needle);

  //  for (i = 0; i < m - 1; ) {
  //    c = pattern[i];
  //    ++i;
  //    // c < 256 for Latin1 string, so, no need for branch
  //    #ifdef PATTERN_STRING_IS_LATIN1
  //    bc[c] = m - i;
  //    #else
  //    if (c < ASIZE) bc[c] = m - i;
  //    #endif
  //  }
  bind(BCLOOP);
  (this->*needle_load_1chr)(ch1, Address(nlen_tmp));
  addi_d(nlen_tmp, nlen_tmp, needle_chr_size);
  if (!needle_isL) {
    // ae == StrIntrinsicNode::UU
    li(AT, 256u);
    bgeu(ch1, AT, BCSKIP); // GE for UTF
  }
  stx_b(ch2, SP, ch1); // store skip offset to BC offset table

  bind(BCSKIP);
  addi_d(ch2, ch2, -1); // for next pattern element, skip distance -1
  blt(R0, ch2, BCLOOP);

  if (needle_isL == haystack_isL) {
    // load last 8 pattern bytes (8LL/4UU symbols)
    ld_d(last_dword, Address(needle, needle_len, needle_chr_shift, -wordSize));
    addi_d(nlen_tmp, needle_len, -1); // m - 1, index of the last element in pattern
    move(orig_haystack, haystack);
    bstrpick_d(last_byte, last_dword, 63, 64 - 8 * needle_chr_size); // UU/LL: pattern[m-1]
  } else {
    // UL: from UTF-16(source) search Latin1(pattern)
    // load last 4 bytes(4 symbols)
    ld_wu(last_byte, Address(needle, needle_len, Address::no_scale, -wordSize / 2));
    addi_d(nlen_tmp, needle_len, -1); // m - 1, index of the last element in pattern
    move(orig_haystack, haystack);
    // convert Latin1 to UTF. eg: 0x0000abcd -> 0x0a0b0c0d
    bstrpick_d(last_dword, last_byte, 7, 0);
    srli_d(last_byte, last_byte, 8);
    bstrins_d(last_dword, last_byte, 23, 16);
    srli_d(last_byte, last_byte, 8);
    bstrins_d(last_dword, last_byte, 39, 32);
    srli_d(last_byte, last_byte, 8); // last_byte: 0x0000000a
    bstrins_d(last_dword, last_byte, 55, 48); // last_dword: 0x0a0b0c0d
  }

  // i = m - 1;
  // skipch = j + i;
  // if (skipch == pattern[m - 1]
  //   for (k = m - 2; k >= 0 && pattern[k] == src[k + j]; --k);
  // else
  //   move j with bad char offset table
  bind(BMLOOPSTR2);
  // compare pattern to source string backward
  (this->*haystack_load_1chr)(skipch, Address(haystack, nlen_tmp, haystack_chr_shift, 0));
  addi_d(nlen_tmp, nlen_tmp, -first_step); // nlen_tmp is positive here, because needle_len >= 8
  bne(last_byte, skipch, BMSKIP); // if not equal, skipch is bad char
  ld_d(ch2, Address(haystack, nlen_tmp, haystack_chr_shift, 0)); // load 8 bytes from source string
  move(ch1, last_dword);
  if (isLL) {
    b(BMLOOPSTR1_AFTER_LOAD);
  } else {
    addi_d(nlen_tmp, nlen_tmp, -1); // no need to branch for UU/UL case. cnt1 >= 8
    b(BMLOOPSTR1_CMP);
  }

  bind(BMLOOPSTR1);
  (this->*needle_load_1chr)(ch1, Address(needle, nlen_tmp, needle_chr_shift, 0));
  (this->*haystack_load_1chr)(ch2, Address(haystack, nlen_tmp, haystack_chr_shift, 0));

  bind(BMLOOPSTR1_AFTER_LOAD);
  addi_d(nlen_tmp, nlen_tmp, -1);
  blt(nlen_tmp, R0, BMLOOPSTR1_LASTCMP);

  bind(BMLOOPSTR1_CMP);
  beq(ch1, ch2, BMLOOPSTR1);

  bind(BMSKIP);
  if (!isLL) {
    // if we've met UTF symbol while searching Latin1 pattern, then we can
    // skip needle_len symbols
    if (needle_isL != haystack_isL) {
      move(result_tmp, needle_len);
    } else {
      li(result_tmp, 1);
    }
    li(AT, 256u);
    bgeu(skipch, AT, BMADV); // GE for UTF
  }
  ldx_bu(result_tmp, SP, skipch); // load skip offset

  bind(BMADV);
  addi_d(nlen_tmp, needle_len, -1);
  // move haystack after bad char skip offset
  lea(haystack, Address(haystack, result_tmp, haystack_chr_shift, 0));
  bge(haystack_end, haystack, BMLOOPSTR2);
  addi_d(SP, SP, ASIZE);
  b(NOMATCH);

  bind(BMLOOPSTR1_LASTCMP);
  bne(ch1, ch2, BMSKIP);

  bind(BMMATCH);
  sub_d(result, haystack, orig_haystack);
  if (!haystack_isL) {
    srli_d(result, result, 1);
  }
  addi_d(SP, SP, ASIZE);
  pop(spilled_regs);
  b(DONE);

  bind(LINEARSTUB);
  li(AT, 16); // small patterns still should be handled by simple algorithm
  blt(needle_len, AT, LINEARSEARCH);
  move(result, R0);
  address stub;
  if (isLL) {
    stub = StubRoutines::la::string_indexof_linear_ll();
    assert(stub != NULL, "string_indexof_linear_ll stub has not been generated");
  } else if (needle_isL) {
    stub = StubRoutines::la::string_indexof_linear_ul();
    assert(stub != NULL, "string_indexof_linear_ul stub has not been generated");
  } else {
    stub = StubRoutines::la::string_indexof_linear_uu();
    assert(stub != NULL, "string_indexof_linear_uu stub has not been generated");
  }
  trampoline_call(RuntimeAddress(stub));
  b(DONE);

  bind(NOMATCH);
  li(result, -1);
  pop(spilled_regs);
  b(DONE);

  bind(LINEARSEARCH);
  string_indexof_linearscan(haystack, needle, haystack_len, needle_len, -1, result, ae);

  bind(DONE);
}

void C2_MacroAssembler::string_indexof_linearscan(Register haystack, Register needle,
                                                  Register haystack_len, Register needle_len,
                                                  int needle_con_cnt, Register result, int ae)
{
  // Note:
  // needle_con_cnt > 0 means needle_len register is invalid, needle length is constant
  // for UU/LL: needle_con_cnt[1, 4], UL: needle_con_cnt = 1
  assert(needle_con_cnt <= 4, "Invalid needle constant count");
  assert(ae != StrIntrinsicNode::LU, "Invalid encoding");

  Register hlen_neg = haystack_len;
  Register nlen_neg = needle_len;
  Register result_tmp = result;

  Register nlen_tmp = A0, hlen_tmp = A1;
  Register first = A2, ch1 = A3, ch2 = AT;

  RegSet spilled_regs = RegSet::range(A0, A3);

  push(spilled_regs);

  bool isLL = ae == StrIntrinsicNode::LL;

  bool needle_isL = ae == StrIntrinsicNode::LL || ae == StrIntrinsicNode::UL;
  bool haystack_isL = ae == StrIntrinsicNode::LL || ae == StrIntrinsicNode::LU;
  int needle_chr_shift = needle_isL ? 0 : 1;
  int haystack_chr_shift = haystack_isL ? 0 : 1;
  int needle_chr_size = needle_isL ? 1 : 2;
  int haystack_chr_size = haystack_isL ? 1 : 2;

  load_chr_insn needle_load_1chr = needle_isL ? (load_chr_insn)&MacroAssembler::ld_bu
                                              : (load_chr_insn)&MacroAssembler::ld_hu;
  load_chr_insn haystack_load_1chr = haystack_isL ? (load_chr_insn)&MacroAssembler::ld_bu
                                                  : (load_chr_insn)&MacroAssembler::ld_hu;
  load_chr_insn load_2chr = isLL ? (load_chr_insn)&MacroAssembler::ld_hu
                                 : (load_chr_insn)&MacroAssembler::ld_wu;
  load_chr_insn load_4chr = isLL ? (load_chr_insn)&MacroAssembler::ld_wu
                                 : (load_chr_insn)&MacroAssembler::ld_d;

  Label DO1, DO2, DO3, MATCH, NOMATCH, DONE;

  if (needle_con_cnt == -1) {
    Label DOSHORT, FIRST_LOOP, STR2_NEXT, STR1_LOOP, STR1_NEXT;

    li(AT, needle_isL == haystack_isL ? 4 : 2); // UU/LL:4, UL:2
    blt(needle_len, AT, DOSHORT);

    sub_d(result_tmp, haystack_len, needle_len);

    (this->*needle_load_1chr)(first, Address(needle));
    if (!haystack_isL) slli_d(result_tmp, result_tmp, haystack_chr_shift);
    add_d(haystack, haystack, result_tmp);
    sub_d(hlen_neg, R0, result_tmp);
    if (!needle_isL) slli_d(needle_len, needle_len, needle_chr_shift);
    add_d(needle, needle, needle_len);
    sub_d(nlen_neg, R0, needle_len);

    bind(FIRST_LOOP);
    (this->*haystack_load_1chr)(ch2, Address(haystack, hlen_neg, Address::no_scale, 0));
    beq(first, ch2, STR1_LOOP);

    bind(STR2_NEXT);
    addi_d(hlen_neg, hlen_neg, haystack_chr_size);
    bge(R0, hlen_neg, FIRST_LOOP);
    b(NOMATCH);

    bind(STR1_LOOP);
    addi_d(nlen_tmp, nlen_neg, needle_chr_size);
    addi_d(hlen_tmp, hlen_neg, haystack_chr_size);
    bge(nlen_tmp, R0, MATCH);

    bind(STR1_NEXT);
    (this->*needle_load_1chr)(ch1, Address(needle, nlen_tmp, Address::no_scale, 0));
    (this->*haystack_load_1chr)(ch2, Address(haystack, hlen_tmp, Address::no_scale, 0));
    bne(ch1, ch2, STR2_NEXT);
    addi_d(nlen_tmp, nlen_tmp, needle_chr_size);
    addi_d(hlen_tmp, hlen_tmp, haystack_chr_size);
    blt(nlen_tmp, R0, STR1_NEXT);
    b(MATCH);

    bind(DOSHORT);
    if (needle_isL == haystack_isL) {
      li(AT, 2);
      blt(needle_len, AT, DO1); // needle_len == 1
      blt(AT, needle_len, DO3); // needle_len == 3
      // if needle_len == 2 then goto DO2
    }
  }

  if (needle_con_cnt == 4) {
    Label CH1_LOOP;
    (this->*load_4chr)(ch1, Address(needle));
    addi_d(result_tmp, haystack_len, -4);
    if (!haystack_isL) slli_d(result_tmp, result_tmp, haystack_chr_shift);
    add_d(haystack, haystack, result_tmp);
    sub_d(hlen_neg, R0, result_tmp);

    bind(CH1_LOOP);
    (this->*load_4chr)(ch2, Address(haystack, hlen_neg, Address::no_scale, 0));
    beq(ch1, ch2, MATCH);
    addi_d(hlen_neg, hlen_neg, haystack_chr_size);
    bge(R0, hlen_neg, CH1_LOOP);
    b(NOMATCH);
  }

  if ((needle_con_cnt == -1 && needle_isL == haystack_isL) || needle_con_cnt == 2) {
    Label CH1_LOOP;
    bind(DO2);
    (this->*load_2chr)(ch1, Address(needle));
    addi_d(result_tmp, haystack_len, -2);
    if (!haystack_isL) slli_d(result_tmp, result_tmp, haystack_chr_shift);
    add_d(haystack, haystack, result_tmp);
    sub_d(hlen_neg, R0, result_tmp);

    bind(CH1_LOOP);
    (this->*load_2chr)(ch2, Address(haystack, hlen_neg, Address::no_scale, 0));
    beq(ch1, ch2, MATCH);
    addi_d(hlen_neg, hlen_neg, haystack_chr_size);
    bge(R0, hlen_neg, CH1_LOOP);
    b(NOMATCH);
  }

  if ((needle_con_cnt == -1 && needle_isL == haystack_isL) || needle_con_cnt == 3) {
    Label FIRST_LOOP, STR2_NEXT, STR1_LOOP;

    bind(DO3);
    (this->*load_2chr)(first, Address(needle));
    (this->*needle_load_1chr)(ch1, Address(needle, 2 * needle_chr_size));
    addi_d(result_tmp, haystack_len, -3);
    if (!haystack_isL) slli_d(result_tmp, result_tmp, haystack_chr_shift);
    add_d(haystack, haystack, result_tmp);
    sub_d(hlen_neg, R0, result_tmp);

    bind(FIRST_LOOP);
    (this->*load_2chr)(ch2, Address(haystack, hlen_neg, Address::no_scale, 0));
    beq(first, ch2, STR1_LOOP);

    bind(STR2_NEXT);
    addi_d(hlen_neg, hlen_neg, haystack_chr_size);
    bge(R0, hlen_neg, FIRST_LOOP);
    b(NOMATCH);

    bind(STR1_LOOP);
    (this->*haystack_load_1chr)(ch2, Address(haystack, hlen_neg, Address::no_scale, 2 * haystack_chr_size));
    bne(ch1, ch2, STR2_NEXT);
    b(MATCH);
  }

  if (needle_con_cnt == -1 || needle_con_cnt == 1) {
    Label CH1_LOOP, HAS_ZERO, DO1_SHORT, DO1_LOOP;
    Register mask01 = nlen_tmp;
    Register mask7f = hlen_tmp;
    Register masked = first;

    bind(DO1);
    (this->*needle_load_1chr)(ch1, Address(needle));
    li(AT, 8);
    blt(haystack_len, AT, DO1_SHORT);

    addi_d(result_tmp, haystack_len, -8 / haystack_chr_size);
    if (!haystack_isL) slli_d(result_tmp, result_tmp, haystack_chr_shift);
    add_d(haystack, haystack, result_tmp);
    sub_d(hlen_neg, R0, result_tmp);

    if (haystack_isL) bstrins_d(ch1, ch1, 15, 8);
    bstrins_d(ch1, ch1, 31, 16);
    bstrins_d(ch1, ch1, 63, 32);

    li(mask01, haystack_isL ? 0x0101010101010101 : 0x0001000100010001);
    li(mask7f, haystack_isL ? 0x7f7f7f7f7f7f7f7f : 0x7fff7fff7fff7fff);

    bind(CH1_LOOP);
    ldx_d(ch2, haystack, hlen_neg);
    xorr(ch2, ch1, ch2);
    sub_d(masked, ch2, mask01);
    orr(ch2, ch2, mask7f);
    andn(masked, masked, ch2);
    bnez(masked, HAS_ZERO);
    addi_d(hlen_neg, hlen_neg, 8);
    blt(hlen_neg, R0, CH1_LOOP);

    li(AT, 8);
    bge(hlen_neg, AT, NOMATCH);
    move(hlen_neg, R0);
    b(CH1_LOOP);

    bind(HAS_ZERO);
    ctz_d(masked, masked);
    srli_d(masked, masked, 3);
    add_d(hlen_neg, hlen_neg, masked);
    b(MATCH);

    bind(DO1_SHORT);
    addi_d(result_tmp, haystack_len, -1);
    if (!haystack_isL) slli_d(result_tmp, result_tmp, haystack_chr_shift);
    add_d(haystack, haystack, result_tmp);
    sub_d(hlen_neg, R0, result_tmp);

    bind(DO1_LOOP);
    (this->*haystack_load_1chr)(ch2, Address(haystack, hlen_neg, Address::no_scale, 0));
    beq(ch1, ch2, MATCH);
    addi_d(hlen_neg, hlen_neg, haystack_chr_size);
    bge(R0, hlen_neg, DO1_LOOP);
  }

  bind(NOMATCH);
  li(result, -1);
  b(DONE);

  bind(MATCH);
  add_d(result, result_tmp, hlen_neg);
  if (!haystack_isL) srai_d(result, result, haystack_chr_shift);

  bind(DONE);
  pop(spilled_regs);
}

void C2_MacroAssembler::string_indexof_char(Register str1, Register cnt1,
                                            Register ch, Register result,
                                            Register tmp1, Register tmp2,
                                            Register tmp3)
{
  Label CH1_LOOP, HAS_ZERO, DO1_SHORT, DO1_LOOP, NOMATCH, DONE;

  beqz(cnt1, NOMATCH);

  move(result, R0);
  ori(tmp1, R0, 4);
  blt(cnt1, tmp1, DO1_LOOP);

  // UTF-16 char occupies 16 bits
  // ch -> chchchch
  bstrins_d(ch, ch, 31, 16);
  bstrins_d(ch, ch, 63, 32);

  li(tmp2, 0x0001000100010001);
  li(tmp3, 0x7fff7fff7fff7fff);

  bind(CH1_LOOP);
    ld_d(AT, str1, 0);
    xorr(AT, ch, AT);
    sub_d(tmp1, AT, tmp2);
    orr(AT, AT, tmp3);
    andn(tmp1, tmp1, AT);
    bnez(tmp1, HAS_ZERO);
    addi_d(str1, str1, 8);
    addi_d(result, result, 4);

    // meet the end of string
    beq(cnt1, result, NOMATCH);

    addi_d(tmp1, result, 4);
    bge(tmp1, cnt1, DO1_SHORT);
    b(CH1_LOOP);

  bind(HAS_ZERO);
    ctz_d(tmp1, tmp1);
    srli_d(tmp1, tmp1, 4);
    add_d(result, result, tmp1);
    b(DONE);

  // restore ch
  bind(DO1_SHORT);
    bstrpick_d(ch, ch, 15, 0);

  bind(DO1_LOOP);
    ld_hu(tmp1, str1, 0);
    beq(ch, tmp1, DONE);
    addi_d(str1, str1, 2);
    addi_d(result, result, 1);
    blt(result, cnt1, DO1_LOOP);

  bind(NOMATCH);
    addi_d(result, R0, -1);

  bind(DONE);
}

void C2_MacroAssembler::stringL_indexof_char(Register str1, Register cnt1,
                                             Register ch, Register result,
                                             Register tmp1, Register tmp2,
                                             Register tmp3)
{
  Label CH1_LOOP, HAS_ZERO, DO1_SHORT, DO1_LOOP, NOMATCH, DONE;

  beqz(cnt1, NOMATCH);

  move(result, R0);
  ori(tmp1, R0, 8);
  blt(cnt1, tmp1, DO1_LOOP);

  // Latin-1 char occupies 8 bits
  // ch -> chchchchchchchch
  bstrins_d(ch, ch, 15, 8);
  bstrins_d(ch, ch, 31, 16);
  bstrins_d(ch, ch, 63, 32);

  li(tmp2, 0x0101010101010101);
  li(tmp3, 0x7f7f7f7f7f7f7f7f);

  bind(CH1_LOOP);
    ld_d(AT, str1, 0);
    xorr(AT, ch, AT);
    sub_d(tmp1, AT, tmp2);
    orr(AT, AT, tmp3);
    andn(tmp1, tmp1, AT);
    bnez(tmp1, HAS_ZERO);
    addi_d(str1, str1, 8);
    addi_d(result, result, 8);

    // meet the end of string
    beq(cnt1, result, NOMATCH);

    addi_d(tmp1, result, 8);
    bge(tmp1, cnt1, DO1_SHORT);
    b(CH1_LOOP);

  bind(HAS_ZERO);
    ctz_d(tmp1, tmp1);
    srli_d(tmp1, tmp1, 3);
    add_d(result, result, tmp1);
    b(DONE);

  // restore ch
  bind(DO1_SHORT);
    bstrpick_d(ch, ch, 7, 0);

  bind(DO1_LOOP);
    ld_bu(tmp1, str1, 0);
    beq(ch, tmp1, DONE);
    addi_d(str1, str1, 1);
    addi_d(result, result, 1);
    blt(result, cnt1, DO1_LOOP);

  bind(NOMATCH);
    addi_d(result, R0, -1);

  bind(DONE);
}

// Compare strings, used for char[] and byte[].
void C2_MacroAssembler::string_compare(Register str1, Register str2,
                                    Register cnt1, Register cnt2, Register result,
                                    int ae, Register tmp1, Register tmp2) {
  Label L, Loop, LoopEnd, HaveResult, Done;

  bool isLL = ae == StrIntrinsicNode::LL;
  bool isLU = ae == StrIntrinsicNode::LU;
  bool isUL = ae == StrIntrinsicNode::UL;

  bool str1_isL = isLL || isLU;
  bool str2_isL = isLL || isUL;

  int charsInWord = isLL ? wordSize : wordSize/2;

  if (!str1_isL) srli_w(cnt1, cnt1, 1);
  if (!str2_isL) srli_w(cnt2, cnt2, 1);

  // compute the difference of lengths (in result)
  sub_d(result, cnt1, cnt2); // result holds the difference of two lengths

  // compute the shorter length (in cnt1)
  ori(AT, R0, charsInWord);
  bge(cnt2, cnt1, Loop);
  move(cnt1, cnt2);

  // Now the shorter length is in cnt1 and cnt2 can be used as a tmp register
  //
  // For example:
  //  If isLL == true and cnt1 > 8, we load 8 bytes from str1 and str2. (Suppose A1 and B1 are different)
  //    tmp1: A7 A6 A5 A4 A3 A2 A1 A0
  //    tmp2: B7 B6 B5 B4 B3 B2 B1 B0
  //
  //  Then Use xor to find the difference between tmp1 and tmp2, right shift.
  //    tmp1: 00 A7 A6 A5 A4 A3 A2 A1
  //    tmp2: 00 B7 B6 B5 B4 B3 B2 B1
  //
  //  Fetch 0 to 7 bits of tmp1 and tmp2, subtract to get the result.
  //  Other types are similar to isLL.
  bind(Loop);
  blt(cnt1, AT, LoopEnd);
  if (isLL) {
    ld_d(tmp1, str1, 0);
    ld_d(tmp2, str2, 0);
    beq(tmp1, tmp2, L);
    xorr(cnt2, tmp1, tmp2);
    ctz_d(cnt2, cnt2);
    andi(cnt2, cnt2, 0x38);
    srl_d(tmp1, tmp1, cnt2);
    srl_d(tmp2, tmp2, cnt2);
    bstrpick_d(tmp1, tmp1, 7, 0);
    bstrpick_d(tmp2, tmp2, 7, 0);
    sub_d(result, tmp1, tmp2);
    b(Done);
    bind(L);
    addi_d(str1, str1, 8);
    addi_d(str2, str2, 8);
    addi_d(cnt1, cnt1, -charsInWord);
    b(Loop);
  } else if (isLU) {
    ld_wu(cnt2, str1, 0);
    andr(tmp1, R0, R0);
    bstrins_d(tmp1, cnt2, 7, 0);
    srli_d(cnt2, cnt2, 8);
    bstrins_d(tmp1, cnt2, 23, 16);
    srli_d(cnt2, cnt2, 8);
    bstrins_d(tmp1, cnt2, 39, 32);
    srli_d(cnt2, cnt2, 8);
    bstrins_d(tmp1, cnt2, 55, 48);
    ld_d(tmp2, str2, 0);
    beq(tmp1, tmp2, L);
    xorr(cnt2, tmp1, tmp2);
    ctz_d(cnt2, cnt2);
    andi(cnt2, cnt2, 0x30);
    srl_d(tmp1, tmp1, cnt2);
    srl_d(tmp2, tmp2, cnt2);
    bstrpick_d(tmp1, tmp1, 15, 0);
    bstrpick_d(tmp2, tmp2, 15, 0);
    sub_d(result, tmp1, tmp2);
    b(Done);
    bind(L);
    addi_d(str1, str1, 4);
    addi_d(str2, str2, 8);
    addi_d(cnt1, cnt1, -charsInWord);
    b(Loop);
  } else if (isUL) {
    ld_wu(cnt2, str2, 0);
    andr(tmp2, R0, R0);
    bstrins_d(tmp2, cnt2, 7, 0);
    srli_d(cnt2, cnt2, 8);
    bstrins_d(tmp2, cnt2, 23, 16);
    srli_d(cnt2, cnt2, 8);
    bstrins_d(tmp2, cnt2, 39, 32);
    srli_d(cnt2, cnt2, 8);
    bstrins_d(tmp2, cnt2, 55, 48);
    ld_d(tmp1, str1, 0);
    beq(tmp1, tmp2, L);
    xorr(cnt2, tmp1, tmp2);
    ctz_d(cnt2, cnt2);
    andi(cnt2, cnt2, 0x30);
    srl_d(tmp1, tmp1, cnt2);
    srl_d(tmp2, tmp2, cnt2);
    bstrpick_d(tmp1, tmp1, 15, 0);
    bstrpick_d(tmp2, tmp2, 15, 0);
    sub_d(result, tmp1, tmp2);
    b(Done);
    bind(L);
    addi_d(str1, str1, 8);
    addi_d(str2, str2, 4);
    addi_d(cnt1, cnt1, -charsInWord);
    b(Loop);
  } else { // isUU
    ld_d(tmp1, str1, 0);
    ld_d(tmp2, str2, 0);
    beq(tmp1, tmp2, L);
    xorr(cnt2, tmp1, tmp2);
    ctz_d(cnt2, cnt2);
    andi(cnt2, cnt2, 0x30);
    srl_d(tmp1, tmp1, cnt2);
    srl_d(tmp2, tmp2, cnt2);
    bstrpick_d(tmp1, tmp1, 15, 0);
    bstrpick_d(tmp2, tmp2, 15, 0);
    sub_d(result, tmp1, tmp2);
    b(Done);
    bind(L);
    addi_d(str1, str1, 8);
    addi_d(str2, str2, 8);
    addi_d(cnt1, cnt1, -charsInWord);
    b(Loop);
  }

  bind(LoopEnd);
  beqz(cnt1, Done);
  if (str1_isL) {
    ld_bu(tmp1, str1, 0);
  } else {
    ld_hu(tmp1, str1, 0);
  }

  // compare current character
  if (str2_isL) {
    ld_bu(tmp2, str2, 0);
  } else {
    ld_hu(tmp2, str2, 0);
  }
  bne(tmp1, tmp2, HaveResult);
  addi_d(str1, str1, str1_isL ? 1 : 2);
  addi_d(str2, str2, str2_isL ? 1 : 2);
  addi_d(cnt1, cnt1, -1);
  b(LoopEnd);

  bind(HaveResult);
  sub_d(result, tmp1, tmp2);

  bind(Done);
}

// Compare char[] or byte[] arrays or substrings.
void C2_MacroAssembler::arrays_equals(Register str1, Register str2,
                                   Register cnt, Register tmp1, Register tmp2, Register result,
                                   bool is_char) {
  Label Loop, LoopEnd, True, False;

  addi_d(result, R0, 1);
  beq(str1, str2, True);  // same char[] ?
  beqz(cnt, True);

  addi_d(AT, R0, is_char ? wordSize/2 : wordSize);
  bind(Loop);
  blt(cnt, AT, LoopEnd);
  ld_d(tmp1, str1, 0);
  ld_d(tmp2, str2, 0);
  bne(tmp1, tmp2, False);
  addi_d(str1, str1, 8);
  addi_d(str2, str2, 8);
  addi_d(cnt, cnt, is_char ? -wordSize/2 : -wordSize);
  b(Loop);

  bind(LoopEnd);
  beqz(cnt, True);
  // compare current character
  if (is_char) {
    ld_hu(tmp1, str1, 0);
    ld_hu(tmp2, str2, 0);
  } else {
    ld_bu(tmp1, str1, 0);
    ld_bu(tmp2, str2, 0);
  }
  bne(tmp1, tmp2, False);
  addi_d(str1, str1, is_char ? 2 : 1);
  addi_d(str2, str2, is_char ? 2 : 1);
  addi_d(cnt, cnt, -1);
  b(LoopEnd);

  bind(False);
  addi_d(result, R0, 0);

  bind(True);
}

void C2_MacroAssembler::loadstore(Register reg, Register base, int disp, int type) {
  switch (type) {
    case STORE_BYTE:   st_b (reg, base, disp); break;
    case STORE_CHAR:
    case STORE_SHORT:  st_h (reg, base, disp); break;
    case STORE_INT:    st_w (reg, base, disp); break;
    case STORE_LONG:   st_d (reg, base, disp); break;
    case LOAD_BYTE:    ld_b (reg, base, disp); break;
    case LOAD_U_BYTE:  ld_bu(reg, base, disp); break;
    case LOAD_SHORT:   ld_h (reg, base, disp); break;
    case LOAD_U_SHORT: ld_hu(reg, base, disp); break;
    case LOAD_INT:     ld_w (reg, base, disp); break;
    case LOAD_U_INT:   ld_wu(reg, base, disp); break;
    case LOAD_LONG:    ld_d (reg, base, disp); break;
    case LOAD_LINKED_LONG:
      ll_d(reg, base, disp);
      break;
    default:
      ShouldNotReachHere();
    }
}

void C2_MacroAssembler::loadstore(Register reg, Register base, Register disp, int type) {
  switch (type) {
    case STORE_BYTE:   stx_b (reg, base, disp); break;
    case STORE_CHAR:
    case STORE_SHORT:  stx_h (reg, base, disp); break;
    case STORE_INT:    stx_w (reg, base, disp); break;
    case STORE_LONG:   stx_d (reg, base, disp); break;
    case LOAD_BYTE:    ldx_b (reg, base, disp); break;
    case LOAD_U_BYTE:  ldx_bu(reg, base, disp); break;
    case LOAD_SHORT:   ldx_h (reg, base, disp); break;
    case LOAD_U_SHORT: ldx_hu(reg, base, disp); break;
    case LOAD_INT:     ldx_w (reg, base, disp); break;
    case LOAD_U_INT:   ldx_wu(reg, base, disp); break;
    case LOAD_LONG:    ldx_d (reg, base, disp); break;
    case LOAD_LINKED_LONG:
      add_d(AT, base, disp);
      ll_d(reg, AT, 0);
      break;
    default:
      ShouldNotReachHere();
    }
}

void C2_MacroAssembler::loadstore(FloatRegister reg, Register base, int disp, int type) {
  switch (type) {
    case STORE_FLOAT:    fst_s(reg, base, disp); break;
    case STORE_DOUBLE:   fst_d(reg, base, disp); break;
    case STORE_VECTORX:  vst  (reg, base, disp); break;
    case STORE_VECTORY: xvst  (reg, base, disp); break;
    case LOAD_FLOAT:     fld_s(reg, base, disp); break;
    case LOAD_DOUBLE:    fld_d(reg, base, disp); break;
    case LOAD_VECTORX:   vld  (reg, base, disp); break;
    case LOAD_VECTORY:  xvld  (reg, base, disp); break;
    default:
      ShouldNotReachHere();
    }
}

void C2_MacroAssembler::loadstore(FloatRegister reg, Register base, Register disp, int type) {
  switch (type) {
    case STORE_FLOAT:    fstx_s(reg, base, disp); break;
    case STORE_DOUBLE:   fstx_d(reg, base, disp); break;
    case STORE_VECTORX:  vstx  (reg, base, disp); break;
    case STORE_VECTORY: xvstx  (reg, base, disp); break;
    case LOAD_FLOAT:     fldx_s(reg, base, disp); break;
    case LOAD_DOUBLE:    fldx_d(reg, base, disp); break;
    case LOAD_VECTORX:   vldx  (reg, base, disp); break;
    case LOAD_VECTORY:  xvldx  (reg, base, disp); break;
    default:
      ShouldNotReachHere();
    }
}

void C2_MacroAssembler::reduce_ins_v(FloatRegister vec1, FloatRegister vec2, FloatRegister vec3, BasicType type, int opcode) {
  switch (type) {
    case T_BYTE:
      switch (opcode) {
        case Op_AddReductionVI: vadd_b(vec1, vec2, vec3); break;
        case Op_MulReductionVI: vmul_b(vec1, vec2, vec3); break;
        case Op_MaxReductionV:  vmax_b(vec1, vec2, vec3); break;
        case Op_MinReductionV:  vmin_b(vec1, vec2, vec3); break;
        case Op_AndReductionV:  vand_v(vec1, vec2, vec3); break;
        case Op_OrReductionV:    vor_v(vec1, vec2, vec3); break;
        case Op_XorReductionV:  vxor_v(vec1, vec2, vec3); break;
        default:
          ShouldNotReachHere();
      }
      break;
    case T_SHORT:
      switch (opcode) {
        case Op_AddReductionVI: vadd_h(vec1, vec2, vec3); break;
        case Op_MulReductionVI: vmul_h(vec1, vec2, vec3); break;
        case Op_MaxReductionV:  vmax_h(vec1, vec2, vec3); break;
        case Op_MinReductionV:  vmin_h(vec1, vec2, vec3); break;
        case Op_AndReductionV:  vand_v(vec1, vec2, vec3); break;
        case Op_OrReductionV:    vor_v(vec1, vec2, vec3); break;
        case Op_XorReductionV:  vxor_v(vec1, vec2, vec3); break;
        default:
          ShouldNotReachHere();
      }
      break;
    case T_INT:
      switch (opcode) {
        case Op_AddReductionVI: vadd_w(vec1, vec2, vec3); break;
        case Op_MulReductionVI: vmul_w(vec1, vec2, vec3); break;
        case Op_MaxReductionV:  vmax_w(vec1, vec2, vec3); break;
        case Op_MinReductionV:  vmin_w(vec1, vec2, vec3); break;
        case Op_AndReductionV:  vand_v(vec1, vec2, vec3); break;
        case Op_OrReductionV:    vor_v(vec1, vec2, vec3); break;
        case Op_XorReductionV:  vxor_v(vec1, vec2, vec3); break;
        default:
          ShouldNotReachHere();
      }
      break;
    case T_LONG:
      switch (opcode) {
        case Op_AddReductionVL: vadd_d(vec1, vec2, vec3); break;
        case Op_MulReductionVL: vmul_d(vec1, vec2, vec3); break;
        case Op_MaxReductionV:  vmax_d(vec1, vec2, vec3); break;
        case Op_MinReductionV:  vmin_d(vec1, vec2, vec3); break;
        case Op_AndReductionV:  vand_v(vec1, vec2, vec3); break;
        case Op_OrReductionV:    vor_v(vec1, vec2, vec3); break;
        case Op_XorReductionV:  vxor_v(vec1, vec2, vec3); break;
        default:
          ShouldNotReachHere();
      }
      break;
    default:
      ShouldNotReachHere();
  }
}

void C2_MacroAssembler::reduce_ins_r(Register reg1, Register reg2, Register reg3, BasicType type, int opcode) {
  switch (type) {
    case T_BYTE:
    case T_SHORT:
    case T_INT:
      switch (opcode) {
        case Op_AddReductionVI: add_w(reg1, reg2, reg3); break;
        case Op_MulReductionVI: mul_w(reg1, reg2, reg3); break;
        case Op_AndReductionV:   andr(reg1, reg2, reg3); break;
        case Op_OrReductionV:     orr(reg1, reg2, reg3); break;
        case Op_XorReductionV:   xorr(reg1, reg2, reg3); break;
        default:
          ShouldNotReachHere();
      }
      break;
    case T_LONG:
      switch (opcode) {
        case Op_AddReductionVL: add_d(reg1, reg2, reg3); break;
        case Op_MulReductionVL: mul_d(reg1, reg2, reg3); break;
        case Op_AndReductionV:   andr(reg1, reg2, reg3); break;
        case Op_OrReductionV:     orr(reg1, reg2, reg3); break;
        case Op_XorReductionV:   xorr(reg1, reg2, reg3); break;
        default:
          ShouldNotReachHere();
      }
      break;
    default:
      ShouldNotReachHere();
  }
}

void C2_MacroAssembler::reduce_ins_f(FloatRegister reg1, FloatRegister reg2, FloatRegister reg3, BasicType type, int opcode) {
  switch (type) {
    case T_FLOAT:
      switch (opcode) {
        case Op_AddReductionVF: fadd_s(reg1, reg2, reg3); break;
        case Op_MulReductionVF: fmul_s(reg1, reg2, reg3); break;
        default:
          ShouldNotReachHere();
      }
      break;
    case T_DOUBLE:
      switch (opcode) {
        case Op_AddReductionVD: fadd_d(reg1, reg2, reg3); break;
        case Op_MulReductionVD: fmul_d(reg1, reg2, reg3); break;
        default:
          ShouldNotReachHere();
      }
      break;
    default:
      ShouldNotReachHere();
  }
}

void C2_MacroAssembler::reduce(Register dst, Register src, FloatRegister vsrc, FloatRegister tmp1, FloatRegister tmp2, BasicType type, int opcode, int vector_size) {
  if (vector_size == 32) {
    xvpermi_d(tmp1, vsrc, 0b00001110);
    reduce_ins_v(tmp1, vsrc, tmp1, type, opcode);
    vpermi_w(tmp2, tmp1, 0b00001110);
    reduce_ins_v(tmp1, tmp2, tmp1, type, opcode);
  } else if (vector_size == 16) {
    vpermi_w(tmp1, vsrc, 0b00001110);
    reduce_ins_v(tmp1, vsrc, tmp1, type, opcode);
  } else {
    ShouldNotReachHere();
  }

  if (type != T_LONG) {
    vshuf4i_w(tmp2, tmp1, 0b00000001);
    reduce_ins_v(tmp1, tmp2, tmp1, type, opcode);
    if (type != T_INT) {
      vshuf4i_h(tmp2, tmp1, 0b00000001);
      reduce_ins_v(tmp1, tmp2, tmp1, type, opcode);
      if (type != T_SHORT) {
        vshuf4i_b(tmp2, tmp1, 0b00000001);
        reduce_ins_v(tmp1, tmp2, tmp1, type, opcode);
      }
    }
  }

  switch (type) {
    case T_BYTE:  vpickve2gr_b(dst, tmp1, 0); break;
    case T_SHORT: vpickve2gr_h(dst, tmp1, 0); break;
    case T_INT:   vpickve2gr_w(dst, tmp1, 0); break;
    case T_LONG:  vpickve2gr_d(dst, tmp1, 0); break;
    default:
      ShouldNotReachHere();
  }
  if (opcode == Op_MaxReductionV) {
    slt(AT, dst, src);
    masknez(dst, dst, AT);
    maskeqz(AT, src, AT);
    orr(dst, dst, AT);
  } else if (opcode == Op_MinReductionV) {
    slt(AT, src, dst);
    masknez(dst, dst, AT);
    maskeqz(AT, src, AT);
    orr(dst, dst, AT);
  } else {
    reduce_ins_r(dst, dst, src, type, opcode);
  }
  switch (type) {
    case T_BYTE:  ext_w_b(dst, dst); break;
    case T_SHORT: ext_w_h(dst, dst); break;
    default:
      break;
  }
}

void C2_MacroAssembler::reduce(FloatRegister dst, FloatRegister src, FloatRegister vsrc, FloatRegister tmp, BasicType type, int opcode, int vector_size) {
  if (vector_size == 32) {
    switch (type) {
      case T_FLOAT:
        reduce_ins_f(dst, vsrc, src, type, opcode);
        xvpickve_w(tmp, vsrc, 1);
        reduce_ins_f(dst, tmp, dst, type, opcode);
        xvpickve_w(tmp, vsrc, 2);
        reduce_ins_f(dst, tmp, dst, type, opcode);
        xvpickve_w(tmp, vsrc, 3);
        reduce_ins_f(dst, tmp, dst, type, opcode);
        xvpickve_w(tmp, vsrc, 4);
        reduce_ins_f(dst, tmp, dst, type, opcode);
        xvpickve_w(tmp, vsrc, 5);
        reduce_ins_f(dst, tmp, dst, type, opcode);
        xvpickve_w(tmp, vsrc, 6);
        reduce_ins_f(dst, tmp, dst, type, opcode);
        xvpickve_w(tmp, vsrc, 7);
        reduce_ins_f(dst, tmp, dst, type, opcode);
        break;
      case T_DOUBLE:
        reduce_ins_f(dst, vsrc, src, type, opcode);
        xvpickve_d(tmp, vsrc, 1);
        reduce_ins_f(dst, tmp, dst, type, opcode);
        xvpickve_d(tmp, vsrc, 2);
        reduce_ins_f(dst, tmp, dst, type, opcode);
        xvpickve_d(tmp, vsrc, 3);
        reduce_ins_f(dst, tmp, dst, type, opcode);
        break;
      default:
        ShouldNotReachHere();
    }
  } else if (vector_size == 16) {
    switch (type) {
      case T_FLOAT:
        reduce_ins_f(dst, vsrc, src, type, opcode);
        vpermi_w(tmp, vsrc, 0b00000001);
        reduce_ins_f(dst, tmp, dst, type, opcode);
        vpermi_w(tmp, vsrc, 0b00000010);
        reduce_ins_f(dst, tmp, dst, type, opcode);
        vpermi_w(tmp, vsrc, 0b00000011);
        reduce_ins_f(dst, tmp, dst, type, opcode);
        break;
      case T_DOUBLE:
        reduce_ins_f(dst, vsrc, src, type, opcode);
        vpermi_w(tmp, vsrc, 0b00001110);
        reduce_ins_f(dst, tmp, dst, type, opcode);
        break;
      default:
        ShouldNotReachHere();
    }
  } else {
    ShouldNotReachHere();
  }
}

void C2_MacroAssembler::vector_compare(FloatRegister dst, FloatRegister src1, FloatRegister src2, BasicType bt, int cond, int vector_size) {
  if (vector_size == 32) {
    if (bt == T_BYTE) {
      switch (cond) {
        case BoolTest::ne:  xvseq_b (dst, src1, src2); xvxori_b(dst, dst, 0xff); break;
        case BoolTest::eq:  xvseq_b (dst, src1, src2); break;
        case BoolTest::ge:  xvsle_b (dst, src2, src1); break;
        case BoolTest::gt:  xvslt_b (dst, src2, src1); break;
        case BoolTest::le:  xvsle_b (dst, src1, src2); break;
        case BoolTest::lt:  xvslt_b (dst, src1, src2); break;
        case BoolTest::uge: xvsle_bu(dst, src2, src1); break;
        case BoolTest::ugt: xvslt_bu(dst, src2, src1); break;
        case BoolTest::ule: xvsle_bu(dst, src1, src2); break;
        case BoolTest::ult: xvslt_bu(dst, src1, src2); break;
        default:
          ShouldNotReachHere();
      }
    } else if (bt == T_SHORT) {
      switch (cond) {
        case BoolTest::ne:  xvseq_h (dst, src1, src2); xvxori_b(dst, dst, 0xff); break;
        case BoolTest::eq:  xvseq_h (dst, src1, src2); break;
        case BoolTest::ge:  xvsle_h (dst, src2, src1); break;
        case BoolTest::gt:  xvslt_h (dst, src2, src1); break;
        case BoolTest::le:  xvsle_h (dst, src1, src2); break;
        case BoolTest::lt:  xvslt_h (dst, src1, src2); break;
        case BoolTest::uge: xvsle_hu(dst, src2, src1); break;
        case BoolTest::ugt: xvslt_hu(dst, src2, src1); break;
        case BoolTest::ule: xvsle_hu(dst, src1, src2); break;
        case BoolTest::ult: xvslt_hu(dst, src1, src2); break;
        default:
          ShouldNotReachHere();
      }
    } else if (bt == T_INT) {
      switch (cond) {
        case BoolTest::ne:  xvseq_w (dst, src1, src2); xvxori_b(dst, dst, 0xff); break;
        case BoolTest::eq:  xvseq_w (dst, src1, src2); break;
        case BoolTest::ge:  xvsle_w (dst, src2, src1); break;
        case BoolTest::gt:  xvslt_w (dst, src2, src1); break;
        case BoolTest::le:  xvsle_w (dst, src1, src2); break;
        case BoolTest::lt:  xvslt_w (dst, src1, src2); break;
        case BoolTest::uge: xvsle_wu(dst, src2, src1); break;
        case BoolTest::ugt: xvslt_wu(dst, src2, src1); break;
        case BoolTest::ule: xvsle_wu(dst, src1, src2); break;
        case BoolTest::ult: xvslt_wu(dst, src1, src2); break;
        default:
          ShouldNotReachHere();
      }
    } else if (bt == T_LONG) {
      switch (cond) {
        case BoolTest::ne:  xvseq_d (dst, src1, src2); xvxori_b(dst, dst, 0xff); break;
        case BoolTest::eq:  xvseq_d (dst, src1, src2); break;
        case BoolTest::ge:  xvsle_d (dst, src2, src1); break;
        case BoolTest::gt:  xvslt_d (dst, src2, src1); break;
        case BoolTest::le:  xvsle_d (dst, src1, src2); break;
        case BoolTest::lt:  xvslt_d (dst, src1, src2); break;
        case BoolTest::uge: xvsle_du(dst, src2, src1); break;
        case BoolTest::ugt: xvslt_du(dst, src2, src1); break;
        case BoolTest::ule: xvsle_du(dst, src1, src2); break;
        case BoolTest::ult: xvslt_du(dst, src1, src2); break;
        default:
          ShouldNotReachHere();
      }
    } else if (bt == T_FLOAT) {
      switch (cond) {
        case BoolTest::ne: xvfcmp_cune_s(dst, src1, src2); break;
        case BoolTest::eq: xvfcmp_ceq_s (dst, src1, src2); break;
        case BoolTest::ge: xvfcmp_cle_s (dst, src2, src1); break;
        case BoolTest::gt: xvfcmp_clt_s (dst, src2, src1); break;
        case BoolTest::le: xvfcmp_cule_s(dst, src1, src2); break;
        case BoolTest::lt: xvfcmp_cult_s(dst, src1, src2); break;
        default:
          ShouldNotReachHere();
      }
    } else if (bt == T_DOUBLE) {
      switch (cond) {
        case BoolTest::ne: xvfcmp_cune_d(dst, src1, src2); break;
        case BoolTest::eq: xvfcmp_ceq_d (dst, src1, src2); break;
        case BoolTest::ge: xvfcmp_cle_d (dst, src2, src1); break;
        case BoolTest::gt: xvfcmp_clt_d (dst, src2, src1); break;
        case BoolTest::le: xvfcmp_cule_d(dst, src1, src2); break;
        case BoolTest::lt: xvfcmp_cult_d(dst, src1, src2); break;
        default:
          ShouldNotReachHere();
      }
    } else {
      ShouldNotReachHere();
    }
  } else if (vector_size == 16) {
    if (bt == T_BYTE) {
      switch (cond) {
        case BoolTest::ne:  vseq_b (dst, src1, src2); vxori_b(dst, dst, 0xff); break;
        case BoolTest::eq:  vseq_b (dst, src1, src2); break;
        case BoolTest::ge:  vsle_b (dst, src2, src1); break;
        case BoolTest::gt:  vslt_b (dst, src2, src1); break;
        case BoolTest::le:  vsle_b (dst, src1, src2); break;
        case BoolTest::lt:  vslt_b (dst, src1, src2); break;
        case BoolTest::uge: vsle_bu(dst, src2, src1); break;
        case BoolTest::ugt: vslt_bu(dst, src2, src1); break;
        case BoolTest::ule: vsle_bu(dst, src1, src2); break;
        case BoolTest::ult: vslt_bu(dst, src1, src2); break;
        default:
          ShouldNotReachHere();
      }
    } else if (bt == T_SHORT) {
      switch (cond) {
        case BoolTest::ne:  vseq_h (dst, src1, src2); vxori_b(dst, dst, 0xff); break;
        case BoolTest::eq:  vseq_h (dst, src1, src2); break;
        case BoolTest::ge:  vsle_h (dst, src2, src1); break;
        case BoolTest::gt:  vslt_h (dst, src2, src1); break;
        case BoolTest::le:  vsle_h (dst, src1, src2); break;
        case BoolTest::lt:  vslt_h (dst, src1, src2); break;
        case BoolTest::uge: vsle_hu(dst, src2, src1); break;
        case BoolTest::ugt: vslt_hu(dst, src2, src1); break;
        case BoolTest::ule: vsle_hu(dst, src1, src2); break;
        case BoolTest::ult: vslt_hu(dst, src1, src2); break;
        default:
          ShouldNotReachHere();
      }
    } else if (bt == T_INT) {
      switch (cond) {
        case BoolTest::ne:  vseq_w (dst, src1, src2); vxori_b(dst, dst, 0xff); break;
        case BoolTest::eq:  vseq_w (dst, src1, src2); break;
        case BoolTest::ge:  vsle_w (dst, src2, src1); break;
        case BoolTest::gt:  vslt_w (dst, src2, src1); break;
        case BoolTest::le:  vsle_w (dst, src1, src2); break;
        case BoolTest::lt:  vslt_w (dst, src1, src2); break;
        case BoolTest::uge: vsle_wu(dst, src2, src1); break;
        case BoolTest::ugt: vslt_wu(dst, src2, src1); break;
        case BoolTest::ule: vsle_wu(dst, src1, src2); break;
        case BoolTest::ult: vslt_wu(dst, src1, src2); break;
        default:
          ShouldNotReachHere();
      }
    } else if (bt == T_LONG) {
      switch (cond) {
        case BoolTest::ne:  vseq_d (dst, src1, src2); vxori_b(dst, dst, 0xff); break;
        case BoolTest::eq:  vseq_d (dst, src1, src2); break;
        case BoolTest::ge:  vsle_d (dst, src2, src1); break;
        case BoolTest::gt:  vslt_d (dst, src2, src1); break;
        case BoolTest::le:  vsle_d (dst, src1, src2); break;
        case BoolTest::lt:  vslt_d (dst, src1, src2); break;
        case BoolTest::uge: vsle_du(dst, src2, src1); break;
        case BoolTest::ugt: vslt_du(dst, src2, src1); break;
        case BoolTest::ule: vsle_du(dst, src1, src2); break;
        case BoolTest::ult: vslt_du(dst, src1, src2); break;
        default:
          ShouldNotReachHere();
      }
    } else if (bt == T_FLOAT) {
      switch (cond) {
        case BoolTest::ne: vfcmp_cune_s(dst, src1, src2); break;
        case BoolTest::eq: vfcmp_ceq_s (dst, src1, src2); break;
        case BoolTest::ge: vfcmp_cle_s (dst, src2, src1); break;
        case BoolTest::gt: vfcmp_clt_s (dst, src2, src1); break;
        case BoolTest::le: vfcmp_cule_s(dst, src1, src2); break;
        case BoolTest::lt: vfcmp_cult_s(dst, src1, src2); break;
        default:
          ShouldNotReachHere();
      }
    } else if (bt == T_DOUBLE) {
      switch (cond) {
        case BoolTest::ne: vfcmp_cune_d(dst, src1, src2); break;
        case BoolTest::eq: vfcmp_ceq_d (dst, src1, src2); break;
        case BoolTest::ge: vfcmp_cle_d (dst, src2, src1); break;
        case BoolTest::gt: vfcmp_clt_d (dst, src2, src1); break;
        case BoolTest::le: vfcmp_cule_d(dst, src1, src2); break;
        case BoolTest::lt: vfcmp_cult_d(dst, src1, src2); break;
        default:
          ShouldNotReachHere();
      }
    } else {
      ShouldNotReachHere();
    }
  } else {
    ShouldNotReachHere();
  }
}

void C2_MacroAssembler::cmp_branch_short(int flag, Register op1, Register op2, Label& L, bool is_signed) {

    switch(flag) {
      case 0x01: //equal
          beq(op1, op2, L);
        break;
      case 0x02: //not_equal
          bne(op1, op2, L);
        break;
      case 0x03: //above
        if (is_signed)
          blt(op2, op1, L);
        else
          bltu(op2, op1, L);
        break;
      case 0x04: //above_equal
        if (is_signed)
          bge(op1, op2, L);
        else
          bgeu(op1, op2, L);
        break;
      case 0x05: //below
        if (is_signed)
          blt(op1, op2, L);
        else
          bltu(op1, op2, L);
        break;
      case 0x06: //below_equal
        if (is_signed)
          bge(op2, op1, L);
        else
          bgeu(op2, op1, L);
        break;
      default:
        Unimplemented();
    }
}

void C2_MacroAssembler::cmp_branch_long(int flag, Register op1, Register op2, Label* L, bool is_signed) {
    switch(flag) {
      case 0x01: //equal
        beq_long(op1, op2, *L);
        break;
      case 0x02: //not_equal
        bne_long(op1, op2, *L);
        break;
      case 0x03: //above
        if (is_signed)
          blt_long(op2, op1, *L, true /* signed */);
        else
          blt_long(op2, op1, *L, false);
        break;
      case 0x04: //above_equal
        if (is_signed)
          bge_long(op1, op2, *L, true /* signed */);
        else
          bge_long(op1, op2, *L, false);
        break;
      case 0x05: //below
        if (is_signed)
          blt_long(op1, op2, *L, true /* signed */);
        else
          blt_long(op1, op2, *L, false);
        break;
      case 0x06: //below_equal
        if (is_signed)
          bge_long(op2, op1, *L, true /* signed */);
        else
          bge_long(op2, op1, *L, false);
        break;
      default:
        Unimplemented();
    }
}

void C2_MacroAssembler::cmp_branchEqNe_off21(int flag, Register op1, Label& L) {
    switch(flag) {
      case 0x01: //equal
        beqz(op1, L);
        break;
      case 0x02: //not_equal
        bnez(op1, L);
        break;
      default:
        Unimplemented();
    }
}
