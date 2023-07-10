/*--------------------------------------------------------------------*/
/*--- Derivgrind utilities.                             dg_utils.c ---*/
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

#include "dg_utils.h"

#include "pub_tool_libcassert.h"


IRExpr* mkIRConst_zero(IRType type){
  IRExpr* zeroU = IRExpr_Const(IRConst_U64(0));
  //IRExpr* zeroF = IRExpr_Const(IRConst_F64(0.));
  IRExpr* zeroF = IRExpr_Unop(Iop_ReinterpI64asF64, zeroU);
  switch(type){
    case Ity_INVALID: tl_assert(False); return NULL;
    case Ity_I1: return IRExpr_Const(IRConst_U1(0));
    case Ity_I8: return IRExpr_Const(IRConst_U8(0));
    case Ity_I16: return IRExpr_Const(IRConst_U16(0));
    case Ity_I32: return IRExpr_Const(IRConst_U32(0));
    case Ity_I64: return zeroU;
    case Ity_I128: return IRExpr_Const(IRConst_U128(0));
    case Ity_F16: return IRExpr_Binop(Iop_F64toF16,DEFAULT_ROUNDING,zeroF);
    case Ity_F32: return IRExpr_Const(IRConst_F32(0.));
    case Ity_F64: return zeroF;
    case Ity_D32: return IRExpr_Binop(Iop_F64toD32,DEFAULT_ROUNDING,zeroF);
    case Ity_D64: return IRExpr_Binop(Iop_F64toD64,DEFAULT_ROUNDING,zeroF);
    case Ity_D128: return IRExpr_Binop(Iop_F64toD128,DEFAULT_ROUNDING,zeroF);
    case Ity_F128: return IRExpr_Unop(Iop_F64toF128,zeroF);
    case Ity_V128: return IRExpr_Binop(Iop_64HLtoV128,zeroU,zeroU);
    case Ity_V256: return IRExpr_Qop(Iop_64x4toV256,zeroU,zeroU,zeroU,zeroU);
    default: tl_assert(False); return NULL;
  }
}

IRExpr* mkIRConst_ones(IRType type){
  IRExpr* onesU = IRExpr_Const(IRConst_U64(0xffffffffffffffff));
  switch(type){
    case Ity_INVALID: tl_assert(False); return NULL;
    case Ity_I1: return IRExpr_Const(IRConst_U1(True));
    case Ity_I8: return IRExpr_Const(IRConst_U8(0xff));
    case Ity_I16: return IRExpr_Const(IRConst_U16(0xffff));
    case Ity_I32: return IRExpr_Const(IRConst_U32(0xffffffff));
    case Ity_I64: return onesU;
    case Ity_I128: return IRExpr_Binop(Iop_64HLto128, onesU, onesU);
    case Ity_F32: return IRExpr_Unop(Iop_ReinterpI32asF32, IRExpr_Const(IRConst_U32(0xffffffff)));
    case Ity_F64: return IRExpr_Unop(Iop_ReinterpI64asF64, onesU);
    case Ity_V128: return IRExpr_Binop(Iop_64HLtoV128,onesU,onesU);
    case Ity_V256: return IRExpr_Qop(Iop_64x4toV256,onesU,onesU,onesU,onesU);
    case Ity_D32: case Ity_D64: case Ity_D128: case Ity_F16: case Ity_F128: default:
      tl_assert(False); return NULL;
  }
}

IRExpr* mkIRConst_fptwo(int fpsize, int simdsize){
  if(fpsize==4){
    switch(simdsize){
      case 1:
        return IRExpr_Const(IRConst_F32(2.));
      case 2: {
        IRExpr* two = IRExpr_Unop(Iop_ReinterpF32asI32,mkIRConst_fptwo(4,1));
        return IRExpr_Binop(Iop_32HLto64, two, two);
      }
      case 4: {
        IRExpr* two = mkIRConst_fptwo(4,2);
        return IRExpr_Binop(Iop_64HLtoV128,two,two);
      }
      case 8: {
        IRExpr* two = mkIRConst_fptwo(4,2);
        return IRExpr_Qop(Iop_64x4toV256, two, two, two, two);
      }
    }
  } else {
    switch(simdsize){
      case 1:
        return IRExpr_Const(IRConst_F64(2.));
      case 2: {
        IRExpr* two = IRExpr_Unop(Iop_ReinterpF64asI64, mkIRConst_fptwo(8,1));
        return IRExpr_Binop(Iop_64HLtoV128, two, two);
      }
      case 4: {
        IRExpr* two = IRExpr_Unop(Iop_ReinterpF64asI64, mkIRConst_fptwo(8,1));
        return IRExpr_Qop(Iop_64x4toV256, two, two, two, two);
      }
    }
  }
}

