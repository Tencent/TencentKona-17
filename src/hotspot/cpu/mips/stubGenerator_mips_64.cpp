/*
 * Copyright (c) 2003, 2013, Oracle and/or its affiliates. All rights reserved.
 * Copyright (c) 2015, 2023, Loongson Technology. All rights reserved.
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
#include "compiler/oopMap.hpp"
#include "gc/shared/barrierSet.hpp"
#include "gc/shared/barrierSetAssembler.hpp"
#include "interpreter/interpreter.hpp"
#include "nativeInst_mips.hpp"
#include "oops/instanceOop.hpp"
#include "oops/method.hpp"
#include "oops/objArrayKlass.hpp"
#include "oops/oop.inline.hpp"
#include "prims/methodHandles.hpp"
#include "runtime/frame.inline.hpp"
#include "runtime/handles.inline.hpp"
#include "runtime/sharedRuntime.hpp"
#include "runtime/stubCodeGenerator.hpp"
#include "runtime/stubRoutines.hpp"
#include "runtime/thread.inline.hpp"
#ifdef COMPILER2
#include "opto/runtime.hpp"
#endif

// Declaration and definition of StubGenerator (no .hpp file).
// For a more detailed description of the stub routine structure
// see the comment in stubRoutines.hpp

#define __ _masm->

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
#define T8 RT8
#define T9 RT9

#define TIMES_OOP (UseCompressedOops ? Address::times_4 : Address::times_8)
//#define a__ ((Assembler*)_masm)->

//#ifdef PRODUCT
//#define BLOCK_COMMENT(str) /* nothing */
//#else
//#define BLOCK_COMMENT(str) __ block_comment(str)
//#endif

//#define BIND(label) bind(label); BLOCK_COMMENT(#label ":")
const int MXCSR_MASK = 0xFFC0;  // Mask out any pending exceptions

// Stub Code definitions

class StubGenerator: public StubCodeGenerator {
 private:

  // ABI mips n64
  // This fig is not MIPS ABI. It is call Java from C ABI.
  // Call stubs are used to call Java from C
  //
  //    [ return_from_Java     ]
  //    [ argument word n-1    ] <--- sp
  //      ...
  //    [ argument word 0      ]
  //      ...
  // -8 [ S6                   ]
  // -7 [ S5                   ]
  // -6 [ S4                   ]
  // -5 [ S3                   ]
  // -4 [ S1                   ]
  // -3 [ TSR(S2)              ]
  // -2 [ LVP(S7)              ]
  // -1 [ BCP(S1)              ]
  //  0 [ saved fp             ] <--- fp_after_call
  //  1 [ return address       ]
  //  2 [ ptr. to call wrapper ] <--- a0 (old sp -->)fp
  //  3 [ result               ] <--- a1
  //  4 [ result_type          ] <--- a2
  //  5 [ method               ] <--- a3
  //  6 [ entry_point          ] <--- a4
  //  7 [ parameters           ] <--- a5
  //  8 [ parameter_size       ] <--- a6
  //  9 [ thread               ] <--- a7

  //
  //  n64 does not save paras in sp.
  //
  //    [ return_from_Java     ]
  //    [ argument word n-1    ] <--- sp
  //      ...
  //    [ argument word 0      ]
  //      ...
  //-13 [ thread               ]
  //-12 [ result_type          ] <--- a2
  //-11 [ result               ] <--- a1
  //-10 [                      ]
  // -9 [ ptr. to call wrapper ] <--- a0
  // -8 [ S6                   ]
  // -7 [ S5                   ]
  // -6 [ S4                   ]
  // -5 [ S3                   ]
  // -4 [ S1                   ]
  // -3 [ TSR(S2)              ]
  // -2 [ LVP(S7)              ]
  // -1 [ BCP(S1)              ]
  //  0 [ saved fp             ] <--- fp_after_call
  //  1 [ return address       ]
  //  2 [                      ] <--- old sp
  //
  // Find a right place in the call_stub for GP.
  // GP will point to the starting point of Interpreter::dispatch_table(itos).
  // It should be saved/restored before/after Java calls.
  //
  enum call_stub_layout {
    RA_off             = 1,
    FP_off             = 0,
    BCP_off            = -1,
    LVP_off            = -2,
    TSR_off            = -3,
    S1_off             = -4,
    S3_off             = -5,
    S4_off             = -6,
    S5_off             = -7,
    S6_off             = -8,
    call_wrapper_off   = -9,
    result_off         = -11,
    result_type_off    = -12,
    thread_off         = -13,
    total_off          = thread_off - 1,
    GP_off             = -14,
 };

  address generate_call_stub(address& return_address) {

    StubCodeMark mark(this, "StubRoutines", "call_stub");
    address start = __ pc();

    // same as in generate_catch_exception()!

    // stub code
    // save ra and fp
    __ enter();
    // I think 14 is the max gap between argument and callee saved register
    assert((int)frame::entry_frame_call_wrapper_offset == (int)call_wrapper_off, "adjust this code");
    __ daddiu(SP, SP, total_off * wordSize);
    __ sd(BCP, FP, BCP_off * wordSize);
    __ sd(LVP, FP, LVP_off * wordSize);
    __ sd(TSR, FP, TSR_off * wordSize);
    __ sd(S1, FP, S1_off * wordSize);
    __ sd(S3, FP, S3_off * wordSize);
    __ sd(S4, FP, S4_off * wordSize);
    __ sd(S5, FP, S5_off * wordSize);
    __ sd(S6, FP, S6_off * wordSize);
    __ sd(A0, FP, call_wrapper_off * wordSize);
    __ sd(A1, FP, result_off * wordSize);
    __ sd(A2, FP, result_type_off * wordSize);
    __ sd(A7, FP, thread_off * wordSize);
    __ sd(GP, FP, GP_off * wordSize);

    __ set64(GP, (long)Interpreter::dispatch_table(itos));

#ifdef OPT_THREAD
    __ move(TREG, A7);
#endif
    //add for compressedoops
    __ reinit_heapbase();

#ifdef ASSERT
    // make sure we have no pending exceptions
    {
      Label L;
      __ ld(AT, A7, in_bytes(Thread::pending_exception_offset()));
      __ beq(AT, R0, L);
      __ delayed()->nop();
      /* FIXME: I do not know how to realize stop in mips arch, do it in the future */
      __ stop("StubRoutines::call_stub: entered with pending exception");
      __ bind(L);
    }
#endif

    // pass parameters if any
    // A5: parameter
    // A6: parameter_size
    // T0: parameter_size_tmp(--)
    // T2: offset(++)
    // T3: tmp
    Label parameters_done;
    // judge if the parameter_size equals 0
    __ beq(A6, R0, parameters_done);
    __ delayed()->nop();
    __ dsll(AT, A6, Interpreter::logStackElementSize);
    __ dsubu(SP, SP, AT);
    assert(StackAlignmentInBytes == 16, "must be");
    __ dins(SP, R0, 0, 4);
    // Copy Java parameters in reverse order (receiver last)
    // Note that the argument order is inverted in the process
    Label loop;
    __ move(T0, A6);
    __ move(T2, R0);
    __ bind(loop);

    // get parameter
    __ dsll(T3, T0, LogBytesPerWord);
    __ daddu(T3, T3, A5);
    __ ld(AT, T3,  -wordSize);
    __ dsll(T3, T2, LogBytesPerWord);
    __ daddu(T3, T3, SP);
    __ sd(AT, T3, Interpreter::expr_offset_in_bytes(0));
    __ daddiu(T2, T2, 1);
    __ daddiu(T0, T0, -1);
    __ bne(T0, R0, loop);
    __ delayed()->nop();
    // advance to next parameter

    // call Java function
    __ bind(parameters_done);

    // receiver in V0, Method* in Rmethod

    __ move(Rmethod, A3);
    __ move(Rsender, SP);             //set sender sp
    __ jalr(A4);
    __ delayed()->nop();
    return_address = __ pc();

    Label common_return;
    __ bind(common_return);

    // store result depending on type
    // (everything that is not T_LONG, T_FLOAT or T_DOUBLE is treated as T_INT)
    __ ld(T0, FP, result_off * wordSize);   // result --> T0
    Label is_long, is_float, is_double, exit;
    __ ld(T2, FP, result_type_off * wordSize);  // result_type --> T2
    __ daddiu(T3, T2, (-1) * T_LONG);
    __ beq(T3, R0, is_long);
    __ delayed()->daddiu(T3, T2, (-1) * T_FLOAT);
    __ beq(T3, R0, is_float);
    __ delayed()->daddiu(T3, T2, (-1) * T_DOUBLE);
    __ beq(T3, R0, is_double);
    __ delayed()->nop();

    // handle T_INT case
    __ sd(V0, T0, 0 * wordSize);
    __ bind(exit);

    // restore
    __ ld(BCP, FP, BCP_off * wordSize);
    __ ld(LVP, FP, LVP_off * wordSize);
    __ ld(GP, FP, GP_off * wordSize);
    __ ld(TSR, FP, TSR_off * wordSize);

    __ ld(S1, FP, S1_off * wordSize);
    __ ld(S3, FP, S3_off * wordSize);
    __ ld(S4, FP, S4_off * wordSize);
    __ ld(S5, FP, S5_off * wordSize);
    __ ld(S6, FP, S6_off * wordSize);

    __ leave();

    // return
    __ jr(RA);
    __ delayed()->nop();

    // handle return types different from T_INT
    __ bind(is_long);
    __ sd(V0, T0, 0 * wordSize);
    __ b(exit);
    __ delayed()->nop();

    __ bind(is_float);
    __ swc1(F0, T0, 0 * wordSize);
    __ b(exit);
    __ delayed()->nop();

    __ bind(is_double);
    __ sdc1(F0, T0, 0 * wordSize);
    __ b(exit);
    __ delayed()->nop();
    //FIXME, 1.6 mips version add operation of fpu here
    StubRoutines::gs2::set_call_stub_compiled_return(__ pc());
    __ b(common_return);
    __ delayed()->nop();
    return start;
  }

  // Return point for a Java call if there's an exception thrown in
  // Java code.  The exception is caught and transformed into a
  // pending exception stored in JavaThread that can be tested from
  // within the VM.
  //
  // Note: Usually the parameters are removed by the callee. In case
  // of an exception crossing an activation frame boundary, that is
  // not the case if the callee is compiled code => need to setup the
  // sp.
  //
  // V0: exception oop

  address generate_catch_exception() {
    StubCodeMark mark(this, "StubRoutines", "catch_exception");
    address start = __ pc();

    Register thread = TREG;

    // get thread directly
#ifndef OPT_THREAD
    __ ld(thread, FP, thread_off * wordSize);
#endif

#ifdef ASSERT
    // verify that threads correspond
    { Label L;
      __ get_thread(T8);
      __ beq(T8, thread, L);
      __ delayed()->nop();
      __ stop("StubRoutines::catch_exception: threads must correspond");
      __ bind(L);
    }
#endif
    // set pending exception
    __ verify_oop(V0);
    __ sd(V0, thread, in_bytes(Thread::pending_exception_offset()));
    __ li(AT, (long)__FILE__);
    __ sd(AT, thread, in_bytes(Thread::exception_file_offset   ()));
    __ li(AT, (long)__LINE__);
    __ sd(AT, thread, in_bytes(Thread::exception_line_offset   ()));

    // complete return to VM
    assert(StubRoutines::_call_stub_return_address != NULL, "_call_stub_return_address must have been generated before");
    __ jmp(StubRoutines::_call_stub_return_address, relocInfo::none);
    __ delayed()->nop();

    return start;
  }

  // Continuation point for runtime calls returning with a pending
  // exception.  The pending exception check happened in the runtime
  // or native call stub.  The pending exception in Thread is
  // converted into a Java-level exception.
  //
  // Contract with Java-level exception handlers:
  // V0: exception
  // V1: throwing pc
  //
  // NOTE: At entry of this stub, exception-pc must be on stack !!

