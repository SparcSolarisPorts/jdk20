/*
 * Copyright (c) 2020 SAP SE. All rights reserved.
 * Copyright (c) 2020, 2022, Oracle and/or its affiliates. All rights reserved.
 * Copyright (c) 2026, SPARC JDK port contributors. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.
 */

#include "precompiled.hpp"
#include "asm/macroAssembler.inline.hpp"
#include "code/codeBlob.hpp"
#include "code/codeCache.hpp"
#include "code/vmreg.inline.hpp"
#include "compiler/oopMap.hpp"
#include "logging/logStream.hpp"
#include "memory/resourceArea.hpp"
#include "prims/downcallLinker.hpp"
#include "runtime/javaFrameAnchor.hpp"
#include "runtime/sharedRuntime.hpp"
#include "runtime/stackOverflow.hpp"
#include "runtime/stubCodeGenerator.hpp"
#include "utilities/align.hpp"

#define __ _masm->

// A save instruction rotates SPARC's register window. If an ABI descriptor
// ever chooses an outgoing register for an internal value (target/return
// buffer), that value is visible through the corresponding incoming register
// after the generated stub opens its frame. Globals do not rotate.
static Register after_save_if_out(Register r) {
  return r->is_out() ? r->after_save() : r;
}

class DowncallStubGenerator : public StubCodeGenerator {
  BasicType* _signature;
  int _num_args;
  BasicType _ret_bt;
  const ABIDescriptor& _abi;
  const GrowableArray<VMReg>& _input_registers;
  const GrowableArray<VMReg>& _output_registers;
  bool _needs_return_buffer;

  int _frame_complete;
  // Number of 4-byte VMReg stack slots. RuntimeStub wants words, while
  // OopMap wants VMReg slots, matching the x86/AArch64 implementations.
  int _framesize;
  OopMapSet* _oop_maps;

 public:
  DowncallStubGenerator(CodeBuffer* buffer,
                        BasicType* signature,
                        int num_args,
                        BasicType ret_bt,
                        const ABIDescriptor& abi,
                        const GrowableArray<VMReg>& input_registers,
                        const GrowableArray<VMReg>& output_registers,
                        bool needs_return_buffer)
    : StubCodeGenerator(buffer, PrintMethodHandleStubs),
      _signature(signature),
      _num_args(num_args),
      _ret_bt(ret_bt),
      _abi(abi),
      _input_registers(input_registers),
      _output_registers(output_registers),
      _needs_return_buffer(needs_return_buffer),
      _frame_complete(0),
      _framesize(0),
      _oop_maps(NULL) {}

  void generate();

  int frame_complete() const { return _frame_complete; }
  int framesize() const {
    return _framesize >> (LogBytesPerWord - LogBytesPerInt);
  }
  OopMapSet* oop_maps() const { return _oop_maps; }
};

// SPARC emits more instructions than x86/AArch64 for the transition and
// register-window bookkeeping. Keep enough headroom so a diagnostic build
// does not fail merely because of stub-code expansion.
static const int native_invoker_code_size = 4096;

RuntimeStub* DowncallLinker::make_downcall_stub(BasicType* signature,
                                                int num_args,
                                                BasicType ret_bt,
                                                const ABIDescriptor& abi,
                                                const GrowableArray<VMReg>& input_registers,
                                                const GrowableArray<VMReg>& output_registers,
                                                bool needs_return_buffer) {
  const int locs_size = 128;
  CodeBuffer code("nep_invoker_blob", native_invoker_code_size, locs_size);
  DowncallStubGenerator g(&code, signature, num_args, ret_bt, abi,
                          input_registers, output_registers,
                          needs_return_buffer);
  g.generate();
  code.log_section_sizes("nep_invoker_blob");

  RuntimeStub* stub = RuntimeStub::new_runtime_stub("nep_invoker_blob",
                                                    &code,
                                                    g.frame_complete(),
                                                    g.framesize(),
                                                    g.oop_maps(), false);
#ifndef PRODUCT
  LogTarget(Trace, foreign, downcall) lt;
  if (lt.is_enabled()) {
    ResourceMark rm;
    LogStream ls(lt);
    stub->print_on(&ls);
  }
#endif

  return stub;
}