IRExpr* isZero(IRExpr* expr, IRType type){ // TODO check whether index is zero
  switch(type){
    case Ity_INVALID: return IRExpr_Const(IRConst_U1(True));
    case Ity_I1: return IRExpr_Unop(Iop_Not1, expr);
    case Ity_I8: return IRExpr_Binop(Iop_CmpEQ8,expr,IRExpr_Const(IRConst_U8(0)));
    case Ity_I16: return IRExpr_Binop(Iop_CmpEQ16,expr,IRExpr_Const(IRConst_U16(0)));
    case Ity_I32: return IRExpr_Binop(Iop_CmpEQ32,expr,IRExpr_Const(IRConst_U32(0)));
    case Ity_I64: return IRExpr_Binop(Iop_CmpEQ64,expr,IRExpr_Const(IRConst_U64(0)));
    case Ity_I128: return IRExpr_Binop(Iop_And1, isZero(IRExpr_Unop(Iop_128to64,expr), Ity_I64), isZero(IRExpr_Unop(Iop_128HIto64,expr), Ity_I64));
    case Ity_F32: return IRExpr_Binop(Iop_CmpEQ32,IRExpr_Unop(Iop_ReinterpF32asI32,expr),IRExpr_Const(IRConst_U32(0)));
    case Ity_F64: return IRExpr_Binop(Iop_CmpEQ64,IRExpr_Unop(Iop_ReinterpF64asI64,expr),IRExpr_Const(IRConst_U64(0)));
    case Ity_F128: return isZero(IRExpr_Unop(Iop_ReinterpF128asI128,expr),Ity_I128);
    case Ity_V128: return IRExpr_Binop(Iop_And1, isZero(IRExpr_Unop(Iop_V128to64,expr), Ity_I64), isZero(IRExpr_Unop(Iop_V128HIto64,expr), Ity_I64));
    case Ity_V256: return IRExpr_Binop(Iop_And1, isZero(IRExpr_Unop(Iop_V256toV128_0,expr), Ity_V128), isZero(IRExpr_Unop(Iop_V256toV128_1,expr), Ity_V128));
    default: return IRExpr_Const(IRConst_U1(False));
  }
}

IRExpr* getSIMDComponent(IRExpr* expression, int fpsize, int simdsize, int component, DiffEnv* diffenv){
  // reinterpret expression as I32, I64, V128, V256
  IRType type = typeOfIRExpr(diffenv->sb_out->tyenv,expression);
  if(type==Ity_F64) expression = IRExpr_Unop(Iop_ReinterpF64asI64, expression);
  if(type==Ity_F32) expression = IRExpr_Unop(Iop_ReinterpF32asI32, expression);
  if(type==Ity_I128) expression = IRExpr_Unop(Iop_ReinterpI128asV128, expression);
  type = typeOfIRExpr(diffenv->sb_out->tyenv,expression);
  if(type!=Ity_I32&&type!=Ity_I64&&type!=Ity_V128&&type!=Ity_V256){
    VG_(printf)("Bad type in getSIMDComponent.");
    tl_assert(False);
  }
  // extract component
  static const IROp arr64to32[2] = {Iop_64to32,Iop_64HIto32};
  static const IROp arr128to64[2] = {Iop_V128to64,Iop_V128HIto64};
  static const IROp arr256to64[4] = {Iop_V256to64_0,Iop_V256to64_1,Iop_V256to64_2,Iop_V256to64_3};
  IRExpr* zero32 = IRExpr_Const(IRConst_U32(0));
  if(fpsize==4){
    IRExpr* result32;
    switch(simdsize){
      case 1: result32 = expression; break;
      case 2: result32 = IRExpr_Unop(arr64to32[component], expression); break;
      case 4: result32 = IRExpr_Unop(arr64to32[component%2], IRExpr_Unop(arr128to64[component/2], expression)); break;
      case 8: result32 = IRExpr_Unop(arr64to32[component%2], IRExpr_Unop(arr256to64[component/2], expression)); break;
    }
    return result32; // IRExpr_Binop(Iop_32HLto64,zero32,result32);
  } else {
    IRExpr* result;
    switch(simdsize){
      case 1: result = expression; break;
      case 2: result = IRExpr_Unop(arr128to64[component], expression); break;
      case 4: result = IRExpr_Unop(arr256to64[component], expression); break;
    }
    return result;
  }
}