  address generate_forward_exception() {
    StubCodeMark mark(this, "StubRoutines", "forward exception");
    //Register thread = TREG;
    Register thread = TREG;
    address start = __ pc();

    // Upon entry, the sp points to the return address returning into
    // Java (interpreted or compiled) code; i.e., the return address
    // throwing pc.
    //
    // Arguments pushed before the runtime call are still on the stack
    // but the exception handler will reset the stack pointer ->
    // ignore them.  A potential result in registers can be ignored as
    // well.

#ifndef OPT_THREAD
    __ get_thread(thread);
#endif
#ifdef ASSERT
    // make sure this code is only executed if there is a pending exception
    {
      Label L;
      __ ld(AT, thread, in_bytes(Thread::pending_exception_offset()));
      __ bne(AT, R0, L);
      __ delayed()->nop();
      __ stop("StubRoutines::forward exception: no pending exception (1)");
      __ bind(L);
    }
#endif

    // compute exception handler into T9
    __ ld(A1, SP, 0);
    __ call_VM_leaf(CAST_FROM_FN_PTR(address, SharedRuntime::exception_handler_for_return_address), thread, A1);
    __ move(T9, V0);
    __ pop(V1);

#ifndef OPT_THREAD
    __ get_thread(thread);
#endif
    __ ld(V0, thread, in_bytes(Thread::pending_exception_offset()));
    __ sd(R0, thread, in_bytes(Thread::pending_exception_offset()));

#ifdef ASSERT
    // make sure exception is set
    {
      Label L;
      __ bne(V0, R0, L);
      __ delayed()->nop();
      __ stop("StubRoutines::forward exception: no pending exception (2)");
      __ bind(L);
    }
#endif

    // continue at exception handler (return address removed)
    // V0: exception
    // T9: exception handler
    // V1: throwing pc
    __ verify_oop(V0);
    __ jr(T9);
    __ delayed()->nop();

    return start;
  }

  // Non-destructive plausibility checks for oops
  //
  address generate_verify_oop() {
    StubCodeMark mark(this, "StubRoutines", "verify_oop");
    address start = __ pc();
    __ reinit_heapbase();
    __ verify_oop_subroutine();
    address end = __ pc();
    return start;
  }

  //
  //  Generate overlap test for array copy stubs
  //
  //  Input:
  //     A0    -  array1
  //     A1    -  array2
  //     A2    -  element count
  //

 // use T9 as temp
  void array_overlap_test(address no_overlap_target, int log2_elem_size) {
    int elem_size = 1 << log2_elem_size;
    Address::ScaleFactor sf = Address::times_1;

    switch (log2_elem_size) {
      case 0: sf = Address::times_1; break;
      case 1: sf = Address::times_2; break;
      case 2: sf = Address::times_4; break;
      case 3: sf = Address::times_8; break;
    }

    __ dsll(AT, A2, sf);
    __ daddu(AT, AT, A0);
    __ daddiu(T9, AT, -elem_size);
    __ dsubu(AT, A1, A0);
    __ blez(AT, no_overlap_target);
    __ delayed()->nop();
    __ dsubu(AT, A1, T9);
    __ bgtz(AT, no_overlap_target);
    __ delayed()->nop();

    // If A0 = 0xf... and A1 = 0x0..., than goto no_overlap_target
    Label L;
    __ bgez(A0, L);
    __ delayed()->nop();
    __ bgtz(A1, no_overlap_target);
    __ delayed()->nop();
    __ bind(L);

  }

  //
  // Generate stub for array fill. If "aligned" is true, the
  // "to" address is assumed to be heapword aligned.
  //
  // Arguments for generated stub:
  //   to:    c_rarg0
  //   value: c_rarg1
  //   count: c_rarg2 treated as signed
  //
  address generate_fill(BasicType t, bool aligned, const char *name) {
    __ align(CodeEntryAlignment);
    StubCodeMark mark(this, "StubRoutines", name);
    address start = __ pc();

    const Register to        = A0;  // source array address
    const Register value     = A1;  // value
    const Register count     = A2;  // elements count

    const Register cnt_words = T8;  // temp register

    __ enter();

    Label L_fill_elements, L_exit1;

    int shift = -1;
    switch (t) {
      case T_BYTE:
        shift = 0;
        __ slti(AT, count, 8 >> shift); // Short arrays (< 8 bytes) fill by element
        __ dins(value, value, 8, 8);   // 8 bit -> 16 bit
        __ dins(value, value, 16, 16); // 16 bit -> 32 bit
        __ bne(AT, R0, L_fill_elements);
        __ delayed()->nop();
        break;
      case T_SHORT:
        shift = 1;
        __ slti(AT, count, 8 >> shift); // Short arrays (< 8 bytes) fill by element
        __ dins(value, value, 16, 16); // 16 bit -> 32 bit
        __ bne(AT, R0, L_fill_elements);
        __ delayed()->nop();
        break;
      case T_INT:
        shift = 2;
        __ slti(AT, count, 8 >> shift); // Short arrays (< 8 bytes) fill by element
        __ bne(AT, R0, L_fill_elements);
        __ delayed()->nop();
        break;
      default: ShouldNotReachHere();
    }

    // Align source address at 8 bytes address boundary.
    Label L_skip_align1, L_skip_align2, L_skip_align4;
    if (!aligned) {
      switch (t) {
        case T_BYTE:
          // One byte misalignment happens only for byte arrays.
          __ andi(AT, to, 1);
          __ beq(AT, R0, L_skip_align1);
          __ delayed()->nop();
          __ sb(value, to, 0);
          __ daddiu(to, to, 1);
          __ addiu32(count, count, -1);
          __ bind(L_skip_align1);
          // Fallthrough
        case T_SHORT:
          // Two bytes misalignment happens only for byte and short (char) arrays.
          __ andi(AT, to, 1 << 1);
          __ beq(AT, R0, L_skip_align2);
          __ delayed()->nop();
          __ sh(value, to, 0);
          __ daddiu(to, to, 2);
          __ addiu32(count, count, -(2 >> shift));
          __ bind(L_skip_align2);
          // Fallthrough
        case T_INT:
          // Align to 8 bytes, we know we are 4 byte aligned to start.
          __ andi(AT, to, 1 << 2);
          __ beq(AT, R0, L_skip_align4);
          __ delayed()->nop();
          __ sw(value, to, 0);
          __ daddiu(to, to, 4);
          __ addiu32(count, count, -(4 >> shift));
          __ bind(L_skip_align4);
          break;
        default: ShouldNotReachHere();
      }
    }

    //
    //  Fill large chunks
    //
    __ srl(cnt_words, count, 3 - shift); // number of words
    __ dinsu(value, value, 32, 32);      // 32 bit -> 64 bit
    __ sll(AT, cnt_words, 3 - shift);
    __ subu32(count, count, AT);

    Label L_loop_begin, L_loop_not_64bytes_fill, L_loop_end;
    __ addiu32(AT, cnt_words, -8);
    __ bltz(AT, L_loop_not_64bytes_fill);
    __ delayed()->nop();
    __ bind(L_loop_begin);
    __ sd(value, to,  0);
    __ sd(value, to,  8);
    __ sd(value, to, 16);
    __ sd(value, to, 24);
    __ sd(value, to, 32);
    __ sd(value, to, 40);
    __ sd(value, to, 48);
    __ sd(value, to, 56);
    __ daddiu(to, to, 64);
    __ addiu32(cnt_words, cnt_words, -8);
    __ addiu32(AT, cnt_words, -8);
    __ bgez(AT, L_loop_begin);
    __ delayed()->nop();

    __ bind(L_loop_not_64bytes_fill);
    __ beq(cnt_words, R0, L_loop_end);
    __ delayed()->nop();
    __ sd(value, to, 0);
    __ daddiu(to, to, 8);
    __ addiu32(cnt_words, cnt_words, -1);
    __ b(L_loop_not_64bytes_fill);
    __ delayed()->nop();
    __ bind(L_loop_end);

    // Remaining count is less than 8 bytes. Fill it by a single store.
    // Note that the total length is no less than 8 bytes.
    if (t == T_BYTE || t == T_SHORT) {
      Label L_exit1;
      __ beq(count, R0, L_exit1);
      __ delayed()->nop();
      __ sll(AT, count, shift);
      __ daddu(to, to, AT); // points to the end
      __ sd(value, to, -8);    // overwrite some elements
      __ bind(L_exit1);
      __ leave();
      __ jr(RA);
      __ delayed()->nop();
    }

    // Handle copies less than 8 bytes.
    Label L_fill_2, L_fill_4, L_exit2;
    __ bind(L_fill_elements);
    switch (t) {
      case T_BYTE:
        __ andi(AT, count, 1);
        __ beq(AT, R0, L_fill_2);
        __ delayed()->nop();
        __ sb(value, to, 0);
        __ daddiu(to, to, 1);
        __ bind(L_fill_2);
        __ andi(AT, count, 1 << 1);
        __ beq(AT, R0, L_fill_4);
        __ delayed()->nop();
        __ sh(value, to, 0);
        __ daddiu(to, to, 2);
        __ bind(L_fill_4);
        __ andi(AT, count, 1 << 2);
        __ beq(AT, R0, L_exit2);
        __ delayed()->nop();
        __ sw(value, to, 0);
        break;
      case T_SHORT:
        __ andi(AT, count, 1);
        __ beq(AT, R0, L_fill_4);
        __ delayed()->nop();
        __ sh(value, to, 0);
        __ daddiu(to, to, 2);
        __ bind(L_fill_4);
        __ andi(AT, count, 1 << 1);
        __ beq(AT, R0, L_exit2);
        __ delayed()->nop();
        __ sw(value, to, 0);
        break;
      case T_INT:
        __ beq(count, R0, L_exit2);
        __ delayed()->nop();
        __ sw(value, to, 0);
        break;
      default: ShouldNotReachHere();
    }
    __ bind(L_exit2);
    __ leave();
    __ jr(RA);
    __ delayed()->nop();
    return start;
  }

