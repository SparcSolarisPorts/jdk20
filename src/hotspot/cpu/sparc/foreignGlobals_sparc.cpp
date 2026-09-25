/*
 * Copyright (c) 2020 SAP SE. All rights reserved.
 * Copyright (c) 2020, 2022, Oracle and/or its affiliates. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.
 */

#include "precompiled.hpp"
#include "assembler_sparc.inline.hpp"
#include "macroAssembler_sparc.inline.hpp"
#include "vmreg_sparc.inline.hpp"
#include "classfile/javaClasses.hpp"
#include "oops/oopCast.inline.hpp"
#include "prims/foreignGlobals.inline.hpp"
#include "runtime/jniHandles.inline.hpp"
#include "runtime/sharedRuntime.hpp"

bool ABIDescriptor::is_volatile_reg(Register reg) const {
  return _integer_argument_registers.contains(reg) ||
         _integer_additional_volatile_registers.contains(reg);
}

bool ABIDescriptor::is_volatile_reg(FloatRegister reg) const {
  return _vector_argument_registers.contains(reg) ||
         _vector_additional_volatile_registers.contains(reg);
}

static constexpr int INTEGER_TYPE = 0;
static constexpr int VECTOR_TYPE = 1;
static constexpr int STACK_TYPE = 3;

const ABIDescriptor ForeignGlobals::parse_abi_descriptor(jobject jabi) {
  oop abi_oop = JNIHandles::resolve_non_null(jabi);
  ABIDescriptor abi;
  objArrayOop input_storage = jdk_internal_foreign_abi_ABIDescriptor::inputStorage(abi_oop);
  parse_register_array(input_storage, INTEGER_TYPE, abi._integer_argument_registers, as_Register);
  parse_register_array(input_storage, VECTOR_TYPE, abi._vector_argument_registers, as_FloatRegister);
  objArrayOop output_storage = jdk_internal_foreign_abi_ABIDescriptor::outputStorage(abi_oop);
  parse_register_array(output_storage, INTEGER_TYPE, abi._integer_return_registers, as_Register);
  parse_register_array(output_storage, VECTOR_TYPE, abi._vector_return_registers, as_FloatRegister);
  objArrayOop volatile_storage = jdk_internal_foreign_abi_ABIDescriptor::volatileStorage(abi_oop);
  parse_register_array(volatile_storage, INTEGER_TYPE, abi._integer_additional_volatile_registers, as_Register);
  parse_register_array(volatile_storage, VECTOR_TYPE, abi._vector_additional_volatile_registers, as_FloatRegister);
  abi._stack_alignment_bytes = jdk_internal_foreign_abi_ABIDescriptor::stackAlignment(abi_oop);
  abi._shadow_space_bytes = jdk_internal_foreign_abi_ABIDescriptor::shadowSpace(abi_oop);
  abi._target_addr_reg = parse_vmstorage(jdk_internal_foreign_abi_ABIDescriptor::targetAddrStorage(abi_oop))->as_Register();
  abi._ret_buf_addr_reg = parse_vmstorage(jdk_internal_foreign_abi_ABIDescriptor::retBufAddrStorage(abi_oop))->as_Register();
  return abi;
}

VMReg ForeignGlobals::vmstorage_to_vmreg(int type, int index) {
  switch (type) {
    case INTEGER_TYPE: return ::as_Register(index)->as_VMReg();
    case VECTOR_TYPE:  return ::as_FloatRegister(index)->as_VMReg();
    case STACK_TYPE:   return VMRegImpl::stack2reg(index * 2);
    default:           return VMRegImpl::Bad();
  }
}

int RegSpiller::pd_reg_size(VMReg reg) {
  return (reg->is_Register() || reg->is_FloatRegister()) ? wordSize : 0;
}

void RegSpiller::pd_store_reg(MacroAssembler* masm, int offset, VMReg reg) {
  if (reg->is_Register()) {
    masm->stx(reg->as_Register(), SP, offset + STACK_BIAS);
  } else if (reg->is_FloatRegister()) {
    masm->stf(FloatRegisterImpl::D, reg->as_FloatRegister(), SP, offset + STACK_BIAS);
  }
}

void RegSpiller::pd_load_reg(MacroAssembler* masm, int offset, VMReg reg) {
  if (reg->is_Register()) {
    masm->ldx(SP, offset + STACK_BIAS, reg->as_Register());
  } else if (reg->is_FloatRegister()) {
    masm->ldf(FloatRegisterImpl::D, SP, offset + STACK_BIAS, reg->as_FloatRegister());
  }
}

