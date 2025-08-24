/*
 * Copyright (c) 1999, 2013, Oracle and/or its affiliates. All rights reserved.
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

#ifndef OS_CPU_LINUX_LOONGARCH_ATOMIC_LINUX_LOONGARCH_HPP
#define OS_CPU_LINUX_LOONGARCH_ATOMIC_LINUX_LOONGARCH_HPP

#include "runtime/vm_version.hpp"

// Implementation of class atomic

template<size_t byte_size>
struct Atomic::PlatformAdd {
  template<typename D, typename I>
  D fetch_and_add(D volatile* dest, I add_value, atomic_memory_order order) const;

  template<typename D, typename I>
  D add_and_fetch(D volatile* dest, I add_value, atomic_memory_order order) const {
    return fetch_and_add(dest, add_value, order) + add_value;
  }
};

template<>
template<typename D, typename I>
inline D Atomic::PlatformAdd<4>::fetch_and_add(D volatile* dest, I add_value,
                                               atomic_memory_order order) const {
  STATIC_ASSERT(4 == sizeof(I));
  STATIC_ASSERT(4 == sizeof(D));
  D old_value;

  switch (order) {
  case memory_order_relaxed:
    asm volatile (
      "amadd.w %[old], %[add], %[dest] \n\t"
      : [old] "=&r" (old_value)
      : [add] "r" (add_value), [dest] "r" (dest)
      : "memory");
    break;
  default:
    asm volatile (
      "amadd_db.w %[old], %[add], %[dest] \n\t"
      : [old] "=&r" (old_value)
      : [add] "r" (add_value), [dest] "r" (dest)
      : "memory");
    break;
  }

  return old_value;
}

template<>
template<typename D, typename I>
inline D Atomic::PlatformAdd<8>::fetch_and_add(D volatile* dest, I add_value,
                                               atomic_memory_order order) const {
  STATIC_ASSERT(8 == sizeof(I));
  STATIC_ASSERT(8 == sizeof(D));
  D old_value;

  switch (order) {
  case memory_order_relaxed:
    asm volatile (
      "amadd.d %[old], %[add], %[dest] \n\t"
      : [old] "=&r" (old_value)
      : [add] "r" (add_value), [dest] "r" (dest)
      : "memory");
    break;
  default:
    asm volatile (
      "amadd_db.d %[old], %[add], %[dest] \n\t"
      : [old] "=&r" (old_value)
      : [add] "r" (add_value), [dest] "r" (dest)
      : "memory");
    break;
  }

  return old_value;
}

template<>
template<typename T>
inline T Atomic::PlatformXchg<4>::operator()(T volatile* dest,
                                             T exchange_value,
                                             atomic_memory_order order) const {
  STATIC_ASSERT(4 == sizeof(T));
  T old_value;

  switch (order) {
  case memory_order_relaxed:
    asm volatile (
      "amswap.w %[_old], %[_new], %[dest] \n\t"
      : [_old] "=&r" (old_value)
      : [_new] "r" (exchange_value), [dest] "r" (dest)
      : "memory");
    break;
  default:
    asm volatile (
      "amswap_db.w %[_old], %[_new], %[dest] \n\t"
      : [_old] "=&r" (old_value)
      : [_new] "r" (exchange_value), [dest] "r" (dest)
      : "memory");
    break;
  }

  return old_value;
}

template<>
template<typename T>
inline T Atomic::PlatformXchg<8>::operator()(T volatile* dest,
                                             T exchange_value,
                                             atomic_memory_order order) const {
  STATIC_ASSERT(8 == sizeof(T));
  T old_value;

  switch (order) {
  case memory_order_relaxed:
    asm volatile (
      "amswap.d %[_old], %[_new], %[dest] \n\t"
      : [_old] "=&r" (old_value)
      : [_new] "r" (exchange_value), [dest] "r" (dest)
      : "memory");
    break;
  default:
    asm volatile (
      "amswap_db.d %[_old], %[_new], %[dest] \n\t"
      : [_old] "=&r" (old_value)
      : [_new] "r" (exchange_value), [dest] "r" (dest)
      : "memory");
    break;
  }

  return old_value;
}

template<>
struct Atomic::PlatformCmpxchg<1> : Atomic::CmpxchgByteUsingInt {};

template<>
template<typename T>
inline T Atomic::PlatformCmpxchg<4>::operator()(T volatile* dest,
                                                T compare_value,
                                                T exchange_value,
                                                atomic_memory_order order) const {
  STATIC_ASSERT(4 == sizeof(T));
  T prev, temp;

  switch (order) {
  case memory_order_relaxed:
  case memory_order_release:
    asm volatile (
      "1: ll.w %[prev], %[dest]     \n\t"
      "   bne  %[prev], %[_old], 2f \n\t"
      "   move %[temp], %[_new]     \n\t"
      "   sc.w %[temp], %[dest]     \n\t"
      "   beqz %[temp], 1b          \n\t"
      "   b    3f                   \n\t"
      "2: dbar 0x700                \n\t"
      "3:                           \n\t"
      : [prev] "=&r" (prev), [temp] "=&r" (temp)
      : [_old] "r" (compare_value), [_new] "r" (exchange_value), [dest] "ZC" (*dest)
      : "memory");
    break;
  default:
    asm volatile (
      "1: ll.w %[prev], %[dest]     \n\t"
      "   bne  %[prev], %[_old], 2f \n\t"
      "   move %[temp], %[_new]     \n\t"
      "   sc.w %[temp], %[dest]     \n\t"
      "   beqz %[temp], 1b          \n\t"
      "   b    3f                   \n\t"
      "2: dbar 0x14                \n\t"
      "3:                           \n\t"
      : [prev] "=&r" (prev), [temp] "=&r" (temp)
      : [_old] "r" (compare_value), [_new] "r" (exchange_value), [dest] "ZC" (*dest)
      : "memory");
    break;
  }

  return prev;
}

template<>
template<typename T>
inline T Atomic::PlatformCmpxchg<8>::operator()(T volatile* dest,
                                                T compare_value,
                                                T exchange_value,
                                                atomic_memory_order order) const {
  STATIC_ASSERT(8 == sizeof(T));
  T prev, temp;

  switch (order) {
  case memory_order_relaxed:
  case memory_order_release:
    asm volatile (
      "1: ll.d %[prev], %[dest]     \n\t"
      "   bne  %[prev], %[_old], 2f \n\t"
      "   move %[temp], %[_new]     \n\t"
      "   sc.d %[temp], %[dest]     \n\t"
      "   beqz %[temp], 1b          \n\t"
      "   b    3f                   \n\t"
      "2: dbar 0x700                \n\t"
      "3:                           \n\t"
      : [prev] "=&r" (prev), [temp] "=&r" (temp)
      : [_old] "r" (compare_value), [_new] "r" (exchange_value), [dest] "ZC" (*dest)
      : "memory");
    break;
  default:
    asm volatile (
      "1: ll.d %[prev], %[dest]     \n\t"
      "   bne  %[prev], %[_old], 2f \n\t"
      "   move %[temp], %[_new]     \n\t"
      "   sc.d %[temp], %[dest]     \n\t"
      "   beqz %[temp], 1b          \n\t"
      "   b    3f                   \n\t"
      "2: dbar 0x14                 \n\t"
      "3:                           \n\t"
      : [prev] "=&r" (prev), [temp] "=&r" (temp)
      : [_old] "r" (compare_value), [_new] "r" (exchange_value), [dest] "ZC" (*dest)
      : "memory");
    break;
  }

  return prev;
}

template<>
struct Atomic::PlatformOrderedStore<4, RELEASE_X>
{
  template <typename T>
  void operator()(volatile T* p, T v) const { xchg(p, v, memory_order_release); }
};

template<>
struct Atomic::PlatformOrderedStore<8, RELEASE_X>
{
  template <typename T>
  void operator()(volatile T* p, T v) const { xchg(p, v, memory_order_release); }
};

template<>
struct Atomic::PlatformOrderedStore<4, RELEASE_X_FENCE>
{
  template <typename T>
  void operator()(volatile T* p, T v) const { xchg(p, v, memory_order_conservative); }
};

template<>
struct Atomic::PlatformOrderedStore<8, RELEASE_X_FENCE>
{
  template <typename T>
  void operator()(volatile T* p, T v) const { xchg(p, v, memory_order_conservative); }
};

#endif // OS_CPU_LINUX_LOONGARCH_ATOMIC_LINUX_LOONGARCH_HPP