  // Arguments:
  //   aligned - true => Input and output aligned on a HeapWord == 8-byte boundary
  //             ignored
  //   name    - stub name string
  //
  // Inputs:
  //   c_rarg0   - source array address
  //   c_rarg1   - destination array address
  //   c_rarg2   - element count, treated as ssize_t, can be zero
  //
  // If 'from' and/or 'to' are aligned on 4-, 2-, or 1-byte boundaries,
  // we let the hardware handle it.  The one to eight bytes within words,
  // dwords or qwords that span cache line boundaries will still be loaded
  // and stored atomically.
  //
  // Side Effects:
  //   disjoint_byte_copy_entry is set to the no-overlap entry point
  //   used by generate_conjoint_byte_copy().
  //
  address generate_disjoint_byte_copy(bool aligned, const char * name) {
    StubCodeMark mark(this, "StubRoutines", name);
    __ align(CodeEntryAlignment);


    Register tmp1 = T0;
    Register tmp2 = T1;
    Register tmp3 = T3;

    address start = __ pc();

    __ push(tmp1);
    __ push(tmp2);
    __ push(tmp3);
    __ move(tmp1, A0);
    __ move(tmp2, A1);
    __ move(tmp3, A2);


    Label l_1, l_2, l_3, l_4, l_5, l_6, l_7, l_8, l_9, l_10, l_11;
    Label l_debug;

    __ daddiu(AT, tmp3, -9); //why the number is 9 ?
    __ blez(AT, l_9);
    __ delayed()->nop();

    if (!aligned) {
      __ xorr(AT, tmp1, tmp2);
      __ andi(AT, AT, 1);
      __ bne(AT, R0, l_9); // if arrays don't have the same alignment mod 2, do 1 element copy
      __ delayed()->nop();

      __ andi(AT, tmp1, 1);
      __ beq(AT, R0, l_10); //copy 1 enlement if necessary to aligh to 2 bytes
      __ delayed()->nop();

      __ lb(AT, tmp1, 0);
      __ daddiu(tmp1, tmp1, 1);
      __ sb(AT, tmp2, 0);
      __ daddiu(tmp2, tmp2, 1);
      __ daddiu(tmp3, tmp3, -1);
      __ bind(l_10);

      __ xorr(AT, tmp1, tmp2);
      __ andi(AT, AT, 3);
      __ bne(AT, R0, l_1); // if arrays don't have the same alignment mod 4, do 2 elements copy
      __ delayed()->nop();

      // At this point it is guaranteed that both, from and to have the same alignment mod 4.

      // Copy 2 elements if necessary to align to 4 bytes.
      __ andi(AT, tmp1, 3);
      __ beq(AT, R0, l_2);
      __ delayed()->nop();

      __ lhu(AT, tmp1, 0);
      __ daddiu(tmp1, tmp1, 2);
      __ sh(AT, tmp2, 0);
      __ daddiu(tmp2, tmp2, 2);
      __ daddiu(tmp3, tmp3, -2);
      __ bind(l_2);

      // At this point the positions of both, from and to, are at least 4 byte aligned.

      // Copy 4 elements at a time.
      // Align to 8 bytes, but only if both, from and to, have same alignment mod 8.
      __ xorr(AT, tmp1, tmp2);
      __ andi(AT, AT, 7);
      __ bne(AT, R0, l_6); // not same alignment mod 8 -> copy 2, either from or to will be unaligned
      __ delayed()->nop();

      // Copy a 4 elements if necessary to align to 8 bytes.
      __ andi(AT, tmp1, 7);
      __ beq(AT, R0, l_7);
      __ delayed()->nop();

      __ lw(AT, tmp1, 0);
      __ daddiu(tmp3, tmp3, -4);
      __ sw(AT, tmp2, 0);
      { // FasterArrayCopy
        __ daddiu(tmp1, tmp1, 4);
        __ daddiu(tmp2, tmp2, 4);
      }
    }

    __ bind(l_7);

    // Copy 4 elements at a time; either the loads or the stores can
    // be unaligned if aligned == false.

    { // FasterArrayCopy
      __ daddiu(AT, tmp3, -7);
      __ blez(AT, l_6); // copy 4 at a time if less than 4 elements remain
      __ delayed()->nop();

      __ bind(l_8);
      // For Loongson, there is 128-bit memory access. TODO
      __ ld(AT, tmp1, 0);
      __ sd(AT, tmp2, 0);
      __ daddiu(tmp1, tmp1, 8);
      __ daddiu(tmp2, tmp2, 8);
      __ daddiu(tmp3, tmp3, -8);
      __ daddiu(AT, tmp3, -8);
      __ bgez(AT, l_8);
      __ delayed()->nop();
    }
    __ bind(l_6);

    // copy 4 bytes at a time
    { // FasterArrayCopy
      __ daddiu(AT, tmp3, -3);
      __ blez(AT, l_1);
      __ delayed()->nop();

      __ bind(l_3);
      __ lw(AT, tmp1, 0);
      __ sw(AT, tmp2, 0);
      __ daddiu(tmp1, tmp1, 4);
      __ daddiu(tmp2, tmp2, 4);
      __ daddiu(tmp3, tmp3, -4);
      __ daddiu(AT, tmp3, -4);
      __ bgez(AT, l_3);
      __ delayed()->nop();

    }

    // do 2 bytes copy
    __ bind(l_1);
    {
      __ daddiu(AT, tmp3, -1);
      __ blez(AT, l_9);
      __ delayed()->nop();

      __ bind(l_5);
      __ lhu(AT, tmp1, 0);
      __ daddiu(tmp3, tmp3, -2);
      __ sh(AT, tmp2, 0);
      __ daddiu(tmp1, tmp1, 2);
      __ daddiu(tmp2, tmp2, 2);
      __ daddiu(AT, tmp3, -2);
      __ bgez(AT, l_5);
      __ delayed()->nop();
    }

    //do 1 element copy--byte
    __ bind(l_9);
    __ beq(R0, tmp3, l_4);
    __ delayed()->nop();

    {
      __ bind(l_11);
      __ lb(AT, tmp1, 0);
      __ daddiu(tmp3, tmp3, -1);
      __ sb(AT, tmp2, 0);
      __ daddiu(tmp1, tmp1, 1);
      __ daddiu(tmp2, tmp2, 1);
      __ daddiu(AT, tmp3, -1);
      __ bgez(AT, l_11);
      __ delayed()->nop();
    }

    __ bind(l_4);
    __ pop(tmp3);
    __ pop(tmp2);
    __ pop(tmp1);

    __ jr(RA);
    __ delayed()->nop();

    return start;
  }

  // Arguments:
  //   aligned - true => Input and output aligned on a HeapWord == 8-byte boundary
  //             ignored
  //   name    - stub name string
  //
  // Inputs:
  //   A0   - source array address
  //   A1   - destination array address
  //   A2   - element count, treated as ssize_t, can be zero
  //
  // If 'from' and/or 'to' are aligned on 4-, 2-, or 1-byte boundaries,
  // we let the hardware handle it.  The one to eight bytes within words,
  // dwords or qwords that span cache line boundaries will still be loaded
  // and stored atomically.
  //
  address generate_conjoint_byte_copy(bool aligned, const char *name) {
    __ align(CodeEntryAlignment);
    StubCodeMark mark(this, "StubRoutines", name);
    address start = __ pc();

    Label l_copy_4_bytes_loop, l_copy_suffix, l_copy_suffix_loop, l_exit;
    Label l_copy_byte, l_from_unaligned, l_unaligned, l_4_bytes_aligned;

    address nooverlap_target = aligned ?
      StubRoutines::arrayof_jbyte_disjoint_arraycopy() :
      StubRoutines::jbyte_disjoint_arraycopy();

    array_overlap_test(nooverlap_target, 0);

    const Register from      = A0;   // source array address
    const Register to        = A1;   // destination array address
    const Register count     = A2;   // elements count
    const Register end_from  = T3;   // source array end address
    const Register end_to    = T0;   // destination array end address
    const Register end_count = T1;   // destination array end address

    __ push(end_from);
    __ push(end_to);
    __ push(end_count);
    __ push(T8);

    // copy from high to low
    __ move(end_count, count);
    __ daddu(end_from, from, end_count);
    __ daddu(end_to, to, end_count);

    // If end_from and end_to has differante alignment, unaligned copy is performed.
    __ andi(AT, end_from, 3);
    __ andi(T8, end_to, 3);
    __ bne(AT, T8, l_copy_byte);
    __ delayed()->nop();

    // First deal with the unaligned data at the top.
    __ bind(l_unaligned);
    __ beq(end_count, R0, l_exit);
    __ delayed()->nop();

    __ andi(AT, end_from, 3);
    __ bne(AT, R0, l_from_unaligned);
    __ delayed()->nop();

    __ andi(AT, end_to, 3);
    __ beq(AT, R0, l_4_bytes_aligned);
    __ delayed()->nop();

    __ bind(l_from_unaligned);
    __ lb(AT, end_from, -1);
    __ sb(AT, end_to, -1);
    __ daddiu(end_from, end_from, -1);
    __ daddiu(end_to, end_to, -1);
    __ daddiu(end_count, end_count, -1);
    __ b(l_unaligned);
    __ delayed()->nop();

    // now end_to, end_from point to 4-byte aligned high-ends
    //     end_count contains byte count that is not copied.
    // copy 4 bytes at a time
    __ bind(l_4_bytes_aligned);

    __ move(T8, end_count);
    __ daddiu(AT, end_count, -3);
    __ blez(AT, l_copy_suffix);
    __ delayed()->nop();

    //__ andi(T8, T8, 3);
    __ lea(end_from, Address(end_from, -4));
    __ lea(end_to, Address(end_to, -4));

    __ dsrl(end_count, end_count, 2);
    __ align(16);
    __ bind(l_copy_4_bytes_loop); //l_copy_4_bytes
    __ lw(AT, end_from, 0);
    __ sw(AT, end_to, 0);
    __ addiu(end_from, end_from, -4);
    __ addiu(end_to, end_to, -4);
    __ addiu(end_count, end_count, -1);
    __ bne(end_count, R0, l_copy_4_bytes_loop);
    __ delayed()->nop();

    __ b(l_copy_suffix);
    __ delayed()->nop();
    // copy dwords aligned or not with repeat move
    // l_copy_suffix
    // copy suffix (0-3 bytes)
    __ bind(l_copy_suffix);
    __ andi(T8, T8, 3);
    __ beq(T8, R0, l_exit);
    __ delayed()->nop();
    __ addiu(end_from, end_from, 3);
    __ addiu(end_to, end_to, 3);
    __ bind(l_copy_suffix_loop);
    __ lb(AT, end_from, 0);
    __ sb(AT, end_to, 0);
    __ addiu(end_from, end_from, -1);
    __ addiu(end_to, end_to, -1);
    __ addiu(T8, T8, -1);
    __ bne(T8, R0, l_copy_suffix_loop);
    __ delayed()->nop();

    __ bind(l_copy_byte);
    __ beq(end_count, R0, l_exit);
    __ delayed()->nop();
    __ lb(AT, end_from, -1);
    __ sb(AT, end_to, -1);
    __ daddiu(end_from, end_from, -1);
    __ daddiu(end_to, end_to, -1);
    __ daddiu(end_count, end_count, -1);
    __ b(l_copy_byte);
    __ delayed()->nop();

    __ bind(l_exit);
    __ pop(T8);
    __ pop(end_count);
    __ pop(end_to);
    __ pop(end_from);
    __ jr(RA);
    __ delayed()->nop();
    return start;
  }

  // Generate stub for disjoint short copy.  If "aligned" is true, the
  // "from" and "to" addresses are assumed to be heapword aligned.
  //
  // Arguments for generated stub:
  //      from:  A0
  //      to:    A1
  //  elm.count: A2 treated as signed
  //  one element: 2 bytes
  //
  // Strategy for aligned==true:
  //
  //  If length <= 9:
  //     1. copy 1 elements at a time (l_5)
  //
  //  If length > 9:
  //     1. copy 4 elements at a time until less than 4 elements are left (l_7)
  //     2. copy 2 elements at a time until less than 2 elements are left (l_6)
  //     3. copy last element if one was left in step 2. (l_1)
  //
  //
  // Strategy for aligned==false:
  //
  //  If length <= 9: same as aligned==true case
  //
  //  If length > 9:
  //     1. continue with step 7. if the alignment of from and to mod 4
  //        is different.
  //     2. align from and to to 4 bytes by copying 1 element if necessary
  //     3. at l_2 from and to are 4 byte aligned; continue with
  //        6. if they cannot be aligned to 8 bytes because they have
  //        got different alignment mod 8.
  //     4. at this point we know that both, from and to, have the same
  //        alignment mod 8, now copy one element if necessary to get
  //        8 byte alignment of from and to.
  //     5. copy 4 elements at a time until less than 4 elements are
  //        left; depending on step 3. all load/stores are aligned.
  //     6. copy 2 elements at a time until less than 2 elements are
  //        left. (l_6)
  //     7. copy 1 element at a time. (l_5)
  //     8. copy last element if one was left in step 6. (l_1)