void DowncallStubGenerator::generate() {
  // L registers survive an ordinary C call through SPARC register-window
  // semantics. L7 is the conventional HotSpot cache for G2_thread.
  Register shuffle_reg = L0;
  Register tmp1 = L1;
  Register tmp2 = L2;

  JavaCallingConvention in_conv;
  NativeCallingConvention out_conv(_input_registers);
  ArgumentShuffle arg_shuffle(_signature, _num_args,
                              _signature, _num_args,
                              &in_conv, &out_conv,
                              shuffle_reg->as_VMReg());

#ifndef PRODUCT
  LogTarget(Trace, foreign, downcall) lt;
  if (lt.is_enabled()) {
    ResourceMark rm;
    LogStream ls(lt);
    arg_shuffle.print_on(&ls);
  }
#endif

  assert(_abi._shadow_space_bytes == 0,
         "SPARC V9 does not use x64-style shadow space");
  assert(_abi._stack_alignment_bytes == StackAlignmentInBytes,
         "SPARC V9 stack alignment mismatch");
  // linkToNative uses G3 as the temporary downcall-stub jump register.
  // The current Solaris/SPARC FFM ABI may therefore carry the C target in G1.
  assert(_abi._target_addr_reg != G3_scratch,
         "G3 is clobbered by linkToNative before the downcall stub");

  // SPARC V9 frame layout, in bytes from the current architectural SP
  // after adding STACK_BIAS:
  //   [0, 128)   register-window save area
  //   [128,176)  six integer parameter-array slots
  //   [176,...)  overflow native arguments
  //
  // Java stack VMRegs, by contrast, are idealized above only the 128-byte
  // register-save area. ArgumentShuffle::pd_generate handles that distinction
  // when told the native output-stack bias below.
  const int abi_area_bytes = frame::memory_parameter_word_sp_offset * wordSize;
  const int native_stack_bytes =
      arg_shuffle.out_arg_stack_slots() << LogBytesPerInt;

  int allocated_bytes = native_stack_bytes;

  int ret_buf_addr_sp_offset = -1;
  if (_needs_return_buffer) {
    // Keep the return-buffer address beyond all outgoing stack arguments so
    // the native callee cannot overwrite it through its parameter array.
    ret_buf_addr_sp_offset = abi_area_bytes + allocated_bytes;
    allocated_bytes += wordSize;
  }

  RegSpiller out_reg_spiller(_output_registers);
  int result_spill_sp_offset = -1;
  if (!_needs_return_buffer) {
    // After the native call has returned its outgoing argument area is dead,
    // so the slow-path result spill may reuse it. This mirrors the upstream
    // x86/AArch64 layout and keeps the frame compact.
    result_spill_sp_offset = abi_area_bytes;
    allocated_bytes = MAX2(allocated_bytes,
                           out_reg_spiller.spill_size_bytes());
  }

  allocated_bytes = align_up(allocated_bytes, StackAlignmentInBytes);
  const int extra_words = allocated_bytes / wordSize;
  // total_frame_size_in_bytes() is an instance helper on the SPARC
  // MacroAssembler (save_frame() uses the same helper), not a static method.
  const int frame_bytes =
      _masm->total_frame_size_in_bytes(extra_words);
  _framesize = frame_bytes >> LogBytesPerInt; // 4-byte VMReg slots

  _oop_maps = new OopMapSet();
  address start = __ pc();

  __ save_frame(extra_words);
  _frame_complete = __ pc() - start;

  // Spill the return-buffer pointer immediately after opening the SPARC
  // window.  This must happen before set_last_Java_frame(), which uses G4 as
  // an architectural scratch register.  It also makes the stub compatible
  // with the recommended SPARC arranger choice G3=target, G4=retbuf.
  if (_needs_return_buffer) {
    assert(ret_buf_addr_sp_offset >= abi_area_bytes,
           "missing return-buffer spill");
    Register ret_buf_reg = after_save_if_out(_abi._ret_buf_addr_reg);
    __ stx(ret_buf_reg, SP, ret_buf_addr_sp_offset + STACK_BIAS);
  }

  // Record a PC inside this RuntimeStub. Native code does not necessarily
  // provide Java-walkable frame linkage, so SPARC needs an explicit anchor
  // PC just like its normal JNI wrappers.
  address anchor_pc = (address)__ load_pc_address(tmp1, 0);
  __ set_last_Java_frame(SP, tmp1);
  _oop_maps->add_gc_map(anchor_pc - start, new OopMap(_framesize, 0));

  // Stack walkers cannot ask a thread executing arbitrary native code to
  // materialize live SPARC register windows. Flush before publishing native
  // state and mark the JavaFrameAnchor accordingly (same protocol as the
  // SPARC interpreter/compiled JNI wrappers).
  __ flushw();
  __ set(JavaFrameAnchor::flushed, tmp1);
  __ st(tmp1, G2_thread,
        JavaThread::frame_anchor_offset() + JavaFrameAnchor::flags_offset());

  // Publish native state while G2 is still valid, then cache the thread in L7.
  // save_thread() deliberately poisons G2 in VerifyThread builds, and its
  // verification helper may clobber %o registers, so do it before placing the
  // foreign arguments in %o0-%o5.
  __ set(_thread_in_native, tmp1);
  __ st(tmp1, G2_thread, JavaThread::thread_state_offset());
  __ save_thread(L7_thread_cache);

  __ block_comment("{ argument shuffle");
  // Java input stack locations are based at the caller's FP plus the normal
  // register-save area. Native overflow arguments begin after the SPARC V9
  // register save + six-slot parameter array (176 bytes on LP64).
  arg_shuffle.generate(_masm, shuffle_reg->as_VMReg(),
                       0, abi_area_bytes);

  __ block_comment("} argument shuffle");

  Register target = after_save_if_out(_abi._target_addr_reg);
  __ callr(target, 0);
  __ delayed()->nop();

  __ restore_thread(L7_thread_cache);
  __ reinit_heapbase();

  if (!_needs_return_buffer) {
    // Normalize scalar C results to the Java carrier representation. Native
    // integer/pointer results arrive in %o0 in this window; FP results remain
    // in %f0 and are unaffected by register-window rotation.
    switch (_ret_bt) {
      case T_VOID:
      case T_LONG:
      case T_FLOAT:
      case T_DOUBLE:
        break;
      case T_BOOLEAN:
        __ subcc(G0, O0, G0);
        __ addc(G0, 0, O0);
        break;
      case T_BYTE:
        __ sll(O0, 24, O0);
        __ sra(O0, 24, O0);
        break;
      case T_CHAR:
        __ sll(O0, 16, O0);
        __ srl(O0, 16, O0);
        break;
      case T_SHORT:
        __ sll(O0, 16, O0);
        __ sra(O0, 16, O0);
        break;
      case T_INT:
        __ sra(O0, 0, O0); // sign-extend low 32 bits on V9
        break;
      default:
        ShouldNotReachHere();
    }
  } else {
    // Aggregate return support is only valid when the Java arranger has
    // explicitly requested a return buffer. Copy each ABI return storage into
    // that buffer. The Java layer should continue rejecting unsupported
    // aggregate shapes until its dedicated classification is implemented.
    __ ldx(SP, ret_buf_addr_sp_offset + STACK_BIAS, tmp1);
    int offset = 0;
    for (int i = 0; i < _output_registers.length(); i++) {
      VMReg reg = _output_registers.at(i);
      if (reg->is_Register()) {
        __ stx(reg->as_Register(), tmp1, offset);
        offset += wordSize;
      } else if (reg->is_FloatRegister()) {
        __ stf(FloatRegisterImpl::D, reg->as_FloatRegister(), tmp1, offset);
        // FFM VMStorage vector lanes use 16-byte result-buffer slots.
        offset += 16;
      } else {
        ShouldNotReachHere();
      }
    }
  }

  // Native -> native_trans. The StoreLoad barrier is required before checking
  // the safepoint/suspend state, matching the existing SPARC JNI wrappers.
  __ set(_thread_in_native_trans, tmp1);
  __ st(tmp1, G2_thread, JavaThread::thread_state_offset());
  __ membar(Assembler::StoreLoad);

  Label L_after_safepoint;
  Label L_safepoint_slow;

  __ safepoint_poll(L_safepoint_slow, false, G2_thread, tmp1);
  __ delayed()->ld(G2_thread, JavaThread::suspend_flags_offset(), tmp1);
  __ cmp_and_br_short(tmp1, 0, Assembler::equal, Assembler::pt,
                      L_after_safepoint);
  __ bind(L_safepoint_slow);

  if (!_needs_return_buffer) {
    out_reg_spiller.generate_spill(_masm, result_spill_sp_offset);
  }

  __ call_VM_leaf(L7_thread_cache,
                  CAST_FROM_FN_PTR(address,
                      JavaThread::check_special_condition_for_native_trans),
                  G2_thread);

  if (!_needs_return_buffer) {
    out_reg_spiller.generate_fill(_masm, result_spill_sp_offset);
  }

  __ bind(L_after_safepoint);

  __ set(_thread_in_Java, tmp1);
  __ st(tmp1, G2_thread, JavaThread::thread_state_offset());

  // Re-enable yellow/reserved stack pages if native execution disabled them.
  Label L_after_reguard;
  __ ld(G2_thread, JavaThread::stack_guard_state_offset(), tmp1);
  __ cmp_and_br_short(tmp1,
                      StackOverflow::stack_guard_yellow_reserved_disabled,
                      Assembler::notEqual, Assembler::pt,
                      L_after_reguard);

  if (!_needs_return_buffer) {
    out_reg_spiller.generate_spill(_masm, result_spill_sp_offset);
  }

  __ call(CAST_FROM_FN_PTR(address, SharedRuntime::reguard_yellow_pages),
          relocInfo::runtime_call_type);
  __ delayed()->nop();
  __ restore_thread(L7_thread_cache);
  __ reinit_heapbase();

  if (!_needs_return_buffer) {
    out_reg_spiller.generate_fill(_masm, result_spill_sp_offset);
  }

  __ bind(L_after_reguard);

  __ reset_last_Java_frame();

  // restore rotates this window's %i registers into the caller's %o registers.
  // Move integer/pointer results from native %o0 into %i0 first. Floating
  // results stay in %f0 and do not rotate.
  if (!_needs_return_buffer &&
      _ret_bt != T_VOID && _ret_bt != T_FLOAT && _ret_bt != T_DOUBLE) {
    __ mov(O0, I0);
  }

  __ ret();
  __ delayed()->restore();

  __ flush();
}

#undef __
