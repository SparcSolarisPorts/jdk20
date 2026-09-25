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

#ifndef CPU_SPARC_STACKCHUNKFRAMESTREAM_SPARC_INLINE_HPP
#define CPU_SPARC_STACKCHUNKFRAMESTREAM_SPARC_INLINE_HPP

#include "interpreter/oopMapCache.hpp"
#include "runtime/continuationHelper.hpp"
#include "runtime/frame.inline.hpp"
#include "runtime/registerMap.hpp"
#include "runtime/smallRegisterMap.inline.hpp"

static constexpr int stack_chunk_sparc_i5_slot = 13;
static constexpr int stack_chunk_sparc_fp_slot = 14;
static constexpr int stack_chunk_sparc_i7_slot = 15;

#ifdef ASSERT
template <ChunkFrames frame_kind>
inline bool StackChunkFrameStream<frame_kind>::is_in_frame(void* p0) const {
  assert(!is_done(), "stream is done");
  intptr_t* p = (intptr_t*)p0;
  intptr_t* low = sp();
  intptr_t* high = is_interpreted()
      ? ContinuationHelper::InterpretedFrame::frame_bottom(to_frame())
      : unextended_sp() + _cb->frame_size() + stack_argsize();
  return p >= low && p < high;
}
#endif

template <ChunkFrames frame_kind>
inline frame StackChunkFrameStream<frame_kind>::to_frame() const {
  if (is_done()) {
    frame result(_sp, _sp, nullptr, nullptr, nullptr, nullptr,
                 true /* on_heap */);
    result.set_younger_sp(nullptr);
    return result;
  }
  frame result(sp(), unextended_sp(), fp(), pc(), cb(), _oopmap,
               true /* on_heap */);
  result.set_younger_sp(_pd_younger_sp);
  return result;
}

template <ChunkFrames frame_kind>
inline address StackChunkFrameStream<frame_kind>::get_pc() const {
  assert(!is_done(), "stream is done");
  return (address)_sp[stack_chunk_sparc_i7_slot];
}

template <ChunkFrames frame_kind>
inline intptr_t* StackChunkFrameStream<frame_kind>::fp() const {
  intptr_t raw = _sp[stack_chunk_sparc_fp_slot];
  // Internal chunk links are word displacements.  A bottom link may remain
  // an absolute carrier-stack pointer until the continuation is thawed.
  return raw > -max_jint && raw < max_jint ? _sp + raw : (intptr_t*)raw;
}

template <ChunkFrames frame_kind>
inline intptr_t* StackChunkFrameStream<frame_kind>::derelativize(int offset) const {
  intptr_t* fp = this->fp();
  assert(fp != nullptr, "frame pointer must exist");
  return fp + fp[offset];
}

template <ChunkFrames frame_kind>
inline intptr_t* StackChunkFrameStream<frame_kind>::unextended_sp_for_interpreter_frame() const {
  assert_is_interpreted_and_frame_type_mixed();
  intptr_t raw = _sp[stack_chunk_sparc_i5_slot];
  return raw > -max_jint && raw < max_jint ? fp() + raw : (intptr_t*)raw;
}

template <ChunkFrames frame_kind>
intptr_t* StackChunkFrameStream<frame_kind>::next_sp_for_interpreter_frame() const {
  assert_is_interpreted_and_frame_type_mixed();
  intptr_t* next = fp();
  return next >= _end ? _end : next;
}

template <ChunkFrames frame_kind>
inline void StackChunkFrameStream<frame_kind>::next_for_interpreter_frame() {
  assert_is_interpreted_and_frame_type_mixed();
  intptr_t* next = fp();
  if (next >= _end) {
    _unextended_sp = _end;
    _sp = _end;
  } else {
    _sp = next;
    intptr_t raw = _sp[stack_chunk_sparc_i5_slot];
    _unextended_sp = raw > -max_jint && raw < max_jint
        ? fp() + raw : (intptr_t*)raw;
  }
}

template <ChunkFrames frame_kind>
inline int StackChunkFrameStream<frame_kind>::interpreter_frame_size() const {
  assert_is_interpreted_and_frame_type_mixed();
  frame f = to_frame();
  return (int)(ContinuationHelper::InterpretedFrame::frame_bottom(f)
               - unextended_sp());
}

template <ChunkFrames frame_kind>
inline int StackChunkFrameStream<frame_kind>::interpreter_frame_stack_argsize() const {
  assert_is_interpreted_and_frame_type_mixed();
  return to_frame().interpreter_frame_method()->size_of_parameters()
         * Interpreter::stackElementWords;
}

template <ChunkFrames frame_kind>
inline int StackChunkFrameStream<frame_kind>::interpreter_frame_num_oops() const {
  assert_is_interpreted_and_frame_type_mixed();
  ResourceMark rm;
  InterpreterOopMap mask;
  frame f = to_frame();
  f.interpreted_frame_oop_map(&mask);
  return mask.num_oops() + 1
       + ((intptr_t*)f.interpreter_frame_monitor_begin()
          - (intptr_t*)f.interpreter_frame_monitor_end())
           / BasicObjectLock::size();
}

template<>
template<>
inline void StackChunkFrameStream<ChunkFrames::Mixed>::update_reg_map_pd(RegisterMap* map) {
  if (map->update_map()) {
    map->shift_window(sp(), _pd_younger_sp);
  }
}

template<>
template<>
inline void StackChunkFrameStream<ChunkFrames::CompiledOnly>::update_reg_map_pd(RegisterMap* map) {
  if (map->update_map()) {
    map->shift_window(sp(), _pd_younger_sp);
  }
}

template <ChunkFrames frame_kind>
template <typename RegisterMapT>
inline void StackChunkFrameStream<frame_kind>::update_reg_map_pd(RegisterMapT* map) {
  (void)map;
}

template <ChunkFrames frame_kind>
template <typename RegisterMapT>
inline void* StackChunkFrameStream<frame_kind>::reg_to_loc_pd(
    VMReg reg, const RegisterMapT* map) const {
  return (void*)map->location(reg, sp());
}

template <ChunkFrames frame_kind>
inline void* StackChunkFrameStream<frame_kind>::reg_to_loc_pd(
    VMReg reg, const SmallRegisterMap* map) const {
  (void)map;
  return (void*)SmallRegisterMap::location(reg, sp(), _pd_younger_sp);
}

#endif // CPU_SPARC_STACKCHUNKFRAMESTREAM_SPARC_INLINE_HPP
