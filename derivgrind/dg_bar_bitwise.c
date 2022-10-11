/*--------------------------------------------------------------------*/
/*--- Handling of logical operations.             dg_bar_bitwise.c ---*/
/*--------------------------------------------------------------------*/

/*
   This file is part of DerivGrind, a tool performing forward-mode
   algorithmic differentiation of compiled programs implemented
   in the Valgrind framework.

   Copyright (C) 2022 Chair for Scientific Computing (SciComp), TU Kaiserslautern
   Homepage: https://www.scicomp.uni-kl.de
   Contact: Prof. Nicolas R. Gauger (derivgrind@scicomp.uni-kl.de)

   Lead developer: Max Aehle (SciComp, TU Kaiserslautern)

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

#include "dg_bar_bitwise.h"
#include "dg_bar_tape.h"

/*! \file dg_bar_bitwise.c
 *  Define functions for recording-mode AD handling of logical operations.
 *
 *  See \ref ad_handling_bitwise for details.
 */

/*! Assemble lower 4 bytes of both arguments into 8-byte index.
 * \param[in] iLo - Lower four bytes determine lower four bytes of result.
 * \param[in] iHi - Lower four bytes determine higher four bytes of result.
 * \returns Assembled index.
 */
static ULong assemble64x2to64(ULong iLo, ULong iHi){
  ULong ret;
  *(UInt*)&ret = *(UInt*)&iLo;
  *((UInt*)&ret+1) = *(UInt*)&iHi;
  return ret;
}
/*! Building block to apply 32-bit recording-mode AD handling to both
 *  halves of a 64-bit number.
 *  \param[in] fun32 - Function to be called for both 32-bit halves.
 */
#define DG_HANDLE_HALVES(fun32) \
  { \
    typedef union {ULong u64; struct {UInt u32_1, u32_2;} u32; } u; \
    u result, x_u, xiLo_u, xiHi_u, y_u, yiLo_u, yiHi_u; \
    x_u.u64 = x; xiLo_u.u64 = xiLo; xiHi_u.u64 = xiHi; \
    y_u.u64 = y; yiLo_u.u64 = yiLo; yiHi_u.u64 = yiHi; \
    result.u32.u32_1 = fun32(x_u.u32.u32_1, xiLo_u.u32.u32_1, xiHi_u.u32.u32_1, y_u.u32.u32_1, yiLo_u.u32.u32_1, yiHi_u.u32.u32_1); \
    result.u32.u32_2 = fun32(x_u.u32.u32_2, xiLo_u.u32.u32_2, xiHi_u.u32.u32_2, y_u.u32.u32_2, yiLo_u.u32.u32_2, yiHi_u.u32.u32_2); \
    return result.u64; \
  }

/*--- AND <-> abs ---*/

/*! Building block for the AD handling of logical "and".
 *
 *  If x is equal to 0b0111..., assume that the operation
 *  computes abs(y) and return the index accordingly.
 */

#define DG_HANDLE_AND(fptype, inttype, x, y) \
  if( x == (inttype)(((inttype)1)<<(sizeof(inttype)*8-1))-1 ){ /* 0b01..1 */ \
    fptype y_f = *(fptype*)&y; \
    ULong yi = assemble64x2to64(y##iLo,y##iHi); \
    if(y_f>=0) return yi; \
    else return tapeAddStatement(yi,0,-1.,0.); \
  } \
  else if( x == (inttype)(-1) ){ /* 0b1..1 */ \
    return assemble64x2to64(y##iLo,y##iHi); \
  }

/*! AD handling of logical "and" for F32 type.
 *
 *  The logical "and" can be used to set the sign bit of a F32 number
 *  to zero, in order to take the absolute value.
 *  \param[in] x - Argument to logical "and", reinterpreted as I32.
 *  \param[in] xiLo - Lower 4 bytes of index of x.
 *  \param[in] xiHi - Higher 4 bytes of index of x.
 *  \param[in] y - Argument to logical "and", reinterpreted as I32.
 *  \param[in] yiLo - Lower 4 bytes of index of y.
 *  \param[in] yiHi - Higher 4 bytes of index of y.
 *  \returns Index of abs(x) or abs(y), if the other one is 0b0111.., otherwise zero.
 */
VG_REGPARM(0) ULong dg_bar_bitwise_and32(UInt x, UInt xiLo, UInt xiHi, UInt y, UInt yiLo, UInt yiHi){
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
VG_REGPARM(0) ULong dg_bar_bitwise_and64(ULong x, ULong xiLo, ULong xiHi, ULong y, ULong yiLo, ULong yiHi){
  DG_HANDLE_AND(double,ULong, x, y)
  else DG_HANDLE_AND(double,ULong, y, x)
  else DG_HANDLE_HALVES(dg_bar_bitwise_and32)
}

/*--- OR <-> negative abs ---*/
// compare with 0b100...0 and 0b00...0
#define DG_HANDLE_OR(fptype, inttype, x, y) \
  if( x == (inttype)(((inttype)1)<<(sizeof(inttype)*8-1)) && *(UInt*)&x##iLo == 0 && *(UInt*)&x##iHi == 0 ){ /* 0b10..0 */ \
    fptype y_f = *(fptype*)&y; \
    ULong yi = assemble64x2to64(y##iLo,y##iHi); \
    if(y_f<=0) return yi; \
    else return tapeAddStatement(yi,0,-1.,0); \
  } \
  else if( x == 0 && *(UInt*)&x##iLo==0 && *(UInt*)&x##iHi==0 ){ /* 0b0..0 */ \
    return assemble64x2to64(y##iLo,y##iHi); \
  }

VG_REGPARM(0) ULong dg_bar_bitwise_or32(UInt x, UInt xiLo, UInt xiHi, UInt y, UInt yiLo, UInt yiHi){
  DG_HANDLE_OR(float,UInt, x, y)
  else DG_HANDLE_OR(float,UInt, y, x)
  else return 0x0;
}

VG_REGPARM(0) ULong dg_bar_bitwise_or64(ULong x, ULong xiLo, ULong xiHi, ULong y, ULong yiLo, ULong yiHi){
  DG_HANDLE_OR(double,ULong, x, y)
  else DG_HANDLE_OR(double,ULong, y, x)
  else DG_HANDLE_HALVES(dg_bar_bitwise_or32)
}

/*--- XOR <-> negative ---*/
// compare with 0b100...0

#define DG_HANDLE_XOR(fptype, inttype, x, y) \
  if( x == (inttype)(((inttype)1)<<(sizeof(inttype)*8-1)) && *(UInt*)&x##iLo == 0 && *(UInt*)&x##iHi == 0 ){ \
    ULong yi = assemble64x2to64(y##iLo,y##iHi); \
    return tapeAddStatement(yi,0,-1.,0); \
  }

VG_REGPARM(0) ULong dg_bar_bitwise_xor32(UInt x, UInt xiLo, UInt xiHi, UInt y, UInt yiLo, UInt yiHi){
  DG_HANDLE_XOR(float,UInt, x, y)
  else DG_HANDLE_XOR(float,UInt, y, x)
  else return 0x0;
}

VG_REGPARM(0) ULong dg_bar_bitwise_xor64(ULong x, ULong xiLo, ULong xiHi, ULong y, ULong yiLo, ULong yiHi){
  DG_HANDLE_XOR(double,ULong, x, y)
  else DG_HANDLE_XOR(double,ULong, y, x)
  else DG_HANDLE_HALVES(dg_bar_bitwise_xor32)
}

