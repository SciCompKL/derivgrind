/*--------------------------------------------------------------------*/
/*--- Definition of bit-trick-finding                   dg_trick.c ---*/
/*--- expression handling.                                         ---*/
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

/*! \file dg_trick.c
 *  Define statement handling for the bit-trick finding mode of Derivgrind.
 *
 *  Most of the instrumentation is similar to the recording pass instrumentation
 *  in the sense that two layers of shadow memory are used. Therefore, the 
 *  implementation of the "trick" tool reuses a lot of the "bar" tool functionality.
 */

#include "../dg_expressionhandling.h"

#include "pub_tool_basics.h"
#include "pub_tool_libcassert.h"
#include "pub_tool_tooliface.h"

#include "../dg_shadow.h"
#include "../bar/dg_bar_shadow.h"

#define DG_BAR_H_INCLUDE_TOOL_FUNCTIONS
#include "../bar/dg_bar.h"
#include "dg_trick.h"
#include "dg_trick_bitwise.h"
#include "dg_utils.h"

extern Long* dg_disable;

//! Data is copied to/from shadow memory via this buffer of 2x V256.
V256* dg_trick_shadow_mem_buffer;

#define dg_rounding_mode IRExpr_Const(IRConst_U32(0))

/* --- Define ExpressionHandling. --- */

ULong dg_trick_warn_dirtyhelper( ULong fLo, ULong fHi, ULong size );

#include <VEX/priv/guest_generic_x87.h>
/*! Dirtyhelper for the extra bit-trick-finding logic to dirty calls to
 *  x86g_dirtyhelper_storeF80le / amd64g_dirtyhelper_storeF80le.
 *
 *  If at least one of the 64 activity bits is on, it infect all the
 *  80 shadow bits stored in shadow memory.
 *
 *  If at least one of the 64 discreteness bits is on in addition, issue
 *  a warning message. But the result is non-discrete again.
 */
void dg_trick_x86g_amd64g_dirtyhelper_storeF80le ( Addr addrU, ULong a64Lo, ULong a64Hi )
{
  ULong zero[2], ones[2];
  zero[0] = zero[1] = 0;
  ones[0] = ones[1] = 0xfffffffffffffffful;
  if(a64Lo==0){
    dg_bar_shadowSet((void*)addrU,(void*)zero,(void*)zero,10);
  } else {
    if(a64Hi){
      dg_trick_warn_dirtyhelper(a64Lo,a64Hi,8);
    }
    dg_bar_shadowSet((void*)addrU,(void*)ones,(void*)zero,10);
  }
}
/*! Dirtyhelper for the bit-trick-finding logic to dirty calls to
 *  x86g_dirtyhelper_loadF80le / amd64g_dirtyhelper_loadF80le.
 *
 *  If at least one of the 80 activity bits is on, it infects all the
 *  64 shadow bits of the datum read from memory.
 *
 *  If at least one of the 80 discreteness bits is on in addition, issue
 *  a warning message. But the result is non-discrete again.
 */
ULong dg_trick_x86g_amd64g_dirtyhelper_loadF80le_Lo ( Addr addrU )
{
  ULong a64Lo[2], a64Hi[2];
  dg_bar_shadowGet((void*)addrU, (void*)a64Lo, (void*)a64Hi, 10);
  if(a64Lo[0]!=0 || a64Lo[1]%0x10000!=0) return 0xfffffffffffffful;
  else return 0;
}
ULong dg_trick_x86g_amd64g_dirtyhelper_loadF80le_Hi ( Addr addrU )
{
  ULong a64Lo[2], a64Hi[2];
  dg_bar_shadowGet((void*)addrU, (void*)a64Lo, (void*)a64Hi, 10);
  if(a64Lo[0]!=0 || a64Lo[1]%0x10000!=0){
    if(a64Hi[0]!=0 || a64Hi[1]%0x10000!=0){
      dg_trick_warn_dirtyhelper(a64Lo[0], a64Hi[0], 8); // TODO not quite, two bytes are not reported to user
    }
  }
  return 0;
}

static void dg_trick_dirty_storeF80le(DiffEnv* diffenv, IRExpr* addr, void* expr){
  IRDirty* dd = unsafeIRDirty_0_N(
        0, "dg_trick_x86g_amd64g_dirtyhelper_storeF80le",
        &dg_trick_x86g_amd64g_dirtyhelper_storeF80le,
        mkIRExprVec_3(addr, ((IRExpr**)expr)[0], ((IRExpr**)expr)[1]) );
  addStmtToIRSB(diffenv->sb_out, IRStmt_Dirty(dd));
}

static void dg_trick_dirty_loadF80le(DiffEnv* diffenv, IRExpr* addr, IRTemp temp){
  IRDirty* ddLo = unsafeIRDirty_1_N(
        temp+diffenv->tmp_offset,
        0, "dg_trick_x86g_amd64g_dirtyhelper_loadF80le_Lo",
        &dg_trick_x86g_amd64g_dirtyhelper_loadF80le_Lo,
        mkIRExprVec_1(addr) );
  addStmtToIRSB(diffenv->sb_out, IRStmt_Dirty(ddLo));
  IRDirty* ddHi = unsafeIRDirty_1_N(
        temp+2*diffenv->tmp_offset,
        0, "dg_trick_x86g_amd64g_dirtyhelper_loadF80le_Hi",
        &dg_trick_x86g_amd64g_dirtyhelper_loadF80le_Hi,
        mkIRExprVec_1(addr) );
  addStmtToIRSB(diffenv->sb_out, IRStmt_Dirty(ddHi));
}

