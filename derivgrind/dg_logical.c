/*--------------------------------------------------------------------*/
/*--- Handling of logical operations.                 dg_logical.c ---*/
/*--------------------------------------------------------------------*/

/*
   This file is part of DerivGrind, a tool performing forward-mode
   algorithmic differentiation of compiled programs implemented
   in the Valgrind framework.

   Copyright (C) 2022 Max Aehle
      max.aehle@scicomp.uni-kl.de

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

#include "pub_tool_basics.h"

#include "dg_logical.h"

extern Bool paragrind;

/*! \file dg_logical.c
 *  Define functions for AD handling of logical operations.
 */

/*! \page ad_handling_logical AD handling of logical operations
 *
 *  Generally, DerivGrind ignores non-copy operations on discrete datatypes,
 *  such as Add64. That's because reinterpreting a floating-point number as
 *  an integer, adding another integer and reinterpreting the result as a
 *  floating-point number is a really strange operation that we cannot imagine
 *  a compiler would use.
 *
 *  Exceptions to this heuristic are given by the logical "and", "or" and "xor"
 *  operations, which can be used to modify the sign bit of a floating-point
 *  number in order to compute its absolute value, negative absolute value or
 *  negative value, respectively. In this case, the other operand is 0b011..1 or
 *  0b100..0. A 64-bit datatype could either contain one binary64 or two binary32
 *  numbers.
 *
 *  Additionally, "and" with 0b11...1 and "or" with 0b00...0 does not change the
 *  other operand, so we should keep its derivative.
 *
 */

/*! Building block to apply 32-bit AD handling to both
 *  halves of a 64-bit number.
 *  \param[in] fun32 - Function to be called for both 32-bit halves.
 */
#define DG_HANDLE_HALVES(fun32) \
  { \
    typedef union {ULong ul; struct {UInt ui1, ui2;} ui; } u; \
    u result, x_u, xd_u, y_u, yd_u; \
    x_u.ul = x; xd_u.ul = xd; \
    y_u.ul = y; yd_u.ul = yd; \
    result.ui.ui1 = fun32(x_u.ui.ui1, xd_u.ui.ui1, y_u.ui.ui1, yd_u.ui.ui1); \
    result.ui.ui2 = fun32(x_u.ui.ui2, xd_u.ui.ui2, y_u.ui.ui2, yd_u.ui.ui2); \
    return result.ul; \
  }

/*--- AND <-> abs ---*/

/*! Building block for the AD handling of logical "and".
 *
 *  If x is equal to 0b0111..., assume that the operation
 *  computes abs(y) and return the derivative accordingly.
 */

#define DG_HANDLE_AND(fptype, inttype, x, y) \
  if( x == (inttype)(((inttype)1)<<(sizeof(inttype)*8-1))-1 ){ /* 0b01..1 */ \
    if(paragrind) return x & y; \
    fptype y_f = *(fptype*)&y, yd_f = *(fptype*)&y##d; \
    if(y_f<0) yd_f = -yd_f; \
    return *(inttype*)&yd_f; \
  } \
  else if( x == (inttype)(-1) ){ /* 0b1..1 */ \
    return y##d; \
  }

/*! AD handling of logical "and" for F32 type.
 *
 *  The logical "and" can be used to set the sign bit of a F32 number
 *  to zero, in order to take the absolute value.
 *  \param[in] x - Argument to logical "and", reinterpreted as I32.
 *  \param[in] xd - Derivative of x.
 *  \param[in] y - Argument to logical "and", reinterpreted as I32.
 *  \param[in] yd - Derivative of y.
 *  \returns Derivative of abs(x) or abs(y), if the other one is 0b0111.., otherwise zero.
 */
VG_REGPARM(0) UInt dg_logical_and32(UInt x, UInt xd, UInt y, UInt yd){
  DG_HANDLE_AND(float,UInt, x, y)
  else DG_HANDLE_AND(float,UInt, y, x)
  else return 0x0;
}

/*! AD handling of logical "and" for F32 and F64 type.
 *
 * A logical "and" with 64-bit 0b0111... could be an absolute
 * value operation on F64, treat it accordingly. Otherwise, it
 * might be a 32-bit absolute value operation on either half.
 */
VG_REGPARM(0) ULong dg_logical_and64(ULong x, ULong xd, ULong y, ULong yd){
  DG_HANDLE_AND(double,ULong, x, y)
  else DG_HANDLE_AND(double,ULong, y, x)
  else DG_HANDLE_HALVES(dg_logical_and32)
}

/*--- OR <-> negative abs ---*/
// compare with 0b100...0 and 0b00...0
#define DG_HANDLE_OR(fptype, inttype, x, y) \
  if( x == (inttype)(((inttype)1)<<(sizeof(inttype)*8-1)) && x##d == 0 ){ /* 0b10..0 */ \
    if(paragrind) return x | y; \
    fptype y_f = *(fptype*)&y, yd_f = *(fptype*)&y##d; \
    if(y_f>0) yd_f = -yd_f; \
    return *(inttype*)&yd_f; \
  } \
  else if( x == 0 && x##d==0 ){ /* 0b0..0 */ \
    return y##d; \
  }

VG_REGPARM(0) UInt dg_logical_or32(UInt x, UInt xd, UInt y, UInt yd){
  DG_HANDLE_OR(float,UInt, x, y)
  else DG_HANDLE_OR(float,UInt, y, x)
  else return 0x0;
}

VG_REGPARM(0) ULong dg_logical_or64(ULong x, ULong xd, ULong y, ULong yd){
  DG_HANDLE_OR(double,ULong, x, y)
  else DG_HANDLE_OR(double,ULong, y, x)
  else DG_HANDLE_HALVES(dg_logical_or32)
}

/*--- XOR <-> negative ---*/
// compare with 0b100...0

#define DG_HANDLE_XOR(fptype, inttype, x, y) \
  if( x == (inttype)(((inttype)1)<<(sizeof(inttype)*8-1)) && x##d == 0 ){ \
    if(paragrind) return x ^ y; \
    fptype yd_f = *(fptype*)&y##d; \
    yd_f = -yd_f; \
    return *(inttype*)&yd_f; \
  }

VG_REGPARM(0) UInt dg_logical_xor32(UInt x, UInt xd, UInt y, UInt yd){
  DG_HANDLE_XOR(float,UInt, x, y)
  else DG_HANDLE_XOR(float,UInt, y, x)
  else return 0x0;
}

VG_REGPARM(0) ULong dg_logical_xor64(ULong x, ULong xd, ULong y, ULong yd){
  DG_HANDLE_XOR(double,ULong, x, y)
  else DG_HANDLE_XOR(double,ULong, y, x)
  else DG_HANDLE_HALVES(dg_logical_xor32)
}
