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

#ifndef CPU_MIPS_VM_C2_MACROASSEMBLER_MIPS_HPP
#define CPU_MIPS_VM_C2_MACROASSEMBLER_MIPS_HPP

// C2_MacroAssembler contains high-level macros for C2

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

public:

  void fast_lock(Register obj, Register box, Register res, Register tmp, Register scr);
  void fast_unlock(Register obj, Register box, Register res, Register tmp, Register scr);

  // For C2 to support long branches
  void beq_long   (Register rs, Register rt, Label& L);
  void bne_long   (Register rs, Register rt, Label& L);
  void bc1t_long  (Label& L);
  void bc1f_long  (Label& L);

  // Compare strings.
  void string_compare(Register str1, Register str2,
                      Register cnt1, Register cnt2, Register result,
                      int ae);

  // Compare char[] or byte[] arrays.
  void arrays_equals(Register str1, Register str2,
                     Register cnt, Register tmp, Register result,
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
    STORE_FLOAT      = FLOAT_TYPE | SIGNED_TYPE | 0x3,
    STORE_DOUBLE     = FLOAT_TYPE | SIGNED_TYPE | 0x4
  } CMLoadStoreDataType;

  void loadstore_enc(Register reg, int base, int index, int scale, int disp, int type) {
    assert((type & INT_TYPE), "must be General reg type");
    loadstore_t(reg, base, index, scale, disp, type);
  }

  void loadstore_enc(FloatRegister reg, int base, int index, int scale, int disp, int type) {
    assert((type & FLOAT_TYPE), "must be Float reg type");
    loadstore_t(reg, base, index, scale, disp, type);
  }

private:

  template <typename T>
  void loadstore_t(T reg, int base, int index, int scale, int disp, int type) {
    if (index != -1) {
      if (Assembler::is_simm16(disp)) {
        if (UseLEXT1 && (type & SIGNED_TYPE) && Assembler::is_simm(disp, 8)) {
          if (scale == 0) {
            gs_loadstore(reg, as_Register(base), as_Register(index), disp, type);
          } else {
            dsll(AT, as_Register(index), scale);
            gs_loadstore(reg, as_Register(base), AT, disp, type);
          }
        } else {
          if (scale == 0) {
            addu(AT, as_Register(base), as_Register(index));
          } else {
            dsll(AT, as_Register(index), scale);
            addu(AT, as_Register(base), AT);
          }
          loadstore(reg, AT, disp, type);
        }
      } else {
          if (scale == 0) {
            addu(AT, as_Register(base), as_Register(index));
          } else {
            dsll(AT, as_Register(index), scale);
            addu(AT, as_Register(base), AT);
          }
          move(T9, disp);
          if (UseLEXT1 && (type & SIGNED_TYPE)) {
            gs_loadstore(reg, AT, T9, 0, type);
          } else {
            addu(AT, AT, T9);
            loadstore(reg, AT, 0, type);
          }
        }
      } else {
        if (Assembler::is_simm16(disp)) {
          loadstore(reg, as_Register(base), disp, type);
        } else {
          move(T9, disp);
          if (UseLEXT1 && (type & SIGNED_TYPE)) {
            gs_loadstore(reg, as_Register(base), T9, 0, type);
          } else {
            addu(AT, as_Register(base), T9);
            loadstore(reg, AT, 0, type);
          }
        }
    }
  }
  void loadstore(Register reg, Register base, int disp, int type);
  void loadstore(FloatRegister reg, Register base, int disp, int type);
  void gs_loadstore(Register reg, Register base, Register index, int disp, int type);
  void gs_loadstore(FloatRegister reg, Register base, Register index, int disp, int type);

#endif // CPU_MIPS_VM_C2_MACROASSEMBLER_MIPS_HPP