  address generate_disjoint_short_copy(bool aligned, const char * name) {
    StubCodeMark mark(this, "StubRoutines", name);
    __ align(CodeEntryAlignment);

    Register tmp1 = T0;
    Register tmp2 = T1;
    Register tmp3 = T3;
    Register tmp4 = T8;
    Register tmp5 = T9;
    Register tmp6 = T2;

    address start = __ pc();

    __ push(tmp1);
    __ push(tmp2);
    __ push(tmp3);
    __ move(tmp1, A0);
    __ move(tmp2, A1);
    __ move(tmp3, A2);

    Label l_1, l_2, l_3, l_4, l_5, l_6, l_7, l_8, l_9, l_10, l_11, l_12, l_13, l_14;
    Label l_debug;
    // don't try anything fancy if arrays don't have many elements
    __ daddiu(AT, tmp3, -23);
    __ blez(AT, l_14);
    __ delayed()->nop();
    // move push here
    __ push(tmp4);
    __ push(tmp5);
    __ push(tmp6);

    if (!aligned) {
      __ xorr(AT, A0, A1);
      __ andi(AT, AT, 1);
      __ bne(AT, R0, l_debug); // if arrays don't have the same alignment mod 2, can this happen?
      __ delayed()->nop();

      __ xorr(AT, A0, A1);
      __ andi(AT, AT, 3);
      __ bne(AT, R0, l_1); // if arrays don't have the same alignment mod 4, do 1 element copy
      __ delayed()->nop();

      // At this point it is guaranteed that both, from and to have the same alignment mod 4.

      // Copy 1 element if necessary to align to 4 bytes.
      __ andi(AT, A0, 3);
      __ beq(AT, R0, l_2);
      __ delayed()->nop();

      __ lhu(AT, tmp1, 0);
      __ daddiu(tmp1, tmp1, 2);
      __ sh(AT, tmp2, 0);
      __ daddiu(tmp2, tmp2, 2);
      __ daddiu(tmp3, tmp3, -1);
      __ bind(l_2);

      // At this point the positions of both, from and to, are at least 4 byte aligned.

      // Copy 4 elements at a time.
      // Align to 8 bytes, but only if both, from and to, have same alignment mod 8.
      __ xorr(AT, tmp1, tmp2);
      __ andi(AT, AT, 7);
      __ bne(AT, R0, l_6); // not same alignment mod 8 -> copy 2, either from or to will be unaligned
      __ delayed()->nop();

      // Copy a 2-element word if necessary to align to 8 bytes.
      __ andi(AT, tmp1, 7);
      __ beq(AT, R0, l_7);
      __ delayed()->nop();

      __ lw(AT, tmp1, 0);
      __ daddiu(tmp3, tmp3, -2);
      __ sw(AT, tmp2, 0);
      __ daddiu(tmp1, tmp1, 4);
      __ daddiu(tmp2, tmp2, 4);
    }// end of if (!aligned)

    __ bind(l_7);
    // At this time the position of both, from and to, are at least 8 byte aligned.
    // Copy 8 elemnets at a time.
    // Align to 16 bytes, but only if both from and to have same alignment mod 8.
    __ xorr(AT, tmp1, tmp2);
    __ andi(AT, AT, 15);
    __ bne(AT, R0, l_9);
    __ delayed()->nop();

    // Copy 4-element word if necessary to align to 16 bytes,
    __ andi(AT, tmp1, 15);
    __ beq(AT, R0, l_10);
    __ delayed()->nop();

    __ ld(AT, tmp1, 0);
    __ daddiu(tmp3, tmp3, -4);
    __ sd(AT, tmp2, 0);
    __ daddiu(tmp1, tmp1, 8);
    __ daddiu(tmp2, tmp2, 8);

    __ bind(l_10);

    // Copy 8 elements at a time; either the loads or the stores can
    // be unalligned if aligned == false

    { // FasterArrayCopy
      __ bind(l_11);
      // For loongson the 128-bit memory access instruction is gslq/gssq
      if (UseLEXT1) {
        __ gslq(AT, tmp4, tmp1, 0);
        __ gslq(tmp5, tmp6, tmp1, 16);
        __ daddiu(tmp1, tmp1, 32);
        __ daddiu(tmp2, tmp2, 32);
        __ gssq(AT, tmp4, tmp2, -32);
        __ gssq(tmp5, tmp6, tmp2, -16);
      } else {
        __ ld(AT, tmp1, 0);
        __ ld(tmp4, tmp1, 8);
        __ ld(tmp5, tmp1, 16);
        __ ld(tmp6, tmp1, 24);
        __ daddiu(tmp1, tmp1, 32);
        __ sd(AT, tmp2, 0);
        __ sd(tmp4, tmp2, 8);
        __ sd(tmp5, tmp2, 16);
        __ sd(tmp6, tmp2, 24);
        __ daddiu(tmp2, tmp2, 32);
      }
      __ daddiu(tmp3, tmp3, -16);
      __ daddiu(AT, tmp3, -16);
      __ bgez(AT, l_11);
      __ delayed()->nop();
    }
    __ bind(l_9);

    // Copy 4 elements at a time; either the loads or the stores can
    // be unaligned if aligned == false.
    { // FasterArrayCopy
      __ daddiu(AT, tmp3, -15);// loop unrolling 4 times, so if the elements should not be less than 16
      __ blez(AT, l_4); // copy 2 at a time if less than 16 elements remain
      __ delayed()->nop();

      __ bind(l_8);
      __ ld(AT, tmp1, 0);
      __ ld(tmp4, tmp1, 8);
      __ ld(tmp5, tmp1, 16);
      __ ld(tmp6, tmp1, 24);
      __ sd(AT, tmp2, 0);
      __ sd(tmp4, tmp2, 8);
      __ sd(tmp5, tmp2,16);
      __ daddiu(tmp1, tmp1, 32);
      __ daddiu(tmp2, tmp2, 32);
      __ daddiu(tmp3, tmp3, -16);
      __ daddiu(AT, tmp3, -16);
      __ bgez(AT, l_8);
      __ delayed()->sd(tmp6, tmp2, -8);
    }
    __ bind(l_6);

    // copy 2 element at a time
    { // FasterArrayCopy
      __ daddiu(AT, tmp3, -7);
      __ blez(AT, l_4);
      __ delayed()->nop();

      __ bind(l_3);
      __ lw(AT, tmp1, 0);
      __ lw(tmp4, tmp1, 4);
      __ lw(tmp5, tmp1, 8);
      __ lw(tmp6, tmp1, 12);
      __ sw(AT, tmp2, 0);
      __ sw(tmp4, tmp2, 4);
      __ sw(tmp5, tmp2, 8);
      __ daddiu(tmp1, tmp1, 16);
      __ daddiu(tmp2, tmp2, 16);
      __ daddiu(tmp3, tmp3, -8);
      __ daddiu(AT, tmp3, -8);
      __ bgez(AT, l_3);
      __ delayed()->sw(tmp6, tmp2, -4);
    }

    __ bind(l_1);
    // do single element copy (8 bit), can this happen?
    { // FasterArrayCopy
      __ daddiu(AT, tmp3, -3);
      __ blez(AT, l_4);
      __ delayed()->nop();

      __ bind(l_5);
      __ lhu(AT, tmp1, 0);
      __ lhu(tmp4, tmp1, 2);
      __ lhu(tmp5, tmp1, 4);
      __ lhu(tmp6, tmp1, 6);
      __ sh(AT, tmp2, 0);
      __ sh(tmp4, tmp2, 2);
      __ sh(tmp5, tmp2, 4);
      __ daddiu(tmp1, tmp1, 8);
      __ daddiu(tmp2, tmp2, 8);
      __ daddiu(tmp3, tmp3, -4);
      __ daddiu(AT, tmp3, -4);
      __ bgez(AT, l_5);
      __ delayed()->sh(tmp6, tmp2, -2);
    }
    // single element
    __ bind(l_4);

    __ pop(tmp6);
    __ pop(tmp5);
    __ pop(tmp4);

    __ bind(l_14);
    { // FasterArrayCopy
      __ beq(R0, tmp3, l_13);
      __ delayed()->nop();

      __ bind(l_12);
      __ lhu(AT, tmp1, 0);
      __ sh(AT, tmp2, 0);
      __ daddiu(tmp1, tmp1, 2);
      __ daddiu(tmp2, tmp2, 2);
      __ daddiu(tmp3, tmp3, -1);
      __ daddiu(AT, tmp3, -1);
      __ bgez(AT, l_12);
      __ delayed()->nop();
    }

    __ bind(l_13);
    __ pop(tmp3);
    __ pop(tmp2);
    __ pop(tmp1);

    __ jr(RA);
    __ delayed()->nop();

    __ bind(l_debug);
    __ stop("generate_disjoint_short_copy should not reach here");
    return start;
  }

  // Arguments:
  //   aligned - true => Input and output aligned on a HeapWord == 8-byte boundary
  //             ignored
  //   name    - stub name string
  //
  // Inputs:
  //   c_rarg0   - source array address
  //   c_rarg1   - destination array address
  //   c_rarg2   - element count, treated as ssize_t, can be zero
  //
  // If 'from' and/or 'to' are aligned on 4- or 2-byte boundaries, we
  // let the hardware handle it.  The two or four words within dwords
  // or qwords that span cache line boundaries will still be loaded
  // and stored atomically.
  //
  address generate_conjoint_short_copy(bool aligned, const char *name) {
    StubCodeMark mark(this, "StubRoutines", name);
    __ align(CodeEntryAlignment);
    address start = __ pc();

    Label l_exit, l_copy_short, l_from_unaligned, l_unaligned, l_4_bytes_aligned;

    address nooverlap_target = aligned ?
            StubRoutines::arrayof_jshort_disjoint_arraycopy() :
            StubRoutines::jshort_disjoint_arraycopy();

    array_overlap_test(nooverlap_target, 1);

    const Register from      = A0;   // source array address
    const Register to        = A1;   // destination array address
    const Register count     = A2;   // elements count
    const Register end_from  = T3;   // source array end address
    const Register end_to    = T0;   // destination array end address
    const Register end_count = T1;   // destination array end address

    __ push(end_from);
    __ push(end_to);
    __ push(end_count);
    __ push(T8);

    // copy from high to low
    __ move(end_count, count);
    __ sll(AT, end_count, Address::times_2);
    __ daddu(end_from, from, AT);
    __ daddu(end_to, to, AT);

    // If end_from and end_to has differante alignment, unaligned copy is performed.
    __ andi(AT, end_from, 3);
    __ andi(T8, end_to, 3);
    __ bne(AT, T8, l_copy_short);
    __ delayed()->nop();

    // First deal with the unaligned data at the top.
    __ bind(l_unaligned);
    __ beq(end_count, R0, l_exit);
    __ delayed()->nop();

    __ andi(AT, end_from, 3);
    __ bne(AT, R0, l_from_unaligned);
    __ delayed()->nop();

    __ andi(AT, end_to, 3);
    __ beq(AT, R0, l_4_bytes_aligned);
    __ delayed()->nop();

    // Copy 1 element if necessary to align to 4 bytes.
    __ bind(l_from_unaligned);
    __ lhu(AT, end_from, -2);
    __ sh(AT, end_to, -2);
    __ daddiu(end_from, end_from, -2);
    __ daddiu(end_to, end_to, -2);
    __ daddiu(end_count, end_count, -1);
    __ b(l_unaligned);
    __ delayed()->nop();

    // now end_to, end_from point to 4-byte aligned high-ends
    //     end_count contains byte count that is not copied.
    // copy 4 bytes at a time
    __ bind(l_4_bytes_aligned);

    __ daddiu(AT, end_count, -1);
    __ blez(AT, l_copy_short);
    __ delayed()->nop();

    __ lw(AT, end_from, -4);
    __ sw(AT, end_to, -4);
    __ addiu(end_from, end_from, -4);
    __ addiu(end_to, end_to, -4);
    __ addiu(end_count, end_count, -2);
    __ b(l_4_bytes_aligned);
    __ delayed()->nop();

    // copy 1 element at a time
    __ bind(l_copy_short);
    __ beq(end_count, R0, l_exit);
    __ delayed()->nop();
    __ lhu(AT, end_from, -2);
    __ sh(AT, end_to, -2);
    __ daddiu(end_from, end_from, -2);
    __ daddiu(end_to, end_to, -2);
    __ daddiu(end_count, end_count, -1);
    __ b(l_copy_short);
    __ delayed()->nop();

    __ bind(l_exit);
    __ pop(T8);
    __ pop(end_count);
    __ pop(end_to);
    __ pop(end_from);
    __ jr(RA);
    __ delayed()->nop();
    return start;
  }

