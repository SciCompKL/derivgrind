/*--------------------------------------------------------------------*/
/*--- Recording-mode handling of                  dg_bar_bitwise.c ---*/
/*--- bitwise logical operations.                                  ---*/
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

#include "pub_tool_basics.h"

#include "dg_bar_bitwise.h"
#include "dg_bar_tape.h"
#include "dg_bar.h"

extern HChar mode;

/*! \file dg_bar_bitwise.c
 *  Define functions for recording-mode AD handling of bitwise logical operations.
 *
 *  These functions are used to define dirty calls for the handling of operations
 *  in the file dg_bar_operations.c generated by gen_operationhandling_code.py. 
 *  See \ref ad_handling_bitwise for details.
 */

// The normal way to record bitwise logical operations with dirty calls
// would be to pass the value and both layers of the shadow value to
// the handler, and return a V128 (via another Iex_VECRET parameter)
// that contains both layers of the shadow of the result.
// This does not work because in total that makes 7 arguments, and on
// amd64 only 6 arguments are currently supported.
// We solve this by letting the handlers store their return value
// in the following shared state, and providing two handlers to read
// the shared state.
static V128 dg_bar_bitwise_out;
VG_REGPARM(0) ULong dg_bar_bitwise_get_lower(void){
  return *(ULong*)&dg_bar_bitwise_out;
}
VG_REGPARM(0) ULong dg_bar_bitwise_get_higher(void){
  return *((ULong*)&dg_bar_bitwise_out+1);
}

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
    u x_u, xiLo_u, xiHi_u, y_u, yiLo_u, yiHi_u; \
    x_u.u64 = x; xiLo_u.u64 = xiLo; xiHi_u.u64 = xiHi; \
    y_u.u64 = y; yiLo_u.u64 = yiLo; yiHi_u.u64 = yiHi; \
    fun32(x_u.u32.u32_2, xiLo_u.u32.u32_2, xiHi_u.u32.u32_2, y_u.u32.u32_2, yiLo_u.u32.u32_2, yiHi_u.u32.u32_2); \
    V128 out_tmp = dg_bar_bitwise_out; \
    fun32(x_u.u32.u32_1, xiLo_u.u32.u32_1, xiHi_u.u32.u32_1, y_u.u32.u32_1, yiLo_u.u32.u32_1, yiHi_u.u32.u32_1); \
    dg_bar_bitwise_out.w32[1] = out_tmp.w32[0]; \
    dg_bar_bitwise_out.w32[3] = out_tmp.w32[2]; \
  }

/*--- AND <-> abs ---*/

/*! Building block for the AD handling of logical "and".
 *
 *  If x is equal to 0b0111..., assume that the operation
 *  computes abs(y). If y>0, just return the index. If y<0,
 *  create a new index on the tape and return it. Because of
 *  this side effect, the function must be invoked through
 *  a dirty call, not a CCall.
 */

