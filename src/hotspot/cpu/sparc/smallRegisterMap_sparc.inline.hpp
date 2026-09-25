/*
 * Copyright (c) 2019, 2022, Oracle and/or its affiliates. All rights reserved.
 * Copyright (c) 2026, SPARC continuation port contributors.
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

#ifndef CPU_SPARC_SMALLREGISTERMAP_SPARC_INLINE_HPP
#define CPU_SPARC_SMALLREGISTERMAP_SPARC_INLINE_HPP

#include "runtime/frame.inline.hpp"
#include "runtime/registerMap.hpp"

class SmallRegisterMap {
public:
  static constexpr SmallRegisterMap* instance = nullptr;
private:
  static bool is_saved_window_register(Register reg) {
    return reg->is_local() || reg->is_in() || reg->is_out();
  }

  static address pd_location(VMReg vmreg, intptr_t* window,
                             intptr_t* younger_window) {
    if (vmreg == nullptr || !vmreg->is_reg() || !vmreg->is_Register()) {
      return nullptr;
    }

    const int byte_adjust = vmreg->is_concrete() ? 0 : sizeof(jint);
    Register reg = vmreg->is_concrete()
        ? vmreg->as_Register() : vmreg->prev()->as_Register();
    if (!is_saved_window_register(reg)) {
      return nullptr;
    }

    if (reg->is_out()) {
      if (younger_window == nullptr) return nullptr;
      reg = reg->after_save();
      return (address)&younger_window[reg->sp_offset_in_saved_window()]
             + byte_adjust;
    }
    if (window == nullptr) return nullptr;
    return (address)&window[reg->sp_offset_in_saved_window()] + byte_adjust;
  }
public:
  // as_RegisterMap is used when we didn't want to templatize and abstract over RegisterMap type to support SmallRegisterMap
  // Consider enhancing SmallRegisterMap to support those cases
  const RegisterMap* as_RegisterMap() const { return nullptr; }
  RegisterMap* as_RegisterMap() { return nullptr; }

  RegisterMap* copy_to_RegisterMap(RegisterMap* map, intptr_t* sp) const {
    map->clear();
    map->set_include_argument_oops(false);
    frame::update_map_with_saved_link(
        map, (intptr_t**)&sp[FP->sp_offset_in_saved_window()]);
    return map;
  }

  SmallRegisterMap() {}
  SmallRegisterMap(const RegisterMap* map) { (void)map; }

  inline address location(VMReg reg, intptr_t* sp) const {
    return pd_location(reg, sp, nullptr);
  }

  static inline address location(VMReg reg, intptr_t* sp,
                                 intptr_t* younger_sp) {
    return pd_location(reg, sp, younger_sp);
  }

  inline void set_location(VMReg reg, address loc) {
    (void)reg;
    (void)loc;
  }

  JavaThread* thread() const {
  #ifndef ASSERT
    guarantee (false, "unreachable");
  #endif
    return nullptr;
  }

  bool update_map()    const { return false; }
  bool walk_cont()     const { return false; }
  bool include_argument_oops() const { return false; }
  void set_include_argument_oops(bool f)  {}
  bool in_cont()       const { return false; }
  stackChunkHandle stack_chunk() const { return stackChunkHandle(); }

#ifdef ASSERT
  bool should_skip_missing() const  { return false; }
  VMReg find_register_spilled_here(void* p, intptr_t* sp) { return I6->as_VMReg(); }
  void print() const { print_on(tty); }
  void print_on(outputStream* st) const { st->print_cr("Small register map"); }
#endif
};

#endif // CPU_SPARC_SMALLREGISTERMAP_SPARC_INLINE_HPP