  // Arguments:
  //   aligned - true => Input and output aligned on a HeapWord == 8-byte boundary
  //             ignored
  //   is_oop  - true => oop array, so generate store check code
  //   name    - stub name string
  //
  // Inputs:
  //   c_rarg0   - source array address
  //   c_rarg1   - destination array address
  //   c_rarg2   - element count, treated as ssize_t, can be zero
  //
  // If 'from' and/or 'to' are aligned on 4-byte boundaries, we let
  // the hardware handle it.  The two dwords within qwords that span
  // cache line boundaries will still be loaded and stored atomicly.
  //
  // Side Effects:
  //   disjoint_int_copy_entry is set to the no-overlap entry point
  //   used by generate_conjoint_int_oop_copy().
  //
  address generate_disjoint_int_oop_copy(bool aligned, bool is_oop, const char *name, bool dest_uninitialized = false) {
    Label l_3, l_4, l_5, l_6, l_7;
    StubCodeMark mark(this, "StubRoutines", name);

    __ align(CodeEntryAlignment);
    address start = __ pc();
    __ push(T3);
    __ push(T0);
    __ push(T1);
    __ push(T8);
    __ push(T9);
    __ move(T1, A2);
    __ move(T3, A0);
    __ move(T0, A1);

    DecoratorSet decorators = IN_HEAP | IS_ARRAY | ARRAYCOPY_DISJOINT;
    if (dest_uninitialized) {
      decorators |= IS_DEST_UNINITIALIZED;
    }
    if (aligned) {
      decorators |= ARRAYCOPY_ALIGNED;
    }

    BarrierSetAssembler *bs = BarrierSet::barrier_set()->barrier_set_assembler();
    bs->arraycopy_prologue(_masm, decorators, is_oop, A1, A2);

    if(!aligned) {
      __ xorr(AT, T3, T0);
      __ andi(AT, AT, 7);
      __ bne(AT, R0, l_5); // not same alignment mod 8 -> copy 1 element each time
      __ delayed()->nop();

      __ andi(AT, T3, 7);
      __ beq(AT, R0, l_6); //copy 2 elements each time
      __ delayed()->nop();

      __ lw(AT, T3, 0);
      __ daddiu(T1, T1, -1);
      __ sw(AT, T0, 0);
      __ daddiu(T3, T3, 4);
      __ daddiu(T0, T0, 4);
    }

    {
      __ bind(l_6);
      __ daddiu(AT, T1, -1);
      __ blez(AT, l_5);
      __ delayed()->nop();

      __ bind(l_7);
      __ ld(AT, T3, 0);
      __ sd(AT, T0, 0);
      __ daddiu(T3, T3, 8);
      __ daddiu(T0, T0, 8);
      __ daddiu(T1, T1, -2);
      __ daddiu(AT, T1, -2);
      __ bgez(AT, l_7);
      __ delayed()->nop();
    }

    __ bind(l_5);
    __ beq(T1, R0, l_4);
    __ delayed()->nop();

    __ align(16);
    __ bind(l_3);
    __ lw(AT, T3, 0);
    __ sw(AT, T0, 0);
    __ addiu(T3, T3, 4);
    __ addiu(T0, T0, 4);
    __ addiu(T1, T1, -1);
    __ bne(T1, R0, l_3);
    __ delayed()->nop();

    // exit
    __ bind(l_4);
    bs->arraycopy_epilogue(_masm, decorators, is_oop, A1, A2, T1);
    __ pop(T9);
    __ pop(T8);
    __ pop(T1);
    __ pop(T0);
    __ pop(T3);
    __ jr(RA);
    __ delayed()->nop();

    return start;
  }

  // Arguments:
  //   aligned - true => Input and output aligned on a HeapWord == 8-byte boundary
  //             ignored
  //   is_oop  - true => oop array, so generate store check code
  //   name    - stub name string
  //
  // Inputs:
  //   c_rarg0   - source array address
  //   c_rarg1   - destination array address
  //   c_rarg2   - element count, treated as ssize_t, can be zero
  //
  // If 'from' and/or 'to' are aligned on 4-byte boundaries, we let
  // the hardware handle it.  The two dwords within qwords that span
  // cache line boundaries will still be loaded and stored atomicly.
  //
  address generate_conjoint_int_oop_copy(bool aligned, bool is_oop, const char *name, bool dest_uninitialized = false) {
    Label l_2, l_4;
    StubCodeMark mark(this, "StubRoutines", name);
    __ align(CodeEntryAlignment);
    address start = __ pc();
    address nooverlap_target;

    if (is_oop) {
      nooverlap_target = aligned ?
              StubRoutines::arrayof_oop_disjoint_arraycopy() :
              StubRoutines::oop_disjoint_arraycopy();
    } else {
      nooverlap_target = aligned ?
              StubRoutines::arrayof_jint_disjoint_arraycopy() :
              StubRoutines::jint_disjoint_arraycopy();
    }

    array_overlap_test(nooverlap_target, 2);

    DecoratorSet decorators = IN_HEAP | IS_ARRAY;
    if (dest_uninitialized) {
      decorators |= IS_DEST_UNINITIALIZED;
    }
    if (aligned) {
      decorators |= ARRAYCOPY_ALIGNED;
    }

    BarrierSetAssembler *bs = BarrierSet::barrier_set()->barrier_set_assembler();
    // no registers are destroyed by this call
    bs->arraycopy_prologue(_masm, decorators, is_oop, A1, A2);

    __ push(T3);
    __ push(T0);
    __ push(T1);
    __ push(T8);
    __ push(T9);

    __ move(T1, A2);
    __ move(T3, A0);
    __ move(T0, A1);

    // T3: source array address
    // T0: destination array address
    // T1: element count

    __ sll(AT, T1, Address::times_4);
    __ addu(AT, T3, AT);
    __ daddiu(T3, AT, -4);
    __ sll(AT, T1, Address::times_4);
    __ addu(AT, T0, AT);
    __ daddiu(T0, AT, -4);

    __ beq(T1, R0, l_4);
    __ delayed()->nop();

    __ align(16);
    __ bind(l_2);
    __ lw(AT, T3, 0);
    __ sw(AT, T0, 0);
    __ addiu(T3, T3, -4);
    __ addiu(T0, T0, -4);
    __ addiu(T1, T1, -1);
    __ bne(T1, R0, l_2);
    __ delayed()->nop();

    __ bind(l_4);
    bs->arraycopy_epilogue(_masm, decorators, is_oop, A1, A2, T1);
    __ pop(T9);
    __ pop(T8);
    __ pop(T1);
    __ pop(T0);
    __ pop(T3);
    __ jr(RA);
    __ delayed()->nop();

    return start;
  }

  // Arguments:
  //   aligned - true => Input and output aligned on a HeapWord == 8-byte boundary
  //             ignored
  //   is_oop  - true => oop array, so generate store check code
  //   name    - stub name string
  //
  // Inputs:
  //   c_rarg0   - source array address
  //   c_rarg1   - destination array address
  //   c_rarg2   - element count, treated as ssize_t, can be zero
  //
  // If 'from' and/or 'to' are aligned on 4-byte boundaries, we let
  // the hardware handle it.  The two dwords within qwords that span
  // cache line boundaries will still be loaded and stored atomicly.
  //
  // Side Effects:
  //   disjoint_int_copy_entry is set to the no-overlap entry point
  //   used by generate_conjoint_int_oop_copy().
  //
  address generate_disjoint_long_oop_copy(bool aligned, bool is_oop, const char *name, bool dest_uninitialized = false) {
    Label l_3, l_4;
    StubCodeMark mark(this, "StubRoutines", name);
    __ align(CodeEntryAlignment);
    address start = __ pc();

    DecoratorSet decorators = IN_HEAP | IS_ARRAY | ARRAYCOPY_DISJOINT;
    if (dest_uninitialized) {
      decorators |= IS_DEST_UNINITIALIZED;
    }
    if (aligned) {
      decorators |= ARRAYCOPY_ALIGNED;
    }

    BarrierSetAssembler *bs = BarrierSet::barrier_set()->barrier_set_assembler();
    bs->arraycopy_prologue(_masm, decorators, is_oop, A1, A2);

    __ push(T3);
    __ push(T0);
    __ push(T1);
    __ push(T8);
    __ push(T9);

    __ move(T1, A2);
    __ move(T3, A0);
    __ move(T0, A1);

    // T3: source array address
    // T0: destination array address
    // T1: element count

    __ beq(T1, R0, l_4);
    __ delayed()->nop();

    __ align(16);
    __ bind(l_3);
    __ ld(AT, T3, 0);
    __ sd(AT, T0, 0);
    __ addiu(T3, T3, 8);
    __ addiu(T0, T0, 8);
    __ addiu(T1, T1, -1);
    __ bne(T1, R0, l_3);
    __ delayed()->nop();

    // exit
    __ bind(l_4);
    bs->arraycopy_epilogue(_masm, decorators, is_oop, A1, A2, T1);
    __ pop(T9);
    __ pop(T8);
    __ pop(T1);
    __ pop(T0);
    __ pop(T3);
    __ jr(RA);
    __ delayed()->nop();
    return start;
  }

  // Arguments:
  //   aligned - true => Input and output aligned on a HeapWord == 8-byte boundary
  //             ignored
  //   is_oop  - true => oop array, so generate store check code
  //   name    - stub name string
  //
  // Inputs:
  //   c_rarg0   - source array address
  //   c_rarg1   - destination array address
  //   c_rarg2   - element count, treated as ssize_t, can be zero
  //
  // If 'from' and/or 'to' are aligned on 4-byte boundaries, we let
  // the hardware handle it.  The two dwords within qwords that span
  // cache line boundaries will still be loaded and stored atomicly.
  //
  address generate_conjoint_long_oop_copy(bool aligned, bool is_oop, const char *name, bool dest_uninitialized = false) {
    Label l_2, l_4;
    StubCodeMark mark(this, "StubRoutines", name);
    __ align(CodeEntryAlignment);
    address start = __ pc();
    address nooverlap_target;

    if (is_oop) {
      nooverlap_target = aligned ?
              StubRoutines::arrayof_oop_disjoint_arraycopy() :
              StubRoutines::oop_disjoint_arraycopy();
    } else {
      nooverlap_target = aligned ?
              StubRoutines::arrayof_jlong_disjoint_arraycopy() :
              StubRoutines::jlong_disjoint_arraycopy();
    }

    array_overlap_test(nooverlap_target, 3);

    DecoratorSet decorators = IN_HEAP | IS_ARRAY;
    if (dest_uninitialized) {
      decorators |= IS_DEST_UNINITIALIZED;
    }
    if (aligned) {
      decorators |= ARRAYCOPY_ALIGNED;
    }

    BarrierSetAssembler *bs = BarrierSet::barrier_set()->barrier_set_assembler();
    bs->arraycopy_prologue(_masm, decorators, is_oop, A1, A2);

    __ push(T3);
    __ push(T0);
    __ push(T1);
    __ push(T8);
    __ push(T9);

    __ move(T1, A2);
    __ move(T3, A0);
    __ move(T0, A1);

    __ sll(AT, T1, Address::times_8);
    __ addu(AT, T3, AT);
    __ daddiu(T3, AT, -8);
    __ sll(AT, T1, Address::times_8);
    __ addu(AT, T0, AT);
    __ daddiu(T0, AT, -8);

    __ beq(T1, R0, l_4);
    __ delayed()->nop();

    __ align(16);
    __ bind(l_2);
    __ ld(AT, T3, 0);
    __ sd(AT, T0, 0);
    __ addiu(T3, T3, -8);
    __ addiu(T0, T0, -8);
    __ addiu(T1, T1, -1);
    __ bne(T1, R0, l_2);
    __ delayed()->nop();

    // exit
    __ bind(l_4);
    bs->arraycopy_epilogue(_masm, decorators, is_oop, A1, A2, T1);
    __ pop(T9);
    __ pop(T8);
    __ pop(T1);
    __ pop(T0);
    __ pop(T3);
    __ jr(RA);
    __ delayed()->nop();
    return start;
  }

