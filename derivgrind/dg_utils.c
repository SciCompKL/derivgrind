/*--------------------------------------------------------------------*/
/*--- Utilities.                                        dg_utils.c ---*/
/*--------------------------------------------------------------------*/

/*
   This file is part of DerivGrind, a tool performing forward-mode
   algorithmic differentiation of compiled programs, implemented
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

#include "dg_utils.h"

#include "pub_tool_libcassert.h"


IRExpr* mkIRConst_zero(IRType type){
  IRExpr* zeroF = IRExpr_Const(IRConst_F64(0.));
  IRExpr* zeroU = IRExpr_Const(IRConst_U64(0));
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
