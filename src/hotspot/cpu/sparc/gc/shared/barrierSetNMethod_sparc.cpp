/*
 * Copyright (c) 2018, Oracle and/or its affiliates. All rights reserved.
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

#include "precompiled.hpp"
#include "gc/shared/barrierSetNMethod.hpp"
#include "utilities/debug.hpp"

void BarrierSetNMethod::deoptimize(nmethod* nm, address* return_address_ptr) {
  // No SPARC nmethod entry barrier is emitted, so the slow path that would
  // request deoptimization must never be reached.
  ShouldNotReachHere();
}

void BarrierSetNMethod::disarm(nmethod* nm) {
  // SPARC has no generated guard to patch. supports_entry_barrier() returns
  // false for this port, making the code-cache unloading paths conservative.
  assert(!supports_entry_barrier(nm), "SPARC does not support nmethod entry barriers");
}

void BarrierSetNMethod::arm(nmethod* nm, int arm_value) {
  // No generated guard exists to arm on SPARC.
  assert(!supports_entry_barrier(nm), "SPARC does not support nmethod entry barriers");
}

bool BarrierSetNMethod::is_armed(nmethod* nm) {
  // With no generated guard, an nmethod can never be armed on SPARC.
  assert(!supports_entry_barrier(nm), "SPARC does not support nmethod entry barriers");
  return false;
}