IRExpr* assembleSIMDVector(IRExpr** expressions, int fpsize, int simdsize, DiffEnv* diffenv){
  IRType type = typeOfIRExpr(diffenv->sb_out->tyenv,expressions[0]);
  for(int i=0; i<simdsize; i++){
    tl_assert(typeOfIRExpr(diffenv->sb_out->tyenv,expressions[i])==type);
    if(type==Ity_F64) expressions[i] = IRExpr_Unop(Iop_ReinterpF64asI64,expressions[i]);
    if(type==Ity_F32) expressions[i] = IRExpr_Unop(Iop_ReinterpF32asI32,expressions[i]);
    if(type==Ity_I64 && fpsize==4) expressions[i] = IRExpr_Unop(Iop_64to32, expressions[i]);
  }
  if(sizeofIRType(type)==4){
    switch(simdsize){
      case 1: return expressions[0];
      case 2: return IRExpr_Binop(Iop_32HLto64,expressions[1],expressions[0]);
      case 4: return IRExpr_Binop(Iop_64HLtoV128,
        IRExpr_Binop(Iop_32HLto64,expressions[3],expressions[2]),
        IRExpr_Binop(Iop_32HLto64,expressions[1],expressions[0]) );
      case 8: return IRExpr_Qop(Iop_64x4toV256,
        IRExpr_Binop(Iop_32HLto64,expressions[7],expressions[6]),
        IRExpr_Binop(Iop_32HLto64,expressions[5],expressions[4]),
        IRExpr_Binop(Iop_32HLto64,expressions[3],expressions[2]),
        IRExpr_Binop(Iop_32HLto64,expressions[1],expressions[0]) );
    }
  } else {
    switch(simdsize){
      case 1: return expressions[0];
      case 2: return IRExpr_Binop(Iop_64HLtoV128,expressions[1],expressions[0]);
      case 4: return IRExpr_Qop(Iop_64x4toV256,expressions[3],expressions[2],expressions[1],expressions[0]);
    }
  }
}

IRExpr* reinterpretType(DiffEnv* diffenv, IRExpr* expr,IRType type){
  IRType origtype = typeOfIRExpr(diffenv->sb_out->tyenv, expr);
  tl_assert( sizeofIRType(origtype) == sizeofIRType(type) );
  if(type==origtype) return expr;
  if(type==Ity_F64 && origtype==Ity_I64) return IRExpr_Unop(Iop_ReinterpI64asF64,expr);
  if(type==Ity_I64 && origtype==Ity_F64) return IRExpr_Unop(Iop_ReinterpF64asI64,expr);
  if(type==Ity_F32 && origtype==Ity_I32) return IRExpr_Unop(Iop_ReinterpI32asF32,expr);
  if(type==Ity_I32 && origtype==Ity_F32) return IRExpr_Unop(Iop_ReinterpF32asI32,expr);
  if(type==Ity_I128 && origtype==Ity_V128) return IRExpr_Unop(Iop_ReinterpV128asI128,expr);
  if(type==Ity_V128 && origtype==Ity_I128) return IRExpr_Unop(Iop_ReinterpI128asV128,expr);
  VG_(printf)("Unhandled type combination in reinterpType.\n"); tl_assert(False);
}

IRExpr* convertToF64(IRExpr* expr, DiffEnv* diffenv, IRType* originaltype){
  *originaltype = typeOfIRExpr(diffenv->sb_out->tyenv, expr);
  switch(*originaltype){
    case Ity_F64: return expr;
    case Ity_F32: return IRExpr_Unop(Iop_F32toF64, expr);
    default: VG_(printf)("Bad type in convertToF64.\n"); tl_assert(False); return NULL;
  }
}
/*! Convert F64 expressions to F32 or F64.
 *  \param[in] expr - F64 expression.
 *  \param[in] originaltype - Original type is stored here from convertToF64.
 *  \return F32 or F64 expression.
 */
IRExpr* convertFromF64(IRExpr* expr, IRType originaltype){
  switch(originaltype){
    case Ity_F64: return expr;
    case Ity_F32: return IRExpr_Binop(Iop_F64toF32, IRExpr_Const(IRConst_U32(Irrm_ZERO)), expr);
    default: VG_(printf)("Bad type in convertFromF64.\n"); tl_assert(False); return NULL;
  }
}


void addressesOfCAS(IRCAS const* det, IRSB* sb_out, IRExpr** addr_Lo, IRExpr** addr_Hi){
  IRType type = typeOfIRExpr(sb_out->tyenv,det->expdLo);
  Bool double_element = (det->expdHi!=NULL);
  IRExpr* offset; // offset between Hi and Lo part of addr
  IROp add; // operation to add addresses
  switch(typeOfIRExpr(sb_out->tyenv,det->addr)){
    case Ity_I32:
      add = Iop_Add32;
      offset = IRExpr_Const(IRConst_U32(sizeofIRType(type)));
      break;
    case Ity_I64:
      add = Iop_Add64;
      offset = IRExpr_Const(IRConst_U64(sizeofIRType(type)));
      break;
    default:
      VG_(printf)("Unhandled type for address in translation of Ist_CAS.\n");
      tl_assert(False);
      break;
  }
  if(det->end==Iend_LE){
    if(double_element){
      *addr_Lo = det->addr;
      *addr_Hi = IRExpr_Binop(add,det->addr,offset);
    } else {
      *addr_Lo = det->addr;
      *addr_Hi = NULL;
    }
  } else { // Iend_BE
    if(double_element){
      *addr_Lo = IRExpr_Binop(add,det->addr,offset);
      *addr_Hi = det->addr;
    } else {
      *addr_Lo = det->addr;
      *addr_Hi = NULL;
    }
  }
}
