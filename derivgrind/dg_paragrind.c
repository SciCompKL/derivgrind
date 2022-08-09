/*--------------------------------------------------------------------*/
/*--- ParaGrind: Computing results for a shifted    dg_paragrind.c ---*/
/*--- input alongside the original input.                          ---*/
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

#include "pub_tool_basics.h"
#include "pub_tool_tooliface.h"
#include "pub_tool_libcassert.h"
#include "dg_shadow.h"

#include "dg_paragrind.h"

void* sm_pginit;
void* sm_pgdata;

void dg_paragrind_pre_clo_init(void){
  sm_pginit = initializeShadowMap();
  sm_pgdata = initializeShadowMap();
}


/*! Make an expression run "alongside" on shadow memory.
 *
 *  \param[in] ex - Expression.
 *  \param[in,out] diffenv - Additional data necessary for differentiation.
 *  \returns "Parallel" expression.
 */
IRExpr* parallel_expr(IRExpr* ex, DiffEnv* diffenv){
  if(ex->tag==Iex_Qop){
    IRQop* rex = ex->Iex.Qop.details;
    return IRExpr_Qop(rex->op,
      parallel_expr(rex->arg1,diffenv),
      parallel_expr(rex->arg2,diffenv),
      parallel_expr(rex->arg3,diffenv),
      parallel_expr(rex->arg4,diffenv) );
  } else if(ex->tag==Iex_Triop){
    IRTriop* rex = ex->Iex.Triop.details;
    return IRExpr_Triop(rex->op,
      parallel_expr(rex->arg1,diffenv),
      parallel_expr(rex->arg2,diffenv),
      parallel_expr(rex->arg3,diffenv) );
  } else if(ex->tag==Iex_Binop) {
    return IRExpr_Binop(ex->Iex.Binop.op,
      parallel_expr(ex->Iex.Binop.arg1,diffenv),
      parallel_expr(ex->Iex.Binop.arg2,diffenv) );
  } else if(ex->tag==Iex_Unop) {
    return IRExpr_Unop(ex->Iex.Unop.op,
      parallel_expr(ex->Iex.Unop.arg, diffenv) );
  } else if(ex->tag==Iex_Const) {
    return ex;
  } else if(ex->tag==Iex_ITE) {
    return IRExpr_ITE(ex->Iex.ITE.cond,
      parallel_expr(ex->Iex.ITE.iftrue,diffenv),
      parallel_expr(ex->Iex.ITE.iffalse,diffenv) );
  } else if(ex->tag==Iex_RdTmp) {
    return IRExpr_RdTmp( ex->Iex.RdTmp.tmp + diffenv->tmp_offset[2] );
  } else if(ex->tag==Iex_Get) {
    return IRExpr_Get(ex->Iex.Get.offset+diffenv->gs_offset[2],ex->Iex.Get.ty);
  } else if(ex->tag==Iex_GetI) {
    IRRegArray* descr = ex->Iex.GetI.descr;
    IRExpr* ix = ex->Iex.GetI.ix;
    Int bias = ex->Iex.GetI.bias;
    IRRegArray* descr_diff = mkIRRegArray(descr->base+diffenv->gs_offset[2],descr->elemTy,descr->nElems);
    return IRExpr_GetI(descr_diff,ix,bias+diffenv->gs_offset[2]);
  } else if (ex->tag==Iex_Load) {
    return loadLayeredShadowMemory(sm_pgdata,sm_pginit,diffenv->sb_out,ex->Iex.Load.addr,ex->Iex.Load.ty);
  } else if (ex->tag==Iex_CCall) {
    IRExpr** parallel_args = mkIRExprVec_13(NULL,NULL,NULL,NULL,NULL,NULL,
      NULL,NULL,NULL,NULL,NULL,NULL,NULL); // hopefully this is enough...
    int i=0;
    while(ex->Iex.CCall.args[i]!=NULL){
      if(i==13) tl_assert(False);
      parallel_args[i] = parallel_expr(ex->Iex.CCall.args[i],diffenv);
      i++;
    }
    parallel_args[i] = NULL;
    IRExpr_CCall(ex->Iex.CCall.cee,ex->Iex.CCall.retty+diffenv->tmp_offset[2],parallel_args);
  } else {
    tl_assert(False);
  }

}

IRExpr* parallel_or_zero(IRExpr* ex, DiffEnv* diffenv, Bool warn, const char* operation){
  return parallel_expr(ex,diffenv); // there are no errors
}