  //FIXME
  address generate_disjoint_long_copy(bool aligned, const char *name) {
    Label l_1, l_2;
    StubCodeMark mark(this, "StubRoutines", name);
    __ align(CodeEntryAlignment);
    address start = __ pc();

    __ move(T1, A2);
    __ move(T3, A0);
    __ move(T0, A1);
    __ push(T3);
    __ push(T0);
    __ push(T1);
    __ b(l_2);
    __ delayed()->nop();
    __ align(16);
    __ bind(l_1);
    {
      UnsafeCopyMemoryMark ucmm(this, true, true);
      __ ld(AT, T3, 0);
      __ sd (AT, T0, 0);
      __ addiu(T3, T3, 8);
      __ addiu(T0, T0, 8);
      __ bind(l_2);
      __ addiu(T1, T1, -1);
      __ bgez(T1, l_1);
      __ delayed()->nop();
    }
    __ pop(T1);
    __ pop(T0);
    __ pop(T3);
    __ jr(RA);
    __ delayed()->nop();
    return start;
  }


  address generate_conjoint_long_copy(bool aligned, const char *name) {
    Label l_1, l_2;
    StubCodeMark mark(this, "StubRoutines", name);
    __ align(CodeEntryAlignment);
    address start = __ pc();
    address nooverlap_target = aligned ?
      StubRoutines::arrayof_jlong_disjoint_arraycopy() :
      StubRoutines::jlong_disjoint_arraycopy();
    array_overlap_test(nooverlap_target, 3);

    __ push(T3);
    __ push(T0);
    __ push(T1);

    __ move(T1, A2);
    __ move(T3, A0);
    __ move(T0, A1);
    __ sll(AT, T1, Address::times_8);
    __ addu(AT, T3, AT);
    __ daddiu(T3, AT, -8);
    __ sll(AT, T1, Address::times_8);
    __ addu(AT, T0, AT);
    __ daddiu(T0, AT, -8);

    __ b(l_2);
    __ delayed()->nop();
    __ align(16);
    __ bind(l_1);
    {
      UnsafeCopyMemoryMark ucmm(this, true, true);
      __ ld(AT, T3, 0);
      __ sd (AT, T0, 0);
      __ addiu(T3, T3, -8);
      __ addiu(T0, T0,-8);
      __ bind(l_2);
      __ addiu(T1, T1, -1);
      __ bgez(T1, l_1);
      __ delayed()->nop();
    }
    __ pop(T1);
    __ pop(T0);
    __ pop(T3);
    __ jr(RA);
    __ delayed()->nop();
    return start;
  }

  void generate_arraycopy_stubs() {
    if (UseCompressedOops) {
      StubRoutines::_oop_disjoint_arraycopy          = generate_disjoint_int_oop_copy(false, true,
                                                                                      "oop_disjoint_arraycopy");
      StubRoutines::_oop_arraycopy                   = generate_conjoint_int_oop_copy(false, true,
                                                                                      "oop_arraycopy");
      StubRoutines::_oop_disjoint_arraycopy_uninit   = generate_disjoint_int_oop_copy(false, true,
                                                                                      "oop_disjoint_arraycopy_uninit", true);
      StubRoutines::_oop_arraycopy_uninit            = generate_conjoint_int_oop_copy(false, true,
                                                                                      "oop_arraycopy_uninit", true);
    } else {
      StubRoutines::_oop_disjoint_arraycopy          = generate_disjoint_long_oop_copy(false, true,
                                                                                       "oop_disjoint_arraycopy");
      StubRoutines::_oop_arraycopy                   = generate_conjoint_long_oop_copy(false, true,
                                                                                       "oop_arraycopy");
      StubRoutines::_oop_disjoint_arraycopy_uninit   = generate_disjoint_long_oop_copy(false, true,
                                                                                       "oop_disjoint_arraycopy_uninit", true);
      StubRoutines::_oop_arraycopy_uninit            = generate_conjoint_long_oop_copy(false, true,
                                                                                       "oop_arraycopy_uninit", true);
    }

    StubRoutines::_jbyte_disjoint_arraycopy          = generate_disjoint_byte_copy(false, "jbyte_disjoint_arraycopy");
    StubRoutines::_jshort_disjoint_arraycopy         = generate_disjoint_short_copy(false, "jshort_disjoint_arraycopy");
    StubRoutines::_jint_disjoint_arraycopy           = generate_disjoint_int_oop_copy(false, false, "jint_disjoint_arraycopy");
    StubRoutines::_jlong_disjoint_arraycopy          = generate_disjoint_long_copy(false, "jlong_disjoint_arraycopy");

    StubRoutines::_jbyte_arraycopy  = generate_conjoint_byte_copy(false, "jbyte_arraycopy");
    StubRoutines::_jshort_arraycopy = generate_conjoint_short_copy(false, "jshort_arraycopy");
    StubRoutines::_jint_arraycopy   = generate_conjoint_int_oop_copy(false, false, "jint_arraycopy");
    StubRoutines::_jlong_arraycopy  = generate_conjoint_long_copy(false, "jlong_arraycopy");

    // We don't generate specialized code for HeapWord-aligned source
    // arrays, so just use the code we've already generated
    StubRoutines::_arrayof_jbyte_disjoint_arraycopy  = StubRoutines::_jbyte_disjoint_arraycopy;
    StubRoutines::_arrayof_jbyte_arraycopy           = StubRoutines::_jbyte_arraycopy;

    StubRoutines::_arrayof_jshort_disjoint_arraycopy = StubRoutines::_jshort_disjoint_arraycopy;
    StubRoutines::_arrayof_jshort_arraycopy          = StubRoutines::_jshort_arraycopy;

    StubRoutines::_arrayof_jint_disjoint_arraycopy   = StubRoutines::_jint_disjoint_arraycopy;
    StubRoutines::_arrayof_jint_arraycopy            = StubRoutines::_jint_arraycopy;

    StubRoutines::_arrayof_jlong_disjoint_arraycopy  = StubRoutines::_jlong_disjoint_arraycopy;
    StubRoutines::_arrayof_jlong_arraycopy           = StubRoutines::_jlong_arraycopy;

    StubRoutines::_arrayof_oop_disjoint_arraycopy    = StubRoutines::_oop_disjoint_arraycopy;
    StubRoutines::_arrayof_oop_arraycopy             = StubRoutines::_oop_arraycopy;

    StubRoutines::_arrayof_oop_disjoint_arraycopy_uninit    = StubRoutines::_oop_disjoint_arraycopy_uninit;
    StubRoutines::_arrayof_oop_arraycopy_uninit             = StubRoutines::_oop_arraycopy_uninit;

    StubRoutines::_jbyte_fill = generate_fill(T_BYTE, false, "jbyte_fill");
    StubRoutines::_jshort_fill = generate_fill(T_SHORT, false, "jshort_fill");
    StubRoutines::_jint_fill = generate_fill(T_INT, false, "jint_fill");
    StubRoutines::_arrayof_jbyte_fill = generate_fill(T_BYTE, true, "arrayof_jbyte_fill");
    StubRoutines::_arrayof_jshort_fill = generate_fill(T_SHORT, true, "arrayof_jshort_fill");
    StubRoutines::_arrayof_jint_fill = generate_fill(T_INT, true, "arrayof_jint_fill");
  }


#undef __
#define __ masm->

  // Continuation point for throwing of implicit exceptions that are
  // not handled in the current activation. Fabricates an exception
  // oop and initiates normal exception dispatching in this
  // frame. Since we need to preserve callee-saved values (currently
  // only for C2, but done for C1 as well) we need a callee-saved oop
  // map and therefore have to make these stubs into RuntimeStubs
  // rather than BufferBlobs.  If the compiler needs all registers to
  // be preserved between the fault point and the exception handler
  // then it must assume responsibility for that in
  // AbstractCompiler::continuation_for_implicit_null_exception or
  // continuation_for_implicit_division_by_zero_exception. All other
  // implicit exceptions (e.g., NullPointerException or
  // AbstractMethodError on entry) are either at call sites or
  // otherwise assume that stack unwinding will be initiated, so
  // caller saved registers were assumed volatile in the compiler.
  address generate_throw_exception(const char* name,
                                   address runtime_entry,
                                   bool restore_saved_exception_pc) {
    // Information about frame layout at time of blocking runtime call.
    // Note that we only have to preserve callee-saved registers since
    // the compilers are responsible for supplying a continuation point
    // if they expect all registers to be preserved.
    enum layout {
      thread_off,    // last_java_sp
      S7_off,        // callee saved register      sp + 1
      S6_off,        // callee saved register      sp + 2
      S5_off,        // callee saved register      sp + 3
      S4_off,        // callee saved register      sp + 4
      S3_off,        // callee saved register      sp + 5
      S2_off,        // callee saved register      sp + 6
      S1_off,        // callee saved register      sp + 7
      S0_off,        // callee saved register      sp + 8
      FP_off,
      ret_address,
      framesize
    };

    int insts_size = 2048;
    int locs_size  = 32;

    //  CodeBuffer* code     = new CodeBuffer(insts_size, locs_size, 0, 0, 0, false,
    //  NULL, NULL, NULL, false, NULL, name, false);
    CodeBuffer code (name , insts_size, locs_size);
    OopMapSet* oop_maps  = new OopMapSet();
    MacroAssembler* masm = new MacroAssembler(&code);

    address start = __ pc();

    // This is an inlined and slightly modified version of call_VM
    // which has the ability to fetch the return PC out of
    // thread-local storage and also sets up last_Java_sp slightly
    // differently than the real call_VM
#ifndef OPT_THREAD
    Register java_thread = TREG;
    __ get_thread(java_thread);
#else
    Register java_thread = TREG;
#endif
    if (restore_saved_exception_pc) {
      __ ld(RA, java_thread, in_bytes(JavaThread::saved_exception_pc_offset()));
    }

    __ enter(); // required for proper stackwalking of RuntimeStub frame

    __ addiu(SP, SP, (-1) * (framesize-2) * wordSize); // prolog
    __ sd(S0, SP, S0_off * wordSize);
    __ sd(S1, SP, S1_off * wordSize);
    __ sd(S2, SP, S2_off * wordSize);
    __ sd(S3, SP, S3_off * wordSize);
    __ sd(S4, SP, S4_off * wordSize);
    __ sd(S5, SP, S5_off * wordSize);
    __ sd(S6, SP, S6_off * wordSize);
    __ sd(S7, SP, S7_off * wordSize);

    int frame_complete = __ pc() - start;
    // push java thread (becomes first argument of C function)
    __ sd(java_thread, SP, thread_off * wordSize);
    if (java_thread != A0)
      __ move(A0, java_thread);

    // Set up last_Java_sp and last_Java_fp
    __ set_last_Java_frame(java_thread, SP, FP, NULL);
    // Align stack
    assert(StackAlignmentInBytes == 16, "must be");
    __ dins(SP, R0, 0, 4);

    __ relocate(relocInfo::internal_pc_type);
    {
      intptr_t save_pc = (intptr_t)__ pc() +  NativeMovConstReg::instruction_size + 28;
      __ patchable_set48(AT, save_pc);
    }
    __ sd(AT, java_thread, in_bytes(JavaThread::last_Java_pc_offset()));

    // Call runtime
    __ call(runtime_entry);
    __ delayed()->nop();
    // Generate oop map
    OopMap* map =  new OopMap(framesize, 0);
    oop_maps->add_gc_map(__ offset(),  map);

    // restore the thread (cannot use the pushed argument since arguments
    // may be overwritten by C code generated by an optimizing compiler);
    // however can use the register value directly if it is callee saved.
#ifndef OPT_THREAD
    __ get_thread(java_thread);
#endif

    __ ld(SP, java_thread, in_bytes(JavaThread::last_Java_sp_offset()));
    __ reset_last_Java_frame(java_thread, true);

    // Restore callee save registers.  This must be done after resetting the Java frame
    __ ld(S0, SP, S0_off * wordSize);
    __ ld(S1, SP, S1_off * wordSize);
    __ ld(S2, SP, S2_off * wordSize);
    __ ld(S3, SP, S3_off * wordSize);
    __ ld(S4, SP, S4_off * wordSize);
    __ ld(S5, SP, S5_off * wordSize);
    __ ld(S6, SP, S6_off * wordSize);
    __ ld(S7, SP, S7_off * wordSize);

    // discard arguments
    __ move(SP, FP); // epilog
    __ pop(FP);

    // check for pending exceptions
#ifdef ASSERT
    Label L;
    __ ld(AT, java_thread, in_bytes(Thread::pending_exception_offset()));
    __ bne(AT, R0, L);
    __ delayed()->nop();
    __ should_not_reach_here();
    __ bind(L);
#endif //ASSERT
    __ jmp(StubRoutines::forward_exception_entry(), relocInfo::runtime_call_type);
    __ delayed()->nop();
    RuntimeStub* stub = RuntimeStub::new_runtime_stub(name,
                                                      &code,
                                                      frame_complete,
                                                      framesize,
                                                      oop_maps, false);
    return stub->entry_point();
  }