ULong dg_trick_warn_dirtyhelper( ULong fLo, ULong fHi, ULong size ){
  ULong mask = (size==4) ? 0x00000000fffffffful : 0xfffffffffffffffful;
  if((dg_disable[VG_(get_running_tid)()]==0) && (fLo & fHi & mask)){
    VG_(message)(Vg_UserMsg, "Active discrete data used as floating-point operand.\n");
    VG_(message)(Vg_UserMsg, "Activity bits: %llu. Discreteness bits: %llu.\n", fLo, fHi);
    VG_(message)(Vg_UserMsg, "At\n");
    VG_(get_and_pp_StackTrace)(VG_(get_running_tid)(), 16);
    VG_(message)(Vg_UserMsg, "\n");
    //VG_(gdbserver)(VG_(get_running_tid)());
  }
}

static void dg_trick_warn4(DiffEnv* diffenv, IRExpr* flagsLo, IRExpr* flagsHi){
  IRDirty* dd = unsafeIRDirty_0_N(
        0, "dg_trick_warn_dirtyhelper",
        &dg_trick_warn_dirtyhelper,
        mkIRExprVec_3(flagsLo,flagsHi,IRExpr_Const(IRConst_U64(4))) );
  addStmtToIRSB(diffenv->sb_out, IRStmt_Dirty(dd));
}

static void dg_trick_warn8(DiffEnv* diffenv, IRExpr* flagsLo, IRExpr* flagsHi){
  IRDirty* dd = unsafeIRDirty_0_N(
        0, "dg_trick_warn_dirtyhelper",
        &dg_trick_warn_dirtyhelper,
        mkIRExprVec_3(flagsLo,flagsHi,IRExpr_Const(IRConst_U64(8))) );
  addStmtToIRSB(diffenv->sb_out, IRStmt_Dirty(dd));
}

void* dg_trick_operation(DiffEnv* diffenv, IROp op,
                         IRExpr* arg1, IRExpr* arg2, IRExpr* arg3, IRExpr* arg4,
                         void* f1, void* f2, void* f3, void* f4){
  IRExpr *f1Lo=NULL, *f1Hi=NULL, *f2Lo=NULL, *f2Hi=NULL, *f3Lo=NULL, *f3Hi=NULL, *f4Lo=NULL, *f4Hi=NULL;
  if(f1) { f1Lo = ((IRExpr**)f1)[0]; f1Hi = ((IRExpr**)f1)[1]; }
  if(f2) { f2Lo = ((IRExpr**)f2)[0]; f2Hi = ((IRExpr**)f2)[1]; }
  if(f3) { f3Lo = ((IRExpr**)f3)[0]; f3Hi = ((IRExpr**)f3)[1]; }
  if(f4) { f4Lo = ((IRExpr**)f4)[0]; f4Hi = ((IRExpr**)f4)[1]; }
  switch(op){
    #include "dg_trick_operations.c"
    default: {
      IRType t_dst=Ity_INVALID, t_arg1=Ity_INVALID, t_arg2=Ity_INVALID, t_arg3=Ity_INVALID, t_arg4=Ity_INVALID;
      typeOfPrimop(op, &t_dst, &t_arg1, &t_arg2, &t_arg3, &t_arg4);
      // activity is infectious
      IRExpr* notActive = IRExpr_Const(IRConst_U1(True));
      if(f1Lo) notActive = IRExpr_Binop(Iop_And1, notActive, isZero(f1Lo, t_arg1));
      if(f2Lo) notActive = IRExpr_Binop(Iop_And1, notActive, isZero(f2Lo, t_arg2));
      if(f3Lo) notActive = IRExpr_Binop(Iop_And1, notActive, isZero(f3Lo, t_arg3));
      if(f4Lo) notActive = IRExpr_Binop(Iop_And1, notActive, isZero(f4Lo, t_arg4));
      IRExpr* flagsLo;
      if(t_dst==Ity_I128){ // special treatment of I128 and I1 because of ISEL error
        IRExpr* i64 = IRExpr_ITE(notActive, mkIRConst_zero(Ity_I64), mkIRConst_ones(Ity_I64));
        flagsLo = IRExpr_Binop(Iop_64HLto128, i64, i64);
      } else if(t_dst==Ity_I1){
        flagsLo = IRExpr_Unop(Iop_Not1, notActive);
      } else {
        flagsLo =  IRExpr_ITE(notActive, mkIRConst_zero(t_dst), mkIRConst_ones(t_dst));
      }
      // unhandled instruction means discrete data
      IRExpr* flagsHi = mkIRConst_ones(t_dst);
      return mkIRExprVec_2(flagsLo, flagsHi);
    }
  }
}

const ExpressionHandling dg_trick_expressionhandling = {
  &dg_bar_wrtmp,&dg_bar_rdtmp,
  &dg_bar_puti,&dg_bar_geti,
  &dg_bar_store,&dg_bar_load,
  &dg_trick_dirty_storeF80le,&dg_trick_dirty_loadF80le,
  &dg_bar_constant,&dg_bar_default_,
  &dg_bar_compare,&dg_bar_ite,
  &dg_trick_operation
};

void dg_trick_handle_statement(DiffEnv* diffenv, IRStmt* st_orig){
  add_statement_modified(diffenv,dg_trick_expressionhandling,st_orig);
}

void dg_trick_initialize(void){
  dg_bar_shadow_mem_buffer = VG_(malloc)("dg_bar_shadow_mem_buffer",2*sizeof(V256));
  dg_bar_shadowInit();
}

void dg_trick_finalize(void){
  VG_(free)(dg_bar_shadow_mem_buffer);
  dg_bar_shadowFini();
}

