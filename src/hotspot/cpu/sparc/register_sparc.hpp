/*
 * Copyright (c) 2000, 2022, Oracle and/or its affiliates. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.
 *
 * This code is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * version 2 for more details.
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

#ifndef CPU_SPARC_REGISTER_SPARC_HPP
#define CPU_SPARC_REGISTER_SPARC_HPP

#include "asm/register.hpp"

// forward declaration
class Address;
class VMRegImpl;
typedef VMRegImpl* VMReg;


// The implementation of integer registers for the SPARC architecture.
//
// JDK 20 note: Register is now a small value class wrapping the register
// encoding (see the other cpu ports), not a pointer to a RegisterImpl
// object.  The RegisterImpl/FloatRegisterImpl names are kept as aliases
// (at the bottom of this file) so that existing SPARC code referring to
// e.g. FloatRegisterImpl::S keeps compiling.
class Register {
  int _encoding;

  constexpr explicit Register(int encoding) : _encoding(encoding) {}

 public:
  enum {
    log_set_size        = 3,                          // the number of bits to encode the set register number
    number_of_sets      = 4,                          // the number of registers sets (in, local, out, global)
    number_of_registers = number_of_sets << log_set_size,

    iset_no = 3,  ibase = iset_no << log_set_size,    // the in     register set
    lset_no = 2,  lbase = lset_no << log_set_size,    // the local  register set
    oset_no = 1,  obase = oset_no << log_set_size,    // the output register set
    gset_no = 0,  gbase = gset_no << log_set_size     // the global register set
  };

  constexpr Register() : _encoding(-1) {} // noreg

  bool operator==(const Register r) const { return _encoding == r._encoding; }
  bool operator!=(const Register r) const { return _encoding != r._encoding; }
  const Register* operator->() const { return this; }

  // general construction
  inline constexpr friend Register as_Register(int encoding);

  // accessors
  int encoding() const                                { assert(is_valid(), "invalid register"); return _encoding; }
  const char* name() const;
  inline VMReg as_VMReg() const;

  // testers
  bool is_valid() const                               { return 0 <= _encoding && _encoding < number_of_registers; }
  bool is_even() const                                { return (encoding() & 1) == 0; }
  bool is_in() const                                  { return (encoding() >> log_set_size) == iset_no; }
  bool is_local() const                               { return (encoding() >> log_set_size) == lset_no; }
  bool is_out() const                                 { return (encoding() >> log_set_size) == oset_no; }
  bool is_global() const                              { return (encoding() >> log_set_size) == gset_no; }

  // derived registers, offsets, and addresses
  Register successor() const                          { return Register(encoding() + 1); }

  int input_number() const {
    assert(is_in(), "must be input register");
    return encoding() - ibase;
  }

  Register after_save() const {
    assert(is_out() || is_global(), "register not visible after save");
    return is_out() ? Register(encoding() + (ibase - obase)) : *this;
  }

  Register after_restore() const {
    assert(is_in() || is_global(), "register not visible after restore");
    return is_in() ? Register(encoding() + (obase - ibase)) : *this;
  }

  int sp_offset_in_saved_window() const {
    assert(is_in() || is_local(), "only i and l registers are saved in frame");
    return encoding() - lbase;
  }

  inline Address address_in_saved_window() const;     // implemented in macroAssembler_sparc.hpp
};

inline constexpr Register as_Register(int encoding) {
  if (0 <= encoding && encoding < Register::number_of_registers) {
    return Register(encoding);
  }
  return Register(); // noreg
}

// set specific construction
inline constexpr Register as_iRegister(int number)  { return as_Register(Register::ibase + number); }
inline constexpr Register as_lRegister(int number)  { return as_Register(Register::lbase + number); }
inline constexpr Register as_oRegister(int number)  { return as_Register(Register::obase + number); }
inline constexpr Register as_gRegister(int number)  { return as_Register(Register::gbase + number); }

// The integer registers of the SPARC architecture

constexpr Register noreg = Register();

constexpr Register G0 = as_gRegister(0);
constexpr Register G1 = as_gRegister(1);
constexpr Register G2 = as_gRegister(2);
constexpr Register G3 = as_gRegister(3);
constexpr Register G4 = as_gRegister(4);
constexpr Register G5 = as_gRegister(5);
constexpr Register G6 = as_gRegister(6);
constexpr Register G7 = as_gRegister(7);

constexpr Register O0 = as_oRegister(0);
constexpr Register O1 = as_oRegister(1);
constexpr Register O2 = as_oRegister(2);
constexpr Register O3 = as_oRegister(3);
constexpr Register O4 = as_oRegister(4);
constexpr Register O5 = as_oRegister(5);
constexpr Register O6 = as_oRegister(6);
constexpr Register O7 = as_oRegister(7);

constexpr Register L0 = as_lRegister(0);
constexpr Register L1 = as_lRegister(1);
constexpr Register L2 = as_lRegister(2);
constexpr Register L3 = as_lRegister(3);
constexpr Register L4 = as_lRegister(4);
constexpr Register L5 = as_lRegister(5);
constexpr Register L6 = as_lRegister(6);
constexpr Register L7 = as_lRegister(7);

constexpr Register I0 = as_iRegister(0);
constexpr Register I1 = as_iRegister(1);
constexpr Register I2 = as_iRegister(2);
constexpr Register I3 = as_iRegister(3);
constexpr Register I4 = as_iRegister(4);
constexpr Register I5 = as_iRegister(5);
constexpr Register I6 = as_iRegister(6);
constexpr Register I7 = as_iRegister(7);

constexpr Register FP = I6;
constexpr Register SP = O6;


// The implementation of float registers for the SPARC architecture
class FloatRegister {
  int _encoding;

  constexpr explicit FloatRegister(int encoding) : _encoding(encoding) {}

 public:
  enum {
    number_of_registers = 64
  };

  enum Width {
    S = 1,  D = 2,  Q = 3
  };

  constexpr FloatRegister() : _encoding(-1) {} // fnoreg

  bool operator==(const FloatRegister r) const { return _encoding == r._encoding; }
  bool operator!=(const FloatRegister r) const { return _encoding != r._encoding; }
  const FloatRegister* operator->() const { return this; }

  // general construction
  inline constexpr friend FloatRegister as_FloatRegister(int encoding);

  // accessors
  int encoding() const { assert(is_valid(), "invalid register"); return _encoding; }

  int encoding(Width w) const {
    const int c = encoding();
    switch (w) {
      case S:
        assert(c < 32, "bad single float register");
        return c;

      case D:
        assert(c < 64  &&  (c & 1) == 0, "bad double float register");
        return (c & 0x1e) | ((c & 0x20) >> 5);

      case Q:
        assert(c < 64  &&  (c & 3) == 0, "bad quad float register");
        return (c & 0x1c) | ((c & 0x20) >> 5);
    }
    ShouldNotReachHere();
    return -1;
  }

  bool is_valid() const { return 0 <= _encoding && _encoding < number_of_registers; }
  bool is_even()  const { return (encoding() & 1) == 0; }

  const char* name() const;
  inline VMReg as_VMReg() const;

  FloatRegister successor() const { return FloatRegister(encoding() + 1); }
};

inline constexpr FloatRegister as_FloatRegister(int encoding) {
  if (0 <= encoding && encoding < FloatRegister::number_of_registers) {
    return FloatRegister(encoding);
  }
  return FloatRegister(); // fnoreg
}

// The float registers of the SPARC architecture

constexpr FloatRegister fnoreg = FloatRegister();

constexpr FloatRegister F0  = as_FloatRegister( 0);
constexpr FloatRegister F1  = as_FloatRegister( 1);
constexpr FloatRegister F2  = as_FloatRegister( 2);
constexpr FloatRegister F3  = as_FloatRegister( 3);
constexpr FloatRegister F4  = as_FloatRegister( 4);
constexpr FloatRegister F5  = as_FloatRegister( 5);
constexpr FloatRegister F6  = as_FloatRegister( 6);
constexpr FloatRegister F7  = as_FloatRegister( 7);
constexpr FloatRegister F8  = as_FloatRegister( 8);
constexpr FloatRegister F9  = as_FloatRegister( 9);
constexpr FloatRegister F10 = as_FloatRegister(10);
constexpr FloatRegister F11 = as_FloatRegister(11);
constexpr FloatRegister F12 = as_FloatRegister(12);
constexpr FloatRegister F13 = as_FloatRegister(13);
constexpr FloatRegister F14 = as_FloatRegister(14);
constexpr FloatRegister F15 = as_FloatRegister(15);
constexpr FloatRegister F16 = as_FloatRegister(16);
constexpr FloatRegister F17 = as_FloatRegister(17);
constexpr FloatRegister F18 = as_FloatRegister(18);
constexpr FloatRegister F19 = as_FloatRegister(19);
constexpr FloatRegister F20 = as_FloatRegister(20);
constexpr FloatRegister F21 = as_FloatRegister(21);
constexpr FloatRegister F22 = as_FloatRegister(22);
constexpr FloatRegister F23 = as_FloatRegister(23);
constexpr FloatRegister F24 = as_FloatRegister(24);
constexpr FloatRegister F25 = as_FloatRegister(25);
constexpr FloatRegister F26 = as_FloatRegister(26);
constexpr FloatRegister F27 = as_FloatRegister(27);
constexpr FloatRegister F28 = as_FloatRegister(28);
constexpr FloatRegister F29 = as_FloatRegister(29);
constexpr FloatRegister F30 = as_FloatRegister(30);
constexpr FloatRegister F31 = as_FloatRegister(31);

constexpr FloatRegister F32 = as_FloatRegister(32);
constexpr FloatRegister F34 = as_FloatRegister(34);
constexpr FloatRegister F36 = as_FloatRegister(36);
constexpr FloatRegister F38 = as_FloatRegister(38);
constexpr FloatRegister F40 = as_FloatRegister(40);
constexpr FloatRegister F42 = as_FloatRegister(42);
constexpr FloatRegister F44 = as_FloatRegister(44);
constexpr FloatRegister F46 = as_FloatRegister(46);
constexpr FloatRegister F48 = as_FloatRegister(48);
constexpr FloatRegister F50 = as_FloatRegister(50);
constexpr FloatRegister F52 = as_FloatRegister(52);
constexpr FloatRegister F54 = as_FloatRegister(54);
constexpr FloatRegister F56 = as_FloatRegister(56);
constexpr FloatRegister F58 = as_FloatRegister(58);
constexpr FloatRegister F60 = as_FloatRegister(60);
constexpr FloatRegister F62 = as_FloatRegister(62);

// JDK 20 compatibility aliases: pre-JDK20 SPARC code refers to the register
// classes through the old RegisterImpl/FloatRegisterImpl names.
typedef Register      RegisterImpl;
typedef FloatRegister FloatRegisterImpl;

// Maximum number of incoming arguments that can be passed in i registers.
const int SPARC_ARGS_IN_REGS_NUM = 6;

class ConcreteRegisterImpl : public AbstractRegisterImpl {
 public:
  enum {
    // This number must be large enough to cover REG_COUNT (defined by c2) registers.
    // There is no requirement that any ordering here matches any ordering c2 gives
    // it's optoregs.
    number_of_registers = 2*Register::number_of_registers +
                            FloatRegister::number_of_registers +
                            1 + // ccr
                            4  //  fcc
  };
  static const int max_gpr;
  static const int max_fpr;

};

// Single, Double and Quad fp reg classes.  These exist to map the ADLC
// encoding for a floating point register, to the FloatRegister number
// desired by the macroassembler.  A FloatRegister is a number between
// 0 and 63.  For ADLC, an fp register encoding is the actual bit encoding
// used by the sparc hardware.  When ADLC uses the macroassembler to generate
// an instruction that references, e.g., a double fp reg, it passes the bit
// encoding to the macroassembler via as_FloatRegister, which, for double
// regs > 30, returns an illegal register number.
//
// Therefore we provide the following classes for use by ADLC.  Their
// sole purpose is to convert from sparc register encodings to FloatRegisters.
// At some future time, we might replace FloatRegister with these classes,
// hence the definitions of as_xxxFloatRegister as class methods rather
// than as external inline routines.

class SingleFloatRegisterImpl;
typedef SingleFloatRegisterImpl *SingleFloatRegister;

inline FloatRegister as_SingleFloatRegister(int encoding);
class SingleFloatRegisterImpl {
 public:
  friend inline FloatRegister as_SingleFloatRegister(int encoding) {
    assert(encoding < 32, "bad single float register encoding");
    return as_FloatRegister(encoding);
  }
};


class DoubleFloatRegisterImpl;
typedef DoubleFloatRegisterImpl *DoubleFloatRegister;

inline FloatRegister as_DoubleFloatRegister(int encoding);
class DoubleFloatRegisterImpl {
 public:
  friend inline FloatRegister as_DoubleFloatRegister(int encoding) {
    assert(encoding < 32, "bad double float register encoding");
    return as_FloatRegister( ((encoding & 1) << 5) | (encoding & 0x1e) );
  }
};


class QuadFloatRegisterImpl;
typedef QuadFloatRegisterImpl *QuadFloatRegister;

class QuadFloatRegisterImpl {
 public:
  friend FloatRegister as_QuadFloatRegister(int encoding) {
    assert(encoding < 32 && ((encoding & 2) == 0), "bad quad float register encoding");
    return as_FloatRegister( ((encoding & 1) << 5) | (encoding & 0x1c) );
  }
};

#endif // CPU_SPARC_REGISTER_SPARC_HPP