  class MontgomeryMultiplyGenerator : public MacroAssembler {

    Register Pa_base, Pb_base, Pn_base, Pm_base, inv, Rlen, Rlen2, Ra, Rb, Rm,
      Rn, Iam, Ibn, Rhi_ab, Rlo_ab, Rhi_mn, Rlo_mn, t0, t1, t2, Ri, Rj;

    bool _squaring;

  public:
    MontgomeryMultiplyGenerator (Assembler *as, bool squaring)
      : MacroAssembler(as->code()), _squaring(squaring) {

      // Register allocation

      Register reg = A0;
      Pa_base = reg;      // Argument registers:
      if (squaring)
        Pb_base = Pa_base;
      else
        Pb_base = ++reg;
      Pn_base = ++reg;
      Rlen = ++reg;
      inv = ++reg;
      Rlen2 = inv;        // Reuse inv
      Pm_base = ++reg;

                          // Working registers:
      Ra = ++reg;         // The current digit of a, b, n, and m.
      Rb = ++reg;
      Rm = ++reg;
      Rn = ++reg;

      Iam = ++reg;        // Index to the current/next digit of a, b, n, and m.
      Ibn = ++reg;

      if (squaring) {
        t0 = ++reg;       // Three registers which form a
        t1 = AT;          // triple-precision accumuator.
        t2 = V0;

        Ri = V1;          // Inner and outer loop indexes.
        Rj = T8;

        Rhi_ab = T9;      // Product registers: low and high parts
        Rlo_ab = S0;      // of a*b and m*n.

        Rhi_mn = S1;
        Rlo_mn = S2;
      } else {
        t0 = AT;          // Three registers which form a
        t1 = V0;          // triple-precision accumuator.
        t2 = V1;

        Ri = T8;          // Inner and outer loop indexes.
        Rj = T9;

        Rhi_ab = S0;      // Product registers: low and high parts
        Rlo_ab = S1;      // of a*b and m*n.

        Rhi_mn = S2;
        Rlo_mn = S3;
      }
    }

  private:
    void enter() {
      addiu(SP, SP, -6 * wordSize);
      sd(FP, SP, 0 * wordSize);
      move(FP, SP);
    }

    void leave() {
      addiu(T0, FP, 6 * wordSize);
      ld(FP, FP, 0 * wordSize);
      move(SP, T0);
    }

    void save_regs() {
      if (!_squaring)
        sd(Rhi_ab, FP, 5 * wordSize);
      sd(Rlo_ab, FP, 4 * wordSize);
      sd(Rhi_mn, FP, 3 * wordSize);
      sd(Rlo_mn, FP, 2 * wordSize);
      sd(Pm_base, FP, 1 * wordSize);
    }

    void restore_regs() {
      if (!_squaring)
        ld(Rhi_ab, FP, 5 * wordSize);
      ld(Rlo_ab, FP, 4 * wordSize);
      ld(Rhi_mn, FP, 3 * wordSize);
      ld(Rlo_mn, FP, 2 * wordSize);
      ld(Pm_base, FP, 1 * wordSize);
    }

    template <typename T>
    void unroll_2(Register count, T block, Register tmp) {
      Label loop, end, odd;
      andi(tmp, count, 1);
      bne(tmp, R0, odd);
      delayed()->nop();
      beq(count, R0, end);
      delayed()->nop();
      align(16);
      bind(loop);
      (this->*block)();
      bind(odd);
      (this->*block)();
      addiu32(count, count, -2);
      bgtz(count, loop);
      delayed()->nop();
      bind(end);
    }

    template <typename T>
    void unroll_2(Register count, T block, Register d, Register s, Register tmp) {
      Label loop, end, odd;
      andi(tmp, count, 1);
      bne(tmp, R0, odd);
      delayed()->nop();
      beq(count, R0, end);
      delayed()->nop();
      align(16);
      bind(loop);
      (this->*block)(d, s, tmp);
      bind(odd);
      (this->*block)(d, s, tmp);
      addiu32(count, count, -2);
      bgtz(count, loop);
      delayed()->nop();
      bind(end);
    }

    void acc(Register Rhi, Register Rlo,
             Register t0, Register t1, Register t2, Register t, Register c) {
      daddu(t0, t0, Rlo);
      orr(t, t1, Rhi);
      sltu(c, t0, Rlo);
      daddu(t1, t1, Rhi);
      daddu(t1, t1, c);
      sltu(c, t1, t);
      daddu(t2, t2, c);
    }

    void pre1(Register i) {
      block_comment("pre1");
      // Iam = 0;
      // Ibn = i;

      sll(Ibn, i, LogBytesPerWord);

      // Ra = Pa_base[Iam];
      // Rb = Pb_base[Ibn];
      // Rm = Pm_base[Iam];
      // Rn = Pn_base[Ibn];

      ld(Ra, Pa_base, 0);
      gsldx(Rb, Pb_base, Ibn, 0);
      ld(Rm, Pm_base, 0);
      gsldx(Rn, Pn_base, Ibn, 0);

      move(Iam, R0);

      // Zero the m*n result.
      move(Rhi_mn, R0);
      move(Rlo_mn, R0);
    }

    // The core multiply-accumulate step of a Montgomery
    // multiplication.  The idea is to schedule operations as a
    // pipeline so that instructions with long latencies (loads and
    // multiplies) have time to complete before their results are
    // used.  This most benefits in-order implementations of the
    // architecture but out-of-order ones also benefit.
    void step() {
      block_comment("step");
      // MACC(Ra, Rb, t0, t1, t2);
      // Ra = Pa_base[++Iam];
      // Rb = Pb_base[--Ibn];
      addiu32(Iam, Iam, wordSize);
      addiu32(Ibn, Ibn, -wordSize);
      dmultu(Ra, Rb);
      acc(Rhi_mn, Rlo_mn, t0, t1, t2, Ra, Rb); // The pending m*n from the
                                               // previous iteration.
      gsldx(Ra, Pa_base, Iam, 0);
      mflo(Rlo_ab);
      mfhi(Rhi_ab);
      gsldx(Rb, Pb_base, Ibn, 0);

      // MACC(Rm, Rn, t0, t1, t2);
      // Rm = Pm_base[Iam];
      // Rn = Pn_base[Ibn];
      dmultu(Rm, Rn);
      acc(Rhi_ab, Rlo_ab, t0, t1, t2, Rm, Rn);
      gsldx(Rm, Pm_base, Iam, 0);
      mflo(Rlo_mn);
      mfhi(Rhi_mn);
      gsldx(Rn, Pn_base, Ibn, 0);
    }

    void post1() {
      block_comment("post1");

      // MACC(Ra, Rb, t0, t1, t2);
      dmultu(Ra, Rb);
      acc(Rhi_mn, Rlo_mn, t0, t1, t2, Ra, Rb);  // The pending m*n
      mflo(Rlo_ab);
      mfhi(Rhi_ab);
      acc(Rhi_ab, Rlo_ab, t0, t1, t2, Ra, Rb);

      // Pm_base[Iam] = Rm = t0 * inv;
      gsdmultu(Rm, t0, inv);
      gssdx(Rm, Pm_base, Iam, 0);

      // MACC(Rm, Rn, t0, t1, t2);
      // t0 = t1; t1 = t2; t2 = 0;
      dmultu(Rm, Rn);
      mfhi(Rhi_mn);

#ifndef PRODUCT
      // assert(m[i] * n[0] + t0 == 0, "broken Montgomery multiply");
      {
        mflo(Rlo_mn);
        daddu(Rlo_mn, t0, Rlo_mn);
        Label ok;
        beq(Rlo_mn, R0, ok);
        delayed()->nop(); {
          stop("broken Montgomery multiply");
        } bind(ok);
      }
#endif

      // We have very carefully set things up so that
      // m[i]*n[0] + t0 == 0 (mod b), so we don't have to calculate
      // the lower half of Rm * Rn because we know the result already:
      // it must be -t0.  t0 + (-t0) must generate a carry iff
      // t0 != 0.  So, rather than do a mul and an adds we just set
      // the carry flag iff t0 is nonzero.
      //
      // mflo(Rlo_mn);
      // addu(t0, t0, Rlo_mn);
      orr(Ra, t1, Rhi_mn);
      sltu(Rb, R0, t0);
      daddu(t0, t1, Rhi_mn);
      daddu(t0, t0, Rb);
      sltu(Rb, t0, Ra);
      daddu(t1, t2, Rb);
      move(t2, R0);
    }

    void pre2(Register i, Register len) {
      block_comment("pre2");

      // Rj == i-len
      subu32(Rj, i, len);

      // Iam = i - len;
      // Ibn = len;
      sll(Iam, Rj, LogBytesPerWord);
      sll(Ibn, len, LogBytesPerWord);

      // Ra = Pa_base[++Iam];
      // Rb = Pb_base[--Ibn];
      // Rm = Pm_base[++Iam];
      // Rn = Pn_base[--Ibn];
      gsldx(Ra, Pa_base, Iam, wordSize);
      gsldx(Rb, Pb_base, Ibn, -wordSize);
      gsldx(Rm, Pm_base, Iam, wordSize);
      gsldx(Rn, Pn_base, Ibn, -wordSize);

      addiu32(Iam, Iam, wordSize);
      addiu32(Ibn, Ibn, -wordSize);

      move(Rhi_mn, R0);
      move(Rlo_mn, R0);
    }

    void post2(Register i, Register len) {
      block_comment("post2");

      subu32(Rj, i, len);
      sll(Iam, Rj, LogBytesPerWord);

      daddu(t0, t0, Rlo_mn); // The pending m*n, low part

      // As soon as we know the least significant digit of our result,
      // store it.
      // Pm_base[i-len] = t0;
      gssdx(t0, Pm_base, Iam, 0);

      // t0 = t1; t1 = t2; t2 = 0;
      orr(Ra, t1, Rhi_mn);
      sltu(Rb, t0, Rlo_mn);
      daddu(t0, t1, Rhi_mn); // The pending m*n, high part
      daddu(t0, t0, Rb);
      sltu(Rb, t0, Ra);
      daddu(t1, t2, Rb);
      move(t2, R0);
    }

    // A carry in t0 after Montgomery multiplication means that we
    // should subtract multiples of n from our result in m.  We'll
    // keep doing that until there is no carry.
    void normalize(Register len) {
      block_comment("normalize");
      // while (t0)
      //   t0 = sub(Pm_base, Pn_base, t0, len);
      Label loop, post, again;
      Register cnt = t1, i = t2, b = Ra, t = Rb; // Re-use registers; we're done with them now
      beq(t0, R0, post);
      delayed()->nop(); {
        bind(again); {
          move(i, R0);
          move(b, R0);
          sll(cnt, len, LogBytesPerWord);
          align(16);
          bind(loop); {
            gsldx(Rm, Pm_base, i, 0);
            gsldx(Rn, Pn_base, i, 0);
            sltu(t, Rm, b);
            dsubu(Rm, Rm, b);
            sltu(b, Rm, Rn);
            dsubu(Rm, Rm, Rn);
            orr(b, b, t);
            gssdx(Rm, Pm_base, i, 0);
            addiu32(i, i, BytesPerWord);
          } sltu(Rm, i, cnt);
            bne(Rm, R0, loop);
            delayed()->nop();
          subu(t0, t0, b);
        } bne(t0, R0, again);
          delayed()->nop();
      } bind(post);
    }

