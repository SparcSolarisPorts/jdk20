/*
 * Diagnostic tracing helpers for the SPARC C1 port.
 *
 * Enable with:
 *   SPARC_C1_TRACE=1   method/block/LIR emission trace
 *   SPARC_C1_TRACE=2   + address/arithmetic/compare/branch detail
 *   SPARC_C1_TRACE=3   + loads/stores/moves/memory detail
 * Optional:
 *   SPARC_C1_TRACE_FILTER=substring
 *
 * This is intentionally environment-variable controlled so it also works in
 * release/product builds without adding a permanent JVM flag.
 */
#ifndef CPU_SPARC_C1_SPARCTRACE_HPP
#define CPU_SPARC_C1_SPARCTRACE_HPP

#include "ci/ciMethod.hpp"
#include "c1/c1_LIR.hpp"
#include "utilities/ostream.hpp"
#include <stdlib.h>
#include <string.h>

namespace SparcC1Trace {

inline int init_level() {
  const char* s = ::getenv("SPARC_C1_TRACE");
  if (s == NULL || s[0] == '\0' || (s[0] == '0' && s[1] == '\0')) {
    return 0;
  }
  int v = ::atoi(s);
  return v <= 0 ? 1 : v;
}

inline int level() {
  static const int value = init_level();
  return value;
}

inline const char* filter() {
  static const char* value = ::getenv("SPARC_C1_TRACE_FILTER");
  return value;
}

inline bool matches(ciMethod* method) {
  if (method == NULL || level() == 0) {
    return false;
  }
  const char* f = filter();
  if (f == NULL || f[0] == '\0') {
    return true;
  }
  const char* holder = method->holder()->name()->as_utf8();
  const char* name = method->name()->as_utf8();
  return ::strstr(holder, f) != NULL || ::strstr(name, f) != NULL;
}

inline bool active(ciMethod* method, int min_level = 1) {
  return level() >= min_level && matches(method);
}

inline bool begin(ciMethod* method, const char* tag, int min_level = 1) {
  if (!active(method, min_level)) {
    return false;
  }
  tty->print("[SPARC-C1][%s] ", tag);
  method->holder()->name()->print_symbol_on(tty);
  tty->print("::");
  method->name()->print_symbol_on(tty);
  tty->print(" ");
  return true;
}


inline void print_opr(LIR_Opr opr) {
  if (!opr || opr->is_illegal()) {
    tty->print("<illegal>");
    return;
  }

  if (opr->is_constant()) {
    LIR_Const* c = opr->as_constant_ptr();
    tty->print("const<%s>(", type2name(c->type()));
    switch (c->type()) {
      case T_INT:
      case T_ADDRESS:
        tty->print("%d", c->as_jint());
        break;
      case T_LONG:
        tty->print(JLONG_FORMAT, c->as_jlong());
        break;
      case T_FLOAT:
        tty->print("%g", (double)c->as_jfloat());
        break;
      case T_DOUBLE:
        tty->print("%g", c->as_jdouble());
        break;
      case T_OBJECT:
        tty->print("oop");
        break;
      case T_METADATA:
        tty->print("metadata");
        break;
      default:
        tty->print("?");
        break;
    }
    tty->print(")");
    return;
  }

  if (opr->is_address()) {
    LIR_Address* a = opr->as_address_ptr();
    tty->print("addr<%s>(base=", type2name(a->type()));
    print_opr(a->base());
    tty->print(",index=");
    print_opr(a->index());
    tty->print(",scale=%d,disp=" INTX_FORMAT ")", (int)a->scale(), a->disp());
    return;
  }

  if (opr->is_stack()) {
    if (opr->is_single_stack()) {
      tty->print("stack<%s>(%d)", type2name(opr->type()), opr->single_stack_ix());
    } else {
      tty->print("stack2<%s>(%d)", type2name(opr->type()), opr->double_stack_ix());
    }
    return;
  }

  if (opr->is_cpu_register()) {
    if (opr->is_virtual()) {
      tty->print("vreg<%s>(%d)", type2name(opr->type()), opr->vreg_number());
    } else if (opr->is_single_cpu()) {
      tty->print("reg<%s>(%s)", type2name(opr->type()), opr->as_register()->name());
    } else {
      tty->print("reg2<%s>(%s)", type2name(opr->type()), opr->as_register_lo()->name());
    }
    return;
  }

  if (opr->is_fpu_register()) {
    if (opr->is_virtual()) {
      tty->print("vfreg<%s>(%d)", type2name(opr->type()), opr->vreg_number());
    } else if (opr->is_single_fpu()) {
      tty->print("freg<%s>(%s)", type2name(opr->type()), opr->as_float_reg()->name());
    } else {
      tty->print("freg2<%s>(%s)", type2name(opr->type()), opr->as_double_reg()->name());
    }
    return;
  }

  tty->print("opr<%s>", type2name(opr->type()));
}

inline void finish_line() {
  tty->cr();
  tty->flush();
}

} // namespace SparcC1Trace

#endif // CPU_SPARC_C1_SPARCTRACE_HPP