#define DG_HANDLE_AND(out, fptype, inttype, x, y) \
  if( x == (inttype)(((inttype)1)<<(sizeof(inttype)*8-1))-1 ){ /* 0b01..1 */ \
    fptype y_f = *(fptype*)&y; \
    if(y_f>=0) { out->w32[0] = *(UInt*)&y##iLo; out->w32[2] = *(UInt*)&y##iHi; } \
    else { \
      ULong yi = assemble64x2to64(y##iLo, y##iHi); \
      ULong minus_yi = tapeAddStatement(yi,0,-1.,0.); \
      if(bar_record_values && minus_yi!=0) valuesAddStatement(-y_f); \
      out->w32[0] = *(UInt*)&minus_yi; \
      out->w32[2] = *((UInt*)&minus_yi+1); \
    } \
  } \
  else if( x == (inttype)(-1) ){ /* 0b1..1 */ \
    out->w32[0] = *(UInt*)&y##iLo; out->w32[2] = *(UInt*)&y##iHi; \
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
VG_REGPARM(0) void dg_bar_bitwise_and32(UInt x, UInt xiLo, UInt xiHi, UInt y, UInt yiLo, UInt yiHi){
  DG_HANDLE_AND((&dg_bar_bitwise_out), float,UInt, x, y)
  else DG_HANDLE_AND((&dg_bar_bitwise_out), float,UInt, y, x)
  else if (mode=='t'){
    dg_bar_bitwise_out.w64[0] = (xiLo | yiLo);
    dg_bar_bitwise_out.w64[1] = 0xfffffffffffffffful;
  } else {
    dg_bar_bitwise_out.w64[0] = dg_bar_bitwise_out.w64[1] = typegrind ? 0xfffffffffffffffful : 0x0;
  }
}

/*! AD handling of logical "and" for F32 and F64 type.
 *
 * A logical "and" with 64-bit 0b0111... could be an absolute
 * value operation on F64, treat it accordingly. Otherwise, it
 * might be a 32-bit absolute value operation on either half.
 */
VG_REGPARM(0) void dg_bar_bitwise_and64(ULong x, ULong xiLo, ULong xiHi, ULong y, ULong yiLo, ULong yiHi){
  DG_HANDLE_AND((&dg_bar_bitwise_out), double,ULong, x, y)
  else DG_HANDLE_AND((&dg_bar_bitwise_out), double,ULong, y, x)
  else DG_HANDLE_HALVES(dg_bar_bitwise_and32)
}

/*--- OR <-> negative abs ---*/
// compare with 0b100...0 and 0b00...0
#define DG_HANDLE_OR(out, fptype, inttype, x, y) \
  if( x == (inttype)(((inttype)1)<<(sizeof(inttype)*8-1)) && *(UInt*)&x##iLo == 0 && *(UInt*)&x##iHi == 0 ){ /* 0b10..0 */ \
    fptype y_f = *(fptype*)&y; \
    if(y_f<=0) { out->w32[0] = *(UInt*)&y##iLo; out->w32[2] = *(UInt*)&y##iHi; } \
    else { \
      ULong yi = assemble64x2to64(y##iLo,y##iHi); \
      ULong minus_yi = tapeAddStatement(yi,0,-1.,0); \
      if(bar_record_values && minus_yi!=0) valuesAddStatement(-y_f); \
      out->w32[0] = *(UInt*)&minus_yi; \
      out->w32[2] = *((UInt*)&minus_yi+1); \
    } \
  } \
  else if( x == 0 && *(UInt*)&x##iLo==0 && *(UInt*)&x##iHi==0 ){ /* 0b0..0 */ \
    out->w32[0] = *(UInt*)&y##iLo; out->w32[2] = *(UInt*)&y##iHi; \
  }

VG_REGPARM(0) void dg_bar_bitwise_or32(UInt x, UInt xiLo, UInt xiHi, UInt y, UInt yiLo, UInt yiHi){
  DG_HANDLE_OR((&dg_bar_bitwise_out), float,UInt, x, y)
  else DG_HANDLE_OR((&dg_bar_bitwise_out), float,UInt, y, x)
  else if (mode=='t'){
    dg_bar_bitwise_out.w64[0] = (xiLo | yiLo);
    dg_bar_bitwise_out.w64[1] = 0xfffffffffffffffful;
  } else {
    dg_bar_bitwise_out.w64[0] = dg_bar_bitwise_out.w64[1] = typegrind ? 0xfffffffffffffffful : 0x0;
  }
}

VG_REGPARM(0) void dg_bar_bitwise_or64(ULong x, ULong xiLo, ULong xiHi, ULong y, ULong yiLo, ULong yiHi){
  DG_HANDLE_OR((&dg_bar_bitwise_out), double,ULong, x, y)
  else DG_HANDLE_OR((&dg_bar_bitwise_out), double,ULong, y, x)
  else DG_HANDLE_HALVES(dg_bar_bitwise_or32)
}

/*--- XOR <-> negative ---*/
// compare with 0b100...0
// in bit-trick-finding mode, also check if both arguments are equal

#define DG_HANDLE_XOR(out, fptype, inttype, x, y) \
  if( x == (inttype)(((inttype)1)<<(sizeof(inttype)*8-1)) && *(UInt*)&x##iLo == 0 && *(UInt*)&x##iHi == 0 ){ \
    fptype y_f = *(fptype*)&y; \
    ULong yi = assemble64x2to64(y##iLo,y##iHi); \
    ULong minus_yi = tapeAddStatement(yi,0,-1.,0); \
    if(bar_record_values && minus_yi!=0) valuesAddStatement(-y_f); \
    out->w32[0] = *(UInt*)&minus_yi; \
    out->w32[2] = *((UInt*)&minus_yi+1); \
  } \
  else if (mode=='t' && x==y && x##iLo == y##iLo && x##iHi == y##iHi) { \
    *(inttype*)&(out->w32[0]) = 0; /* result is not active */ \
    *(inttype*)&(out->w32[2]) = 0; /* result may be floating-point */ \
  }

VG_REGPARM(0) void dg_bar_bitwise_xor32(UInt x, UInt xiLo, UInt xiHi, UInt y, UInt yiLo, UInt yiHi){
  DG_HANDLE_XOR((&dg_bar_bitwise_out), float,UInt, x, y)
  else DG_HANDLE_XOR((&dg_bar_bitwise_out), float,UInt, y, x)
  else if (mode=='t'){
    dg_bar_bitwise_out.w64[0] = (xiLo | yiLo);
    dg_bar_bitwise_out.w64[1] = 0xfffffffffffffffful;
  } else {
    dg_bar_bitwise_out.w64[0] = dg_bar_bitwise_out.w64[1] = typegrind ? 0xfffffffffffffffful : 0x0;
  }
}

VG_REGPARM(0) void dg_bar_bitwise_xor64(ULong x, ULong xiLo, ULong xiHi, ULong y, ULong yiLo, ULong yiHi){
  DG_HANDLE_XOR((&dg_bar_bitwise_out), double,ULong, x, y)
  else DG_HANDLE_XOR((&dg_bar_bitwise_out), double,ULong, y, x)
  else DG_HANDLE_HALVES(dg_bar_bitwise_xor32)
}
