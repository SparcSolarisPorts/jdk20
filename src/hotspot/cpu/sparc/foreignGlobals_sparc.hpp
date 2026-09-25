/*
 * Copyright (c) 2020 SAP SE. All rights reserved.
 * Copyright (c) 2022, Oracle and/or its affiliates. All rights reserved.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.
 */

#ifndef CPU_SPARC_VM_FOREIGN_GLOBALS_SPARC_HPP
#define CPU_SPARC_VM_FOREIGN_GLOBALS_SPARC_HPP

#include "asm/macroAssembler.hpp"
#include "utilities/growableArray.hpp"

// SPARC V9 floating-point argument and result registers carry scalar
// single- or double-precision values.  The foreign linker spills one
// architectural FP register per storage location.
constexpr size_t float_reg_size = 8; // bytes

struct ABIDescriptor {
  GrowableArray<Register> _integer_argument_registers;
  GrowableArray<Register> _integer_return_registers;
  GrowableArray<FloatRegister> _vector_argument_registers;
  GrowableArray<FloatRegister> _vector_return_registers;

  GrowableArray<Register> _integer_additional_volatile_registers;
  GrowableArray<FloatRegister> _vector_additional_volatile_registers;

  int32_t _stack_alignment_bytes;
  int32_t _shadow_space_bytes;

  Register _target_addr_reg;
  Register _ret_buf_addr_reg;

  bool is_volatile_reg(Register reg) const;
  bool is_volatile_reg(FloatRegister reg) const;
};

#endif // CPU_SPARC_VM_FOREIGN_GLOBALS_SPARC_HPP
