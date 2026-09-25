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

#ifndef CPU_SPARC_STACKCHUNKOOP_SPARC_INLINE_HPP
#define CPU_SPARC_STACKCHUNKOOP_SPARC_INLINE_HPP

#include "runtime/frame.inline.hpp"

inline void stackChunkOopDesc::relativize_frame_pd(frame& fr) const {
  // The SPARC frame wrapper caches the saved %i6 link.  Keep an in-chunk
  // link relative so a moved chunk remains walkable; the bottom frame can
  // temporarily point into the carrier stack and is patched when thawed.
  intptr_t* fp = fr.fp();
  fr.set_offset_fp(fp >= start_address() && fp <= end_address()
                     ? relativize_address(fp) : -1);

  intptr_t* younger = fr.younger_sp_or_null();
  fr.set_offset_younger_sp(younger != nullptr &&
                           younger >= start_address() && younger <= end_address()
                             ? relativize_address(younger) : -1);
}

inline void stackChunkOopDesc::derelativize_frame_pd(frame& fr) const {
  const int fp_offset = fr.offset_fp();
  fr.set_fp(fp_offset >= 0 ? derelativize_address(fp_offset) : nullptr);

  const int younger_offset = fr.offset_younger_sp();
  fr.set_younger_sp(younger_offset >= 0
                      ? derelativize_address(younger_offset) : nullptr);
}

#endif // CPU_SPARC_STACKCHUNKOOP_SPARC_INLINE_HPP