    // Move memory at s to d, reversing words.
    //    Increments d to end of copied memory
    //    Destroys tmp1, tmp2, tmp3
    //    Preserves len
    //    Leaves s pointing to the address which was in d at start
    void reverse(Register d, Register s, Register len, Register tmp1, Register tmp2) {
      assert(tmp1 < S0 && tmp2 < S0, "register corruption");

      sll(tmp1, len, LogBytesPerWord);
      addu(s, s, tmp1);
      move(tmp1, len);
      unroll_2(tmp1, &MontgomeryMultiplyGenerator::reverse1, d, s, tmp2);
      sll(s, len, LogBytesPerWord);
      subu(s, d, s);
    }

    // where
    void reverse1(Register d, Register s, Register tmp) {
      ld(tmp, s, -wordSize);
      addiu(s, s, -wordSize);
      addiu(d, d, wordSize);
      drotr32(tmp, tmp, 32 - 32);
      sd(tmp, d, -wordSize);
    }

  public:
    /**
     * Fast Montgomery multiplication.  The derivation of the
     * algorithm is in A Cryptographic Library for the Motorola
     * DSP56000, Dusse and Kaliski, Proc. EUROCRYPT 90, pp. 230-237.
     *
     * Arguments:
     *
     * Inputs for multiplication:
     *   A0   - int array elements a
     *   A1   - int array elements b
     *   A2   - int array elements n (the modulus)
     *   A3   - int length
     *   A4   - int inv
     *   A5   - int array elements m (the result)
     *
     * Inputs for squaring:
     *   A0   - int array elements a
     *   A1   - int array elements n (the modulus)
     *   A2   - int length
     *   A3   - int inv
     *   A4   - int array elements m (the result)
     *
     */
    address generate_multiply() {
      Label argh, nothing;
      bind(argh);
      stop("MontgomeryMultiply total_allocation must be <= 8192");

      align(CodeEntryAlignment);
      address entry = pc();

      beq(Rlen, R0, nothing);
      delayed()->nop();

      enter();

      // Make room.
      sltiu(Ra, Rlen, 513);
      beq(Ra, R0, argh);
      delayed()->sll(Ra, Rlen, exact_log2(4 * sizeof (jint)));
      subu(Ra, SP, Ra);

      srl(Rlen, Rlen, 1); // length in longwords = len/2

      {
        // Copy input args, reversing as we go.  We use Ra as a
        // temporary variable.
        reverse(Ra, Pa_base, Rlen, t0, t1);
        if (!_squaring)
          reverse(Ra, Pb_base, Rlen, t0, t1);
        reverse(Ra, Pn_base, Rlen, t0, t1);
      }

      // Push all call-saved registers and also Pm_base which we'll need
      // at the end.
      save_regs();

#ifndef PRODUCT
      // assert(inv * n[0] == -1UL, "broken inverse in Montgomery multiply");
      {
        ld(Rn, Pn_base, 0);
        li(t0, -1);
        gsdmultu(Rlo_mn, Rn, inv);
        Label ok;
        beq(Rlo_mn, t0, ok);
        delayed()->nop(); {
          stop("broken inverse in Montgomery multiply");
        } bind(ok);
      }
#endif

      move(Pm_base, Ra);

      move(t0, R0);
      move(t1, R0);
      move(t2, R0);

      block_comment("for (int i = 0; i < len; i++) {");
      move(Ri, R0); {
        Label loop, end;
        slt(Ra, Ri, Rlen);
        beq(Ra, R0, end);
        delayed()->nop();

        bind(loop);
        pre1(Ri);

        block_comment("  for (j = i; j; j--) {"); {
          move(Rj, Ri);
          unroll_2(Rj, &MontgomeryMultiplyGenerator::step, Rlo_ab);
        } block_comment("  } // j");

        post1();
        addiu32(Ri, Ri, 1);
        slt(Ra, Ri, Rlen);
        bne(Ra, R0, loop);
        delayed()->nop();
        bind(end);
        block_comment("} // i");
      }

      block_comment("for (int i = len; i < 2*len; i++) {");
      move(Ri, Rlen);
      sll(Rlen2, Rlen, 1); {
        Label loop, end;
        slt(Ra, Ri, Rlen2);
        beq(Ra, R0, end);
        delayed()->nop();

        bind(loop);
        pre2(Ri, Rlen);

        block_comment("  for (j = len*2-i-1; j; j--) {"); {
          subu32(Rj, Rlen2, Ri);
          addiu32(Rj, Rj, -1);
          unroll_2(Rj, &MontgomeryMultiplyGenerator::step, Rlo_ab);
        } block_comment("  } // j");

        post2(Ri, Rlen);
        addiu32(Ri, Ri, 1);
        slt(Ra, Ri, Rlen2);
        bne(Ra, R0, loop);
        delayed()->nop();
        bind(end);
      }
      block_comment("} // i");

      normalize(Rlen);

      move(Ra, Pm_base);  // Save Pm_base in Ra
      restore_regs();  // Restore caller's Pm_base

      // Copy our result into caller's Pm_base
      reverse(Pm_base, Ra, Rlen, t0, t1);

      leave();
      bind(nothing);
      jr(RA);

      return entry;
    }
    // In C, approximately:

    // void
    // montgomery_multiply(unsigned long Pa_base[], unsigned long Pb_base[],
    //                     unsigned long Pn_base[], unsigned long Pm_base[],
    //                     unsigned long inv, int len) {
    //   unsigned long t0 = 0, t1 = 0, t2 = 0; // Triple-precision accumulator
    //   unsigned long Ra, Rb, Rn, Rm;
    //   int i, Iam, Ibn;

    //   assert(inv * Pn_base[0] == -1UL, "broken inverse in Montgomery multiply");

    //   for (i = 0; i < len; i++) {
    //     int j;

    //     Iam = 0;
    //     Ibn = i;

    //     Ra = Pa_base[Iam];
    //     Rb = Pb_base[Iam];
    //     Rm = Pm_base[Ibn];
    //     Rn = Pn_base[Ibn];

    //     int iters = i;
    //     for (j = 0; iters--; j++) {
    //       assert(Ra == Pa_base[j] && Rb == Pb_base[i-j], "must be");
    //       MACC(Ra, Rb, t0, t1, t2);
    //       Ra = Pa_base[++Iam];
    //       Rb = pb_base[--Ibn];
    //       assert(Rm == Pm_base[j] && Rn == Pn_base[i-j], "must be");
    //       MACC(Rm, Rn, t0, t1, t2);
    //       Rm = Pm_base[++Iam];
    //       Rn = Pn_base[--Ibn];
    //     }

    //     assert(Ra == Pa_base[i] && Rb == Pb_base[0], "must be");
    //     MACC(Ra, Rb, t0, t1, t2);
    //     Pm_base[Iam] = Rm = t0 * inv;
    //     assert(Rm == Pm_base[i] && Rn == Pn_base[0], "must be");
    //     MACC(Rm, Rn, t0, t1, t2);

    //     assert(t0 == 0, "broken Montgomery multiply");

    //     t0 = t1; t1 = t2; t2 = 0;
    //   }

    //   for (i = len; i < 2*len; i++) {
    //     int j;

    //     Iam = i - len;
    //     Ibn = len;

    //     Ra = Pa_base[++Iam];
    //     Rb = Pb_base[--Ibn];
    //     Rm = Pm_base[++Iam];
    //     Rn = Pn_base[--Ibn];

    //     int iters = len*2-i-1;
    //     for (j = i-len+1; iters--; j++) {
    //       assert(Ra == Pa_base[j] && Rb == Pb_base[i-j], "must be");
    //       MACC(Ra, Rb, t0, t1, t2);
    //       Ra = Pa_base[++Iam];
    //       Rb = Pb_base[--Ibn];
    //       assert(Rm == Pm_base[j] && Rn == Pn_base[i-j], "must be");
    //       MACC(Rm, Rn, t0, t1, t2);
    //       Rm = Pm_base[++Iam];
    //       Rn = Pn_base[--Ibn];
    //     }

    //     Pm_base[i-len] = t0;
    //     t0 = t1; t1 = t2; t2 = 0;
    //   }

    //   while (t0)
    //     t0 = sub(Pm_base, Pn_base, t0, len);
    // }
  };

  // Initialization
  void generate_initial() {
    // Generates all stubs and initializes the entry points

    //-------------------------------------------------------------
    //-----------------------------------------------------------
    // entry points that exist in all platforms
    // Note: This is code that could be shared among different platforms - however the benefit seems to be smaller
    // than the disadvantage of having a much more complicated generator structure.
    // See also comment in stubRoutines.hpp.
    StubRoutines::_forward_exception_entry = generate_forward_exception();
    StubRoutines::_call_stub_entry = generate_call_stub(StubRoutines::_call_stub_return_address);
    // is referenced by megamorphic call
    StubRoutines::_catch_exception_entry = generate_catch_exception();

    StubRoutines::_throw_StackOverflowError_entry =
      generate_throw_exception("StackOverflowError throw_exception",
                               CAST_FROM_FN_PTR(address, SharedRuntime::throw_StackOverflowError),
                               false);
    StubRoutines::_throw_delayed_StackOverflowError_entry =
      generate_throw_exception("delayed StackOverflowError throw_exception",
                               CAST_FROM_FN_PTR(address, SharedRuntime::throw_delayed_StackOverflowError),
                               false);
  }

  void generate_all() {
    // Generates all stubs and initializes the entry points

    // These entry points require SharedInfo::stack0 to be set up in
    // non-core builds and need to be relocatable, so they each
    // fabricate a RuntimeStub internally.
    StubRoutines::_throw_AbstractMethodError_entry = generate_throw_exception("AbstractMethodError throw_exception",
                                                                               CAST_FROM_FN_PTR(address, SharedRuntime::throw_AbstractMethodError),  false);

    StubRoutines::_throw_IncompatibleClassChangeError_entry = generate_throw_exception("IncompatibleClassChangeError throw_exception",
                                                                               CAST_FROM_FN_PTR(address, SharedRuntime:: throw_IncompatibleClassChangeError), false);

    StubRoutines::_throw_NullPointerException_at_call_entry = generate_throw_exception("NullPointerException at call throw_exception",
                                                                                        CAST_FROM_FN_PTR(address, SharedRuntime::throw_NullPointerException_at_call), false);

    // entry points that are platform specific

    // support for verify_oop (must happen after universe_init)
    StubRoutines::_verify_oop_subroutine_entry     = generate_verify_oop();
#ifndef CORE
    // arraycopy stubs used by compilers
    generate_arraycopy_stubs();
#endif

#ifdef COMPILER2
    if (UseMontgomeryMultiplyIntrinsic) {
      if (UseLEXT1) {
        StubCodeMark mark(this, "StubRoutines", "montgomeryMultiply");
        MontgomeryMultiplyGenerator g(_masm, false /* squaring */);
        StubRoutines::_montgomeryMultiply = g.generate_multiply();
      } else {
        StubRoutines::_montgomeryMultiply
          = CAST_FROM_FN_PTR(address, SharedRuntime::montgomery_multiply);
      }
    }
    if (UseMontgomerySquareIntrinsic) {
      if (UseLEXT1) {
        StubCodeMark mark(this, "StubRoutines", "montgomerySquare");
        MontgomeryMultiplyGenerator g(_masm, true /* squaring */);
        // We use generate_multiply() rather than generate_square()
        // because it's faster for the sizes of modulus we care about.
        StubRoutines::_montgomerySquare = g.generate_multiply();
      } else {
        StubRoutines::_montgomerySquare
          = CAST_FROM_FN_PTR(address, SharedRuntime::montgomery_square);
      }
    }
#endif
  }

 public:
  StubGenerator(CodeBuffer* code, bool all) : StubCodeGenerator(code) {
    if (all) {
      generate_all();
    } else {
      generate_initial();
    }
  }
}; // end class declaration

#define UCM_TABLE_MAX_ENTRIES 2
void StubGenerator_generate(CodeBuffer* code, bool all) {
  if (UnsafeCopyMemory::_table == NULL) {
    UnsafeCopyMemory::create_table(UCM_TABLE_MAX_ENTRIES);
  }
  StubGenerator g(code, all);
}
