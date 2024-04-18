/*
 * Copyright (c) 2004, 2010, Oracle and/or its affiliates. All rights reserved.
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
#include "code/codeBlob.hpp"
#include "gc/shared/barrierSet.hpp"
#include "gc/shared/barrierSetAssembler.hpp"
#include "memory/resourceArea.hpp"
#include "prims/jniFastGetField.hpp"
#include "prims/jvm_misc.hpp"
#include "prims/jvmtiExport.hpp"
#include "runtime/safepoint.hpp"

#define __ masm->

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

#define BUFFER_SIZE 30*wordSize

// Instead of issuing membar for LoadLoad barrier, we create address dependency
// between loads, which is more efficient than membar.

address JNI_FastGetField::generate_fast_get_int_field0(BasicType type) {
  const char *name = NULL;
  switch (type) {
    case T_BOOLEAN: name = "jni_fast_GetBooleanField"; break;
    case T_BYTE:    name = "jni_fast_GetByteField";    break;
    case T_CHAR:    name = "jni_fast_GetCharField";    break;
    case T_SHORT:   name = "jni_fast_GetShortField";   break;
    case T_INT:     name = "jni_fast_GetIntField";     break;
    case T_LONG:    name = "jni_fast_GetLongField";    break;
    case T_FLOAT:   name = "jni_fast_GetFloatField";   break;
    case T_DOUBLE:  name = "jni_fast_GetDoubleField";  break;
    default:        ShouldNotReachHere();
  }
  ResourceMark rm;
  BufferBlob* blob = BufferBlob::create(name, BUFFER_SIZE);
  CodeBuffer cbuf(blob);
  MacroAssembler* masm = new MacroAssembler(&cbuf);
  address fast_entry = __ pc();
  Label slow;

  const Register env = A0;
  const Register obj = A1;
  const Register fid = A2;
  const Register tmp1 = AT;
  const Register tmp2 = T9;
  const Register obj_addr = T0;
  const Register field_val = V0;
  const Register field_addr = T0;
  const Register counter_addr = T2;
  const Register counter_prev_val = T1;

  __ li(counter_addr, SafepointSynchronize::safepoint_counter_addr());
  __ lw(counter_prev_val, counter_addr, 0);

  // Parameters(A0~A3) should not be modified, since they will be used in slow path
  __ andi(tmp1, counter_prev_val, 1);
  __ bne(tmp1, R0, slow);
  __ delayed()->nop();

  if (JvmtiExport::can_post_field_access()) {
    // Check to see if a field access watch has been set before we
    // take the fast path.
    __ li(tmp1, JvmtiExport::get_field_access_count_addr());
    // address dependency
    __ xorr(tmp1, tmp1, counter_prev_val);
    __ xorr(tmp1, tmp1, counter_prev_val);
    __ lw(tmp1, tmp1, 0);
    __ bne(tmp1, R0, slow);
    __ delayed()->nop();
  }

  __ move(obj_addr, obj);
  // Both obj_addr and tmp2 are clobbered by try_resolve_jobject_in_native.
  BarrierSetAssembler* bs = BarrierSet::barrier_set()->barrier_set_assembler();
  bs->try_resolve_jobject_in_native(masm, env, obj_addr, tmp2, slow);

  __ dsrl(tmp1, fid, 2); // offset
  __ daddu(field_addr, obj_addr, tmp1);
  // address dependency
  __ xorr(field_addr, field_addr, counter_prev_val);
  __ xorr(field_addr, field_addr, counter_prev_val);

  assert(count < LIST_CAPACITY, "LIST_CAPACITY too small");
  speculative_load_pclist[count] = __ pc();
  switch (type) {
    case T_BOOLEAN: __ lbu (field_val, field_addr, 0); break;
    case T_BYTE:    __ lb  (field_val, field_addr, 0); break;
    case T_CHAR:    __ lhu (field_val, field_addr, 0); break;
    case T_SHORT:   __ lh  (field_val, field_addr, 0); break;
    case T_INT:     __ lw  (field_val, field_addr, 0); break;
    case T_LONG:    __ ld  (field_val, field_addr, 0); break;
    case T_FLOAT:   __ lwu (field_val, field_addr, 0); break;
    case T_DOUBLE:  __ ld  (field_val, field_addr, 0); break;
    default:        ShouldNotReachHere();
  }

  // address dependency
  __ xorr(counter_addr, counter_addr, field_val);
  __ xorr(counter_addr, counter_addr, field_val);
  __ lw(tmp1, counter_addr, 0);
  __ bne(counter_prev_val, tmp1, slow);
  __ delayed()->nop();

  switch (type) {
    case T_FLOAT:   __ mtc1 (field_val, F0); break;
    case T_DOUBLE:  __ dmtc1(field_val, F0); break;
    default:                                 break;
  }

  __ jr(RA);
  __ delayed()->nop();

  slowcase_entry_pclist[count++] = __ pc();
  __ bind (slow);
  address slow_case_addr = NULL;
  switch (type) {
    case T_BOOLEAN: slow_case_addr = jni_GetBooleanField_addr(); break;
    case T_BYTE:    slow_case_addr = jni_GetByteField_addr();    break;
    case T_CHAR:    slow_case_addr = jni_GetCharField_addr();    break;
    case T_SHORT:   slow_case_addr = jni_GetShortField_addr();   break;
    case T_INT:     slow_case_addr = jni_GetIntField_addr();     break;
    case T_LONG:    slow_case_addr = jni_GetLongField_addr();    break;
    case T_FLOAT:   slow_case_addr = jni_GetFloatField_addr();   break;
    case T_DOUBLE:  slow_case_addr = jni_GetDoubleField_addr();  break;
    default:        ShouldNotReachHere();
  }
  __ jmp(slow_case_addr);
  __ delayed()->nop();

  __ flush ();

  return fast_entry;
}

address JNI_FastGetField::generate_fast_get_boolean_field() {
  return generate_fast_get_int_field0(T_BOOLEAN);
}

address JNI_FastGetField::generate_fast_get_byte_field() {
  return generate_fast_get_int_field0(T_BYTE);
}

address JNI_FastGetField::generate_fast_get_char_field() {
  return generate_fast_get_int_field0(T_CHAR);
}

address JNI_FastGetField::generate_fast_get_short_field() {
  return generate_fast_get_int_field0(T_SHORT);
}

address JNI_FastGetField::generate_fast_get_int_field() {
  return generate_fast_get_int_field0(T_INT);
}

address JNI_FastGetField::generate_fast_get_long_field() {
  return generate_fast_get_int_field0(T_LONG);
}

address JNI_FastGetField::generate_fast_get_float_field() {
  return generate_fast_get_int_field0(T_FLOAT);
}

address JNI_FastGetField::generate_fast_get_double_field() {
  return generate_fast_get_int_field0(T_DOUBLE);
}
