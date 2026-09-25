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

#ifndef CPU_SPARC_CONTINUATIONHELPER_SPARC_INLINE_HPP
#define CPU_SPARC_CONTINUATIONHELPER_SPARC_INLINE_HPP

#include "runtime/continuationHelper.hpp"

#include "runtime/continuationEntry.inline.hpp"
#include "runtime/frame.inline.hpp"
#include "runtime/registerMap.hpp"
#include "runtime/stubRoutines.hpp"
#include "utilities/align.hpp"

// Register-window save slots, in machine words from an unbiased SP.
static constexpr int sparc_l0_slot = 0;
static constexpr int sparc_i0_slot = 8;
static constexpr int sparc_i5_saved_sp_slot = sparc_i0_slot + 5;
static constexpr int sparc_fp_slot = sparc_i0_slot + 6;
static constexpr int sparc_i7_slot = sparc_i0_slot + 7;

static inline intptr_t* sparc_decode_saved_sp(const frame& f, intptr_t raw) {
  if (f.is_heap_frame()) {
    // In a chunk, an in-chunk link is represented as a word displacement
    // from this frame's SP.  A link outside the chunk is stored as an
    // ordinary unbiased pointer.
    if (raw > -max_jint && raw < max_jint) {
      return f.sp() + raw;
    }
    return (intptr_t*)raw;
  }
  return (intptr_t*)(raw + STACK_BIAS);
}

static inline intptr_t sparc_encode_saved_sp(const frame& f, intptr_t* target) {
  if (target == nullptr) return 0;
  return f.is_heap_frame() ? (intptr_t)(target - f.sp())
                           : (intptr_t)target - STACK_BIAS;
}

static inline address sparc_decode_saved_pc(const frame& f, address raw) {
  return f.is_heap_frame() ? raw : raw + frame::pc_return_offset;
}

static inline address sparc_encode_saved_pc(const frame& f, address pc) {
  return f.is_heap_frame() ? pc : pc - frame::pc_return_offset;
}

template<typename FKind>
static inline intptr_t** link_address(const frame& f) {
  assert(FKind::is_instance(f), "wrong frame kind");
  return (intptr_t**)&f.sp()[sparc_fp_slot];
}

inline int ContinuationHelper::frame_align_words(int size) {
  return size & 1;
}

inline intptr_t* ContinuationHelper::frame_align_pointer(intptr_t* sp) {
  return align_down(sp, frame::frame_alignment);
}

template<typename FKind>
inline void ContinuationHelper::update_register_map(const frame& f,
                                                     RegisterMap* map) {
  assert(FKind::is_instance(f), "wrong frame kind");
  map->shift_window(f.sp(), f.younger_sp_or_null());
}

inline void ContinuationHelper::update_register_map_with_callee(
    const frame& f, RegisterMap* map) {
  map->shift_window(f.sp(), f.younger_sp_or_null());
}

inline void ContinuationHelper::push_pd(const frame& f) {
  // Register windows must be stack-resident before a continuation walk.
  StubRoutines::Sparc::flush_callers_register_windows_func()();
  f.sp()[sparc_fp_slot] = sparc_encode_saved_sp(f, f.fp());
}

inline void ContinuationHelper::set_anchor_to_entry_pd(
    JavaFrameAnchor* anchor, ContinuationEntry* entry) {
  (void)entry;
  anchor->set_flags(JavaFrameAnchor::flushed);
}

#ifdef ASSERT
inline void ContinuationHelper::set_anchor_pd(JavaFrameAnchor* anchor,
                                               intptr_t* sp) {
  (void)sp;
  anchor->set_flags(JavaFrameAnchor::flushed);
}

inline bool ContinuationHelper::Frame::assert_frame_laid_out(frame f) {
  const intptr_t* slot = &f.sp()[sparc_fp_slot];
  const intptr_t* decoded = sparc_decode_saved_sp(f, *slot);
  assert(decoded == f.fp(), "saved %%i6 does not match frame pointer");
  return decoded == f.fp();
}
#endif

inline intptr_t** ContinuationHelper::Frame::callee_link_address(
    const frame& f) {
  return (intptr_t**)&f.sp()[sparc_fp_slot];
}

inline address* ContinuationHelper::InterpretedFrame::return_pc_address(
    const frame& f) {
  return (address*)&f.sp()[sparc_i7_slot];
}

inline void ContinuationHelper::InterpretedFrame::patch_sender_sp(
    frame& f, intptr_t* sp) {
  f.sp()[sparc_i5_saved_sp_slot] = f.is_heap_frame()
      ? (intptr_t)(sp - f.fp())
      : (intptr_t)sp - STACK_BIAS;
  f.set_interpreter_frame_sender_sp(sp);
}

inline address* ContinuationHelper::Frame::return_pc_address(const frame& f) {
  return (address*)&f.sp()[sparc_i7_slot];
}

inline address ContinuationHelper::Frame::real_pc(const frame& f) {
  return sparc_decode_saved_pc(f, *return_pc_address(f));
}

inline void ContinuationHelper::Frame::patch_pc(const frame& f, address pc) {
  *return_pc_address(f) = sparc_encode_saved_pc(f, pc);
}

inline intptr_t* ContinuationHelper::InterpretedFrame::frame_top(
    const frame& f, InterpreterOopMap* mask) {
  (void)mask;
  // Copying from unextended_sp is conservative and preserves the complete
  // SPARC register-save/outgoing-argument area.  Dead expression slots can
  // be optimized later without changing the representation.
  return f.unextended_sp();
}

inline intptr_t* ContinuationHelper::InterpretedFrame::frame_bottom(
    const frame& f) {
  Method* method = f.interpreter_frame_method();
  intptr_t raw_locals = f.sp()[sparc_l0_slot + 3];
  intptr_t* locals = f.is_heap_frame() ? f.fp() + raw_locals
                                        : (intptr_t*)raw_locals;
  return locals + method->max_locals() * Interpreter::stackElementWords;
}

inline intptr_t* ContinuationHelper::InterpretedFrame::frame_top(
    const frame& f, int callee_argsize, bool callee_interpreted) {
  return f.unextended_sp() + (callee_interpreted ? callee_argsize : 0);
}

#endif // CPU_SPARC_CONTINUATIONHELPER_SPARC_INLINE_HPP
