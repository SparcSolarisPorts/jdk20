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

#ifndef CPU_SPARC_CONTINUATIONFREEZETHAW_SPARC_INLINE_HPP
#define CPU_SPARC_CONTINUATIONFREEZETHAW_SPARC_INLINE_HPP

#include "code/codeBlob.inline.hpp"
#include "oops/stackChunkOop.inline.hpp"
#include "runtime/frame.hpp"
#include "runtime/frame.inline.hpp"
#include "runtime/prefetch.inline.hpp"

static constexpr int freeze_sparc_lesp_slot = 0;
static constexpr int freeze_sparc_llocals_slot = 3;
static constexpr int freeze_sparc_lmonitors_slot = 4;
static constexpr int freeze_sparc_llast_sp_slot = 5;
static constexpr int freeze_sparc_i5_slot = 13;
static constexpr int freeze_sparc_fp_slot = 14;
static constexpr int freeze_sparc_i7_slot = 15;

static inline intptr_t* freeze_sparc_decode_link(const frame& f) {
  intptr_t raw = f.sp()[freeze_sparc_fp_slot];
  if (f.is_heap_frame()) {
    return (raw > -max_jint && raw < max_jint) ? f.sp() + raw
                                                : (intptr_t*)raw;
  }
  return (intptr_t*)(raw + STACK_BIAS);
}

static inline void freeze_sparc_patch_link(const frame& f,
                                            intptr_t* target) {
  if (f.is_heap_frame()) {
    const intptr_t delta = (intptr_t)target - (intptr_t)f.sp();
    f.sp()[freeze_sparc_fp_slot] = delta > -max_jint && delta < max_jint
        ? delta : (intptr_t)target;
  } else {
    f.sp()[freeze_sparc_fp_slot] = (intptr_t)target - STACK_BIAS;
  }
}

static inline void freeze_sparc_patch_pc(const frame& f, address pc) {
  f.sp()[freeze_sparc_i7_slot] = (intptr_t)(f.is_heap_frame()
      ? pc
      : pc - frame::pc_return_offset);
}

static inline intptr_t* freeze_sparc_translate_pointer(
    const frame& from, const frame& to, intptr_t* value) {
  return to.sp() + (value - from.sp());
}

static inline void freeze_sparc_relativize_slot(
    const frame& source, const frame& heap, int slot, bool biased) {
  intptr_t raw = source.sp()[slot];
  intptr_t* value = biased ? (intptr_t*)(raw + STACK_BIAS)
                           : (intptr_t*)raw;
  // Pointers into this copied frame must follow the copy.  A saved link in
  // the bottom frame can point outside the chunk; keep that pointer absolute
  // until the thaw path reconnects it to the carrier stack.
  if (value >= source.sp() && value <= source.fp()) {
    intptr_t* translated = freeze_sparc_translate_pointer(source, heap, value);
    heap.sp()[slot] = translated - heap.fp();
  } else {
    heap.sp()[slot] = (intptr_t)value;
  }
}

static inline void freeze_sparc_derelativize_slot(
    const frame& heap, const frame& stack, int slot, bool biased) {
  intptr_t raw = heap.sp()[slot];
  intptr_t* heap_value = raw > -max_jint && raw < max_jint
      ? heap.fp() + raw : (intptr_t*)raw;
  intptr_t* stack_value = (heap_value >= heap.sp() && heap_value <= heap.fp())
      ? freeze_sparc_translate_pointer(heap, stack, heap_value) : heap_value;
  stack.sp()[slot] = biased ? (intptr_t)stack_value - STACK_BIAS
                            : (intptr_t)stack_value;
}

//// Freeze fast path

inline void FreezeBase::patch_stack_pd(intptr_t* frame_sp,
                                        intptr_t* heap_sp) {
  const intptr_t link = heap_sp[freeze_sparc_fp_slot];
  const bool relative = link > -max_jint && link < max_jint;
  intptr_t* target = relative ? heap_sp + link : (intptr_t*)link;
  intptr_t* stack_target = relative ? frame_sp + link : target;
  frame_sp[freeze_sparc_fp_slot] =
      (intptr_t)stack_target - STACK_BIAS;

  const address pc = (address)heap_sp[freeze_sparc_i7_slot];
  frame_sp[freeze_sparc_i7_slot] =
      (intptr_t)(pc - frame::pc_return_offset);
}

//// Freeze slow path

template<typename FKind>
inline frame FreezeBase::sender(const frame& f) {
  assert(FKind::is_instance(f), "wrong frame kind");

  intptr_t* sender_sp = f.fp();
  address sender_pc = f.sender_pc();
  intptr_t raw_sender_fp = sender_sp[freeze_sparc_fp_slot];
  intptr_t* sender_fp = (intptr_t*)(raw_sender_fp + STACK_BIAS);

  int slot = 0;
  CodeBlob* sender_cb = CodeCache::find_blob_and_oopmap(sender_pc, slot);
  frame result(sender_sp, sender_sp, sender_fp, sender_pc, sender_cb,
               slot == -1 || sender_cb == nullptr
                   ? nullptr
                   : sender_cb->oop_map_for_slot(slot, sender_pc),
               false /* on_heap */);
  result.set_younger_sp(f.sp());
  return result;
}

