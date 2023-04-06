/*--------------------------------------------------------------------*/
/*--- Shadow memory stuff.                             dg_shadow.c ---*/
/*--------------------------------------------------------------------*/

/*
   This file is part of Derivgrind, an automatic differentiation
   tool applicable to compiled programs.

   Copyright (C) 2022, Chair for Scientific Computing, TU Kaiserslautern
   Copyright (C) since 2023, Chair for Scientific Computing, University of Kaiserslautern-Landau
   Homepage: https://www.scicomp.uni-kl.de
   Contact: Prof. Nicolas R. Gauger (derivgrind@projects.rptu.de)

   Lead developer: Max Aehle

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, see <http://www.gnu.org/licenses/>.

   The GNU General Public License is contained in the file COPYING.
*/

/*! \file dg_shadow.c
 *  Shadow memory stuff for Derivgrind.
 */
#include "dot/dg_dot_shadow.h"
#include "bar/dg_bar_shadow.h"

/*! \page loading_and_storing Loading and storing tangent values in memory
 *
 *  When translating
 *  - an Ist_Store or Ist_StoreG statement to store data in memory, or
 *  - an Iex_Load expression or Iex_LoadG statements to load data from memory,
 *
 *  Derivgrind has to add instrumentation accessing the shadow memory.
 *  To this end, Derivgrind emits Ist_Dirty statements calling the
 *  functions dg_Store_diffN, dg_Load_diffN.
 *
 *  Dirty calls can only call functions that accept and return 4- and 8-byte
 *  integers arguments. Therefore we have to "encode" all types, including
 *  floating-point and SIMD types, by up to four 8-byte integers (Ity_I64 on
 *  the VEX side, ULong on the C side).
 *
 *  Storing derivative information
 *  ------------------------------
 *  During translation, we call convertToInteger on the IRExpr for the
 *  derivative. This wraps some instructions around the IRExpr, giving
 *  an array of four IRExpr's that evaluate to the type Ity_I64. These
 *  are used to create up to four Ist_Dirty statements for dg_Store_diffN.
 *
 *  When the translated IRSB is executed, the wrapper
 *  VEX instructions encode the value on the simulated CPU and then the
 *  Ist_Dirty mechanism calls dg_Store_diffN with the encoded values of
 *  type ULong. Inside dg_Store_diffN we decode the data and store it in
 *  shadow memory.
 *
 *  Loading derivative information
 *  ------------------------------
 *  Ist_Dirty statements calling dg_Load_diffN are added during translation.
 *  When the translated IRSB is executed, the Ist_Dirty mechanism calls
 *  dg_Load_diffN, which reads the gradient information from shadow memory,
 *  encodes it, and returns a ULong which is stored as Ity_I64 in a temporary.
 *  In the transation phase we also wrap these up to four temporaries by
 *  convertFromInteger, which adds VEX instructions that convert the Ity_I64
 *  data back to the original type.
 *
 *  How the encoding works
 *  ----------------------
 *  For most types, this "encoding" is a binary reinterpretation, and padding
 *  by zeros. On the VEX side, this is accomplished by instructions like
 *  Iop_ReinterpF64asI64, Iop_32HLto64, Iop_128to64. On the C side, no explicit
 *  conversion is required because the gradient information is anyways accessed
 *  per byte.
 *
 *  For the types Ity_F16 and Ity_D32, we miss the corresponding VEX
 *  reinterpretation instructions. One way to circumvent this could be
 *  to encode them in a different way, e.g. first cast Ity_D32 to Ity_D64
 *  and then reinterpret this as Ity_I64. However, in order to revert this
 *  on the C side, Valgrind must be compiled with a compiler that supports
 *  the C type _Decimal32. To avoid this restriction, we just drop any gradient
 *  information associated to such expressions when they are stored in memory.
 *
 *  Issues
 *  ------
 *  We always assume a little-endian storage order, so the code might be incorrect
 *  for architectures other than x86/amd64.
 *
 */

#include "dg_shadow.h"


/*! Print a double value during execution of the generated code, for debugging purposes.
 *  The Ist_Dirty calling this function is produced by dg_add_print_stmt.
 *  \param[in] tag - Tag that is also printed.
 *  \param[in] value - Printed value.
 */
