/*
 * Copyright (c) 2020, Oracle and/or its affiliates. All rights reserved.
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
 *
 */

#ifndef CPU_LOONGARCH_C2_MACROASSEMBLER_LOONGARCH_HPP
#define CPU_LOONGARCH_C2_MACROASSEMBLER_LOONGARCH_HPP

// C2_MacroAssembler contains high-level macros for C2

public:

  void cmp_branch_short(int flag, Register op1, Register op2, Label& L, bool is_signed);
  void cmp_branch_long(int flag, Register op1, Register op2, Label* L, bool is_signed);
  void cmp_branchEqNe_off21(int flag, Register op1, Label& L);
  void fast_lock(Register obj, Register box, Register res, Register tmp, Register scr);
  void fast_unlock(Register obj, Register box, Register res, Register tmp, Register scr);

  // For C2 to support long branches
  void beq_long   (Register rs, Register rt, Label& L);
  void bne_long   (Register rs, Register rt, Label& L);
  void blt_long   (Register rs, Register rt, Label& L, bool is_signed);
  void bge_long   (Register rs, Register rt, Label& L, bool is_signed);
  void bc1t_long  (Label& L);
  void bc1f_long  (Label& L);

  // Compare strings.
  void string_compare(Register str1, Register str2,
                      Register cnt1, Register cnt2, Register result,
                      int ae, Register tmp1, Register tmp2);

  // Find index of char in Latin-1 string
  void stringL_indexof_char(Register str1, Register cnt1,
                            Register ch, Register result,
                            Register tmp1, Register tmp2,
                            Register tmp3);

  // Find index of char in UTF-16 string
  void string_indexof_char(Register str1, Register cnt1,
                           Register ch, Register result,
                           Register tmp1, Register tmp2,
                           Register tmp3);

  void string_indexof(Register haystack, Register needle,
                      Register haystack_len, Register needle_len,
                      Register result, int ae);

  void string_indexof_linearscan(Register haystack, Register needle,
                                 Register haystack_len, Register needle_len,
                                 int needle_con_cnt, Register result, int ae);

  // Compare char[] or byte[] arrays.
  void arrays_equals(Register str1, Register str2,
                     Register cnt, Register tmp1, Register tmp2, Register result,
                     bool is_char);

 // Memory Data Type
  #define INT_TYPE 0x100
  #define FLOAT_TYPE 0x200
  #define SIGNED_TYPE 0x10
  #define UNSIGNED_TYPE 0x20

  typedef enum {
    LOAD_BYTE        = INT_TYPE | SIGNED_TYPE | 0x1,
    LOAD_CHAR        = INT_TYPE | SIGNED_TYPE | 0x2,
    LOAD_SHORT       = INT_TYPE | SIGNED_TYPE | 0x3,
    LOAD_INT         = INT_TYPE | SIGNED_TYPE | 0x4,
    LOAD_LONG        = INT_TYPE | SIGNED_TYPE | 0x5,
    STORE_BYTE       = INT_TYPE | SIGNED_TYPE | 0x6,
    STORE_CHAR       = INT_TYPE | SIGNED_TYPE | 0x7,
    STORE_SHORT      = INT_TYPE | SIGNED_TYPE | 0x8,
    STORE_INT        = INT_TYPE | SIGNED_TYPE | 0x9,
    STORE_LONG       = INT_TYPE | SIGNED_TYPE | 0xa,
    LOAD_LINKED_LONG = INT_TYPE | SIGNED_TYPE | 0xb,

    LOAD_U_BYTE      = INT_TYPE | UNSIGNED_TYPE | 0x1,
    LOAD_U_SHORT     = INT_TYPE | UNSIGNED_TYPE | 0x2,
    LOAD_U_INT       = INT_TYPE | UNSIGNED_TYPE | 0x3,

    LOAD_FLOAT       = FLOAT_TYPE | SIGNED_TYPE | 0x1,
    LOAD_DOUBLE      = FLOAT_TYPE | SIGNED_TYPE | 0x2,
    LOAD_VECTORX     = FLOAT_TYPE | SIGNED_TYPE | 0x3,
    LOAD_VECTORY     = FLOAT_TYPE | SIGNED_TYPE | 0x4,
    STORE_FLOAT      = FLOAT_TYPE | SIGNED_TYPE | 0x5,
    STORE_DOUBLE     = FLOAT_TYPE | SIGNED_TYPE | 0x6,
    STORE_VECTORX    = FLOAT_TYPE | SIGNED_TYPE | 0x7,
    STORE_VECTORY    = FLOAT_TYPE | SIGNED_TYPE | 0x8
  } CMLoadStoreDataType;

  void loadstore_enc(Register reg, int base, int index, int scale, int disp, int type) {
    assert((type & INT_TYPE), "must be General reg type");
    loadstore_t(reg, base, index, scale, disp, type);
  }

  void loadstore_enc(FloatRegister reg, int base, int index, int scale, int disp, int type) {
    assert((type & FLOAT_TYPE), "must be Float reg type");
    loadstore_t(reg, base, index, scale, disp, type);
  }

  void reduce(Register dst, Register src, FloatRegister vsrc, FloatRegister tmp1, FloatRegister tmp2, BasicType type, int opcode, int vector_size);
  void reduce(FloatRegister dst, FloatRegister src, FloatRegister vsrc, FloatRegister tmp, BasicType type, int opcode, int vector_size);

  void vector_compare(FloatRegister dst, FloatRegister src1, FloatRegister src2, BasicType type, int cond, int vector_size);

private:

  template <typename T>
  void loadstore_t(T reg, int base, int index, int scale, int disp, int type) {
    if (index != -1) {
        assert(((scale==0)&&(disp==0)), "only support base+index");
        loadstore(reg, as_Register(base), as_Register(index), type);
    } else {
      loadstore(reg, as_Register(base), disp, type);
    }
  }
  void loadstore(Register reg, Register base, int disp, int type);
  void loadstore(Register reg, Register base, Register disp, int type);
  void loadstore(FloatRegister reg, Register base, int disp, int type);
  void loadstore(FloatRegister reg, Register base, Register disp, int type);

  void reduce_ins_v(FloatRegister vec1, FloatRegister vec2, FloatRegister vec3, BasicType type, int opcode);
  void reduce_ins_r(Register reg1, Register reg2, Register reg3, BasicType type, int opcode);
  void reduce_ins_f(FloatRegister reg1, FloatRegister reg2, FloatRegister reg3, BasicType type, int opcode);
#endif // CPU_LOONGARCH_C2_MACROASSEMBLER_LOONGARCH_HPP