template<typename FKind>
frame FreezeBase::new_heap_frame(frame& f, frame& caller) {
  assert(FKind::is_instance(f), "wrong frame kind");

  const int fsize = FKind::size(f);
  intptr_t* heap_sp = caller.unextended_sp() - fsize;
  if (!FKind::interpreted && caller.is_interpreted_frame()) {
    heap_sp -= FKind::stack_argsize(f);
  }

  intptr_t* heap_fp = heap_sp + (f.fp() - f.unextended_sp());
  caller.set_sp(heap_sp + fsize);

  frame heap_frame(heap_sp, heap_sp, heap_fp, f.pc(), nullptr, nullptr,
                   true /* on_heap */);
  heap_frame.set_younger_sp(nullptr);
  return heap_frame;
}

inline void FreezeBase::adjust_interpreted_frame_unextended_sp(frame& f) {
  intptr_t raw = f.sp()[freeze_sparc_i5_slot];
  if (raw != 0) {
    f.set_unextended_sp(f.is_heap_frame() ? f.fp() + raw
                                          : (intptr_t*)(raw + STACK_BIAS));
  }
}

inline void FreezeBase::relativize_interpreted_frame_metadata(
    const frame& f, const frame& hf) {
  freeze_sparc_relativize_slot(f, hf, freeze_sparc_lesp_slot, false);
  freeze_sparc_relativize_slot(f, hf, freeze_sparc_llocals_slot, false);
  freeze_sparc_relativize_slot(f, hf, freeze_sparc_lmonitors_slot, false);
  freeze_sparc_relativize_slot(f, hf, freeze_sparc_llast_sp_slot, true);
  freeze_sparc_relativize_slot(f, hf, freeze_sparc_i5_slot, true);

  freeze_sparc_patch_link(hf, hf.fp());
  freeze_sparc_patch_pc(hf, f.sender_pc());
}

inline void FreezeBase::set_top_frame_metadata_pd(const frame& hf) {
  freeze_sparc_patch_link(hf, hf.fp());
  freeze_sparc_patch_pc(hf, hf.pc());
}

inline void FreezeBase::patch_pd(frame& hf, const frame& caller) {
  (void)hf;
  freeze_sparc_patch_link(caller, caller.fp());
}

//// Thaw fast path

inline void ThawBase::prefetch_chunk_pd(void* start, int size) {
  size <<= LogBytesPerWord;
  Prefetch::read(start, size);
  if (size >= 64) Prefetch::read(start, size - 64);
}

inline void ThawBase::patch_chunk_pd(intptr_t* sp) {
  sp[freeze_sparc_fp_slot] = (intptr_t)_cont.entryFP();
}

//// Thaw slow path

inline frame ThawBase::new_entry_frame() {
  frame result(_cont.entrySP(), _cont.entrySP(), _cont.entryFP(),
               _cont.entryPC());
  result.set_younger_sp(nullptr);
  return result;
}

template<typename FKind>
frame ThawBase::new_stack_frame(const frame& hf, frame& caller,
                                bool bottom) {
  assert(FKind::is_instance(hf), "wrong frame kind");

  int fsize = FKind::size(hf);
  intptr_t* frame_sp = caller.unextended_sp() - fsize;

  if (!FKind::interpreted &&
      (bottom || caller.is_interpreted_frame())) {
    const int argsize = hf.compiled_frame_stack_argsize();
    frame_sp -= argsize;
    caller.set_sp(caller.sp() - argsize);
    frame_sp = align(hf, frame_sp, caller, bottom);
  }

  intptr_t* frame_fp = frame_sp + (hf.fp() - hf.unextended_sp());
  frame result(frame_sp, frame_sp, frame_fp, hf.pc(), hf.cb(), hf.oop_map(),
               false /* on_heap */);
  result.set_younger_sp(nullptr);
  return result;
}

inline intptr_t* ThawBase::align(const frame& hf, intptr_t* frame_sp,
                                  frame& caller, bool bottom) {
  (void)hf;
  (void)bottom;
  if (!is_aligned(frame_sp, frame::frame_alignment)) {
    --frame_sp;
    caller.set_sp(caller.sp() - 1);
  }
  assert(is_aligned(frame_sp, frame::frame_alignment),
         "SPARC stack must remain 16-byte aligned");
  return frame_sp;
}

inline void ThawBase::patch_pd(frame& f, const frame& caller) {
  (void)f;
  freeze_sparc_patch_link(caller, caller.fp());
}

inline void ThawBase::derelativize_interpreted_frame_metadata(
    const frame& hf, const frame& f) {
  freeze_sparc_derelativize_slot(hf, f, freeze_sparc_lesp_slot, false);
  freeze_sparc_derelativize_slot(hf, f, freeze_sparc_llocals_slot, false);
  freeze_sparc_derelativize_slot(hf, f, freeze_sparc_lmonitors_slot, false);
  freeze_sparc_derelativize_slot(hf, f, freeze_sparc_llast_sp_slot, true);
  freeze_sparc_derelativize_slot(hf, f, freeze_sparc_i5_slot, true);

  freeze_sparc_patch_link(f, f.fp());
  freeze_sparc_patch_pc(f, hf.pc());
}

inline void ThawBase::set_interpreter_frame_bottom(const frame& f,
                                                    intptr_t* bottom) {
  intptr_t* locals = bottom -
      f.interpreter_frame_method()->max_locals()
      * Interpreter::stackElementWords;
  f.sp()[freeze_sparc_llocals_slot] = (intptr_t)locals;
}

#endif // CPU_SPARC_CONTINUATIONFREEZETHAW_SPARC_INLINE_HPP