static VG_REGPARM(0) void dg_Print_double(ULong tag, ULong value){ VG_(printf)("Value for %d : ", tag); VG_(printf)("%lf\n", *(double*)&value); }
static VG_REGPARM(0) void dg_Print_unsignedlong(ULong tag, ULong value){ VG_(printf)("Value for %d : ", tag); VG_(printf)("%p\n", (void*)value); }
static VG_REGPARM(0) void dg_Print_unsignedint(ULong tag, Int value){ VG_(printf)("Value for %d : ", tag); VG_(printf)("%p\n", (void*)value); }

/*! Debugging help. Add a dirty statement to IRSB that prints the value of expr whenever it is run.
 *  \param[in] tag - Tag of your choice, will be printed alongside.
 *  \param[in] sb_out - IRSB to which the dirty statement is added.
 *  \param[in] expr - Expression.
 */
void dg_add_print_stmt(ULong tag, IRSB* sb_out, IRExpr* expr){
  IRType type = typeOfIRExpr(sb_out->tyenv, expr);
  char* fname;
  void* fptr;
  IRExpr* expr_to_print;
  switch(type){
    case Ity_F64:
      fname = "dg_Print_double";
      fptr = dg_Print_double;
      expr_to_print = IRExpr_Unop(Iop_ReinterpF64asI64,expr);
      break;
    case Ity_F32:
      fname = "dg_Print_double";
      fptr = dg_Print_double;
      expr_to_print = IRExpr_Unop(Iop_ReinterpF64asI64,IRExpr_Unop(Iop_F32toF64,expr));
      break;
    case Ity_I64:
      fname = "dg_Print_unsignedlong";
      fptr = dg_Print_unsignedlong;
      expr_to_print = expr;
      break;
    case Ity_I32:
      fname = "dg_Print_unsignedint";
      fptr = dg_Print_unsignedint;
      expr_to_print = expr;
      break;
    default:
      VG_(printf)("Bad type in dg_add_print_stmt.\n");
      return;
  }
  IRDirty* di = unsafeIRDirty_0_N(
        0,
        fname, VG_(fnptr_to_fnentry)(fptr),
        mkIRExprVec_2(IRExpr_Const(IRConst_U64(tag)), expr_to_print));
  addStmtToIRSB(sb_out, IRStmt_Dirty(di));
}

#include "pub_tool_gdbserver.h"
#include "pub_tool_threadstate.h"
#include "pub_tool_libcfile.h"
#include "pub_tool_vki.h"
static unsigned long outcount = 0;
extern Bool diffquotdebug;
extern Long dg_disable;
ULong buffer[2000000];
static Int fd=-1;
static VG_REGPARM(0) void dg_add_diffquotdebug_helper(ULong value, ULong dotvalue){
  if(fd==-1){
    fd=VG_(fd_open)("~/dump",VKI_O_WRONLY,VKI_O_CREAT);
  }
  if(fd==-1){
    VG_(printf)("Cannot get file descriptor.");
  }
  if(diffquotdebug && dg_disable==0){
    buffer[2*(outcount%1000000)] = value;
    buffer[2*(outcount%1000000)+1] = dotvalue;
    if(outcount%1000000==999999){
      VG_(write)(fd,buffer,2000000*sizeof(ULong));
    }
    outcount++;
  }
}
void dg_add_diffquotdebug(IRSB* sb_out, IRExpr* value, IRExpr* dotvalue){
  IRType type = typeOfIRExpr(sb_out->tyenv, value);
  tl_assert(type == typeOfIRExpr(sb_out->tyenv, dotvalue));
  IRExpr *value_to_print, *dotvalue_to_print;
  switch(type){
    case Ity_F64:
      value_to_print = IRExpr_Unop(Iop_ReinterpF64asI64,value);
      dotvalue_to_print = IRExpr_Unop(Iop_ReinterpF64asI64,dotvalue);
      break;
    case Ity_F32:
      value_to_print = IRExpr_Unop(Iop_ReinterpF64asI64,IRExpr_Unop(Iop_F32toF64,value));
      dotvalue_to_print = IRExpr_Unop(Iop_ReinterpF64asI64,IRExpr_Unop(Iop_F32toF64,dotvalue));
      break;
    default:
      VG_(printf)("Bad type in dg_add_diffquotdebug.\n");
      return;
  }
  IRDirty* di = unsafeIRDirty_0_N(
        0,
        "dg_add_diffquotdebug_helper", VG_(fnptr_to_fnentry)(&dg_add_diffquotdebug_helper),
        mkIRExprVec_2(value_to_print, dotvalue_to_print));
  addStmtToIRSB(sb_out, IRStmt_Dirty(di));
}