// VMReg stack locations are numbered in four-byte slots.  Incoming values
// belong to the caller frame (FP); outgoing native values belong to this frame
// (SP).  The supplied biases account for each generated stub's frame layout.
static int foreign_stack_offset(VMReg reg, int bias, bool java_argument) {
  int slots = reg->reg2stack();
  if (java_argument) {
    // Java VMReg stack slots start above SPARC's register-window save area.
    slots += SharedRuntime::out_preserve_stack_slots();
  }
  return slots * VMRegImpl::stack_slot_size + bias + STACK_BIAS;
}

void ArgumentShuffle::pd_generate(MacroAssembler* masm, VMReg tmp, int in_stk_bias, int out_stk_bias) const {
  Register scratch = tmp->as_Register();
  for (int i = 0; i < _moves.length(); i++) {
    const Move move = _moves.at(i);
    VMReg src = move.from.first();
    const VMReg dst = move.to.first();

    // ArgumentShuffle runs after the stub has opened a SPARC register
    // window.  An argument that arrived in an outgoing register is therefore
    // visible through the corresponding incoming register in this window.
    if (src->is_Register() && src->as_Register()->is_out()) {
      src = src->as_Register()->after_save()->as_VMReg();
    }
    const bool wide = move.bt == T_LONG || move.bt == T_DOUBLE;
    const bool floating = move.bt == T_FLOAT || move.bt == T_DOUBLE;
    const FloatRegisterImpl::Width fp_width = wide ? FloatRegisterImpl::D : FloatRegisterImpl::S;

    assert(src->is_reg() || src->is_stack(), "bad source");
    assert(dst->is_reg() || dst->is_stack(), "bad destination");
    masm->block_comment(err_msg("foreign shuffle: %s", null_safe_string(type2name(move.bt))));

    if (floating && src->is_FloatRegister() && dst->is_FloatRegister()) {
      if (src != dst) masm->fmov(fp_width, src->as_FloatRegister(), dst->as_FloatRegister());
    } else if (!floating && src->is_Register() && dst->is_Register()) {
      if (src != dst) masm->mov(src->as_Register(), dst->as_Register());
    } else if (src->is_stack() && dst->is_stack()) {
      if (wide) masm->ldx(FP, foreign_stack_offset(src, in_stk_bias, true), scratch);
      else      masm->ld(FP, foreign_stack_offset(src, in_stk_bias, true), scratch);
      if (wide) masm->stx(scratch, SP, foreign_stack_offset(dst, out_stk_bias, false));
      else      masm->st(scratch, SP, foreign_stack_offset(dst, out_stk_bias, false));
    } else if (src->is_stack()) {
      const int off = foreign_stack_offset(src, in_stk_bias, true);
      if (dst->is_FloatRegister()) masm->ldf(fp_width, FP, off, dst->as_FloatRegister());
      else if (wide) masm->ldx(FP, off, dst->as_Register());
      else masm->ld(FP, off, dst->as_Register());
    } else if (dst->is_stack()) {
      const int off = foreign_stack_offset(dst, out_stk_bias, false);
      if (src->is_FloatRegister()) masm->stf(fp_width, src->as_FloatRegister(), SP, off);
      else if (wide) masm->stx(src->as_Register(), SP, off);
      else masm->st(src->as_Register(), SP, off);
    } else if (src->is_Register() && dst->is_FloatRegister()) {
      // The stub frame reserves a scratch word immediately below FP.
      if (wide) masm->stx(src->as_Register(), FP, -8 + STACK_BIAS);
      else masm->st(src->as_Register(), FP, -4 + STACK_BIAS);
      masm->ldf(fp_width, FP, wide ? -8 + STACK_BIAS : -4 + STACK_BIAS, dst->as_FloatRegister());
    } else if (src->is_FloatRegister() && dst->is_Register()) {
      if (wide) masm->stf(fp_width, src->as_FloatRegister(), FP, -8 + STACK_BIAS);
      else masm->stf(fp_width, src->as_FloatRegister(), FP, -4 + STACK_BIAS);
      if (wide) masm->ldx(FP, -8 + STACK_BIAS, dst->as_Register());
      else masm->ld(FP, -4 + STACK_BIAS, dst->as_Register());
    } else {
      ShouldNotReachHere();
    }
  }
}
