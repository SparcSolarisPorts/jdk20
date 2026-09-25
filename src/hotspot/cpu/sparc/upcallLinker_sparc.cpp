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
#include "prims/upcallLinker.hpp"

// Solaris/SPARC FFM upcalls are intentionally unavailable.  A valid V9
// trampoline must preserve native ABI state, attach/detach correctly, marshal
// arguments and results, and forward exceptions; do not expose a partial stub.
address UpcallLinker::make_upcall_stub(jobject receiver, Method* entry,
                                       BasicType* in_sig_bt, int total_in_args,
                                       BasicType* out_sig_bt, int total_out_args,
                                       BasicType ret_type,
                                       jobject jabi, jobject jconv,
                                       bool needs_return_buffer, int ret_buf_size) {
  ShouldNotCallThis();
  return nullptr;
}
