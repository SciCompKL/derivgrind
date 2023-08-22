/*--------------------------------------------------------------------*/
/*--- Definition of forward-mode expression handling.     dg_dot.c ---*/
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

/*! \file dg_dot.c
 *  Define statement handling for the forward mode of AD.
 */

#include "../dg_expressionhandling.h"

#include "pub_tool_basics.h"
#include "pub_tool_libcassert.h"
#include "pub_tool_tooliface.h"

#include "../dg_shadow.h"
#include "dg_dot_shadow.h"

#include "dg_dot.h"
#include "dg_dot_bitwise.h"
#include "dg_dot_minmax.h"
#include "dg_dot_diffquotdebug.h"

//! Data is copied to/from shadow memory via this buffer of 1x V256.
V256* dg_dot_shadow_mem_buffer;

extern Bool diffquotdebug;
extern const HChar* diffquotdebug_directory;

#define dg_rounding_mode IRExpr_Const(IRConst_U32(0))

/* --- Define ExpressionHandling. --- */

static void dg_dot_wrtmp(DiffEnv* diffenv, IRTemp temp, void* expr){
  IRStmt* sp = IRStmt_WrTmp(temp+diffenv->tmp_offset, (IRExpr*)expr);
  addStmtToIRSB(diffenv->sb_out,sp);
}
static void* dg_dot_rdtmp(DiffEnv* diffenv, IRTemp temp){
  return (void*)IRExpr_RdTmp(temp+diffenv->tmp_offset);
}

static void dg_dot_puti(DiffEnv* diffenv, Int offset, void* expr, IRRegArray* descr, IRExpr* ix){
  if(descr){ // PutI
    IRRegArray* shadow_descr = mkIRRegArray(descr->base+diffenv->gs_offset, descr->elemTy, descr->nElems);
    IRStmt* sp = IRStmt_PutI(mkIRPutI(shadow_descr,ix,offset+diffenv->gs_offset,(IRExpr*)expr));
    addStmtToIRSB(diffenv->sb_out, sp);
  } else { // Put
    IRStmt* sp = IRStmt_Put(offset+diffenv->gs_offset, (IRExpr*)expr);
    addStmtToIRSB(diffenv->sb_out, sp);
  }
}
static void* dg_dot_geti(DiffEnv* diffenv, Int offset, IRType type, IRRegArray* descr, IRExpr* ix){
  if(descr){ // GetI
    IRRegArray* shadow_descr = mkIRRegArray(descr->base+diffenv->gs_offset,descr->elemTy,descr->nElems);
    return (void*)IRExpr_GetI(shadow_descr,ix,offset+diffenv->gs_offset);
  } else { // Get
    return (void*)IRExpr_Get(offset+diffenv->gs_offset,type);
  }
}
/*! Dirty call to copy shadow data from buffer into shadow memory.
 *  \param addr Address for memory location whose shadow should be written to.
 *  \param size Number of bytes per layer to be copied.
 */
void dg_dot_x86g_amd64g_dirtyhelper_store(Addr addr, ULong size){
  dg_dot_shadowSet((void*)addr,dg_dot_shadow_mem_buffer,size);
}

/*! Dirty call to copy shadow data from shadow memory into buffer.
 *  \param addr Address for memory location whose shadow should be read from.
 *  \param size Number of bytes per layer to be copied.
 */
void dg_dot_x86g_amd64g_dirtyhelper_load(Addr addr, ULong size){
  dg_dot_shadowGet((void*)addr,dg_dot_shadow_mem_buffer,size);
}

static void dg_dot_store(DiffEnv* diffenv, IRExpr* addr, void* expr, IRExpr* guard){
  #ifdef BUILD_32BIT
  IRExpr* buffer_addr = IRExpr_Const(IRConst_U32((Addr)dg_dot_shadow_mem_buffer));
  #else
  IRExpr* buffer_addr = IRExpr_Const(IRConst_U64((Addr)dg_dot_shadow_mem_buffer));
  #endif
  addStmtToIRSB(diffenv->sb_out,IRStmt_Store(Iend_LE,buffer_addr,(IRExpr*)expr));
  IRType type = typeOfIRExpr(diffenv->sb_out->tyenv, (IRExpr*)expr);
  ULong size = sizeofIRType(type);
  IRDirty* dd = unsafeIRDirty_0_N(
        0, "dg_dot_x86g_amd64g_dirtyhelper_store",
        &dg_dot_x86g_amd64g_dirtyhelper_store,
        mkIRExprVec_2(addr,IRExpr_Const(IRConst_U64(size))) );
  if(guard) dd->guard=guard;
  addStmtToIRSB(diffenv->sb_out, IRStmt_Dirty(dd));
}

static void* dg_dot_load(DiffEnv* diffenv, IRExpr* addr, IRType type){
  #ifdef BUILD_32BIT
  IRExpr* buffer_addr = IRExpr_Const(IRConst_U32((Addr)dg_dot_shadow_mem_buffer));
  #else
  IRExpr* buffer_addr = IRExpr_Const(IRConst_U64((Addr)dg_dot_shadow_mem_buffer));
  #endif
  ULong size = sizeofIRType(type);
  IRDirty* dd = unsafeIRDirty_0_N(
        0, "dg_dot_x86g_amd64g_dirtyhelper_load",
        &dg_dot_x86g_amd64g_dirtyhelper_load,
        mkIRExprVec_2(addr,IRExpr_Const(IRConst_U64(size))) );
  addStmtToIRSB(diffenv->sb_out, IRStmt_Dirty(dd));
  IRTemp ex_tmp = newIRTemp(diffenv->sb_out->tyenv,type);
  addStmtToIRSB(diffenv->sb_out,IRStmt_WrTmp(ex_tmp,IRExpr_Load(Iend_LE,type,buffer_addr)));
  return (void*)IRExpr_RdTmp(ex_tmp);
}

#include <VEX/priv/guest_generic_x87.h>
/*! Dirtyhelper for the extra AD logic to dirty calls to
 *  x86g_dirtyhelper_storeF80le / amd64g_dirtyhelper_storeF80le.
 *
 *  It's very similar, but writes to shadow memory instead
 *  of guest memory.
 */
void dg_dot_x86g_amd64g_dirtyhelper_storeF80le ( Addr addrU, ULong f64 )
{
   ULong f128[2];
   convert_f64le_to_f80le( (UChar*)&f64, (UChar*)f128 );
   dg_dot_shadowSet((void*)addrU,(void*)f128,10);
}
/*! Dirtyhelper for the extra AD logic to dirty calls to
 *  x86g_dirtyhelper_loadF80le / amd64g_dirtyhelper_loadF80le.
 *
 *  It's very similar, but reads from shadow memory
 *  instead of guest memory:
 *  - Read 128 bit from the shadow memory,
 *  - reinterpret its first 80 bit as an x87 extended double,
 *  - cast it to a 64 bit double,
 *  - reinterpret it as an unsigned long.
 *  - return this.
 */
ULong dg_dot_x86g_amd64g_dirtyhelper_loadF80le ( Addr addrU )
{
   ULong f64, f128[2];
   dg_dot_shadowGet((void*)addrU, (void*)f128, 10);
   convert_f80le_to_f64le ( (UChar*)f128, (UChar*)&f64 );
   return f64;
}

static void dg_dot_dirty_storeF80le(DiffEnv* diffenv, IRExpr* addr, void* expr){
  IRDirty* dd = unsafeIRDirty_0_N(
        0, "dg_dot_x86g_amd64g_dirtyhelper_storeF80le",
        &dg_dot_x86g_amd64g_dirtyhelper_storeF80le,
        mkIRExprVec_2(addr, expr) );
  addStmtToIRSB(diffenv->sb_out, IRStmt_Dirty(dd));
}

static void dg_dot_dirty_loadF80le(DiffEnv* diffenv, IRExpr* addr, IRTemp temp){
  IRDirty* dd = unsafeIRDirty_1_N(
        temp+diffenv->tmp_offset,
        0, "dg_dot_x86g_amd64g_dirtyhelper_loadF80le",
        &dg_dot_x86g_amd64g_dirtyhelper_loadF80le,
        mkIRExprVec_1(addr) );
  addStmtToIRSB(diffenv->sb_out, IRStmt_Dirty(dd));
}

static void* dg_dot_constant(DiffEnv* diffenv, IRConstTag type){
  switch(type){
    case Ico_F64: return IRExpr_Const(IRConst_F64(0.));
    case Ico_F64i: return IRExpr_Const(IRConst_F64i(0));    // TODO why are these not Ity_F64 etc. types?
    case Ico_F32: return IRExpr_Const(IRConst_F32(0.));
    case Ico_F32i: return IRExpr_Const(IRConst_F32i(0));
    case Ico_U1: return IRExpr_Const(IRConst_U1(0));
    case Ico_U8: return IRExpr_Const(IRConst_U8(0));
    case Ico_U16: return IRExpr_Const(IRConst_U16(0));
    case Ico_U32: return IRExpr_Const(IRConst_U32(0));
    case Ico_U64: return IRExpr_Const(IRConst_U64(0));
    case Ico_U128: return IRExpr_Const(IRConst_U128(0));
    case Ico_V128: return IRExpr_Const(IRConst_V128(0));
    case Ico_V256: return IRExpr_Const(IRConst_V256(0));
    default: tl_assert(False); return NULL;
  }
}

static void* dg_dot_default_(DiffEnv* diffenv, IRType type){
  return mkIRConst_zero(type);
}

static IRExpr* dg_dot_compare(DiffEnv* diffenv, void* arg1, void* arg2){
  IROp cmp;
  IRType type = typeOfIRExpr(diffenv->sb_out->tyenv,arg1);
  tl_assert(type == typeOfIRExpr(diffenv->sb_out->tyenv,arg2));
  switch(type){
    case Ity_I8: cmp = Iop_CmpEQ8; break;
    case Ity_I16: cmp = Iop_CmpEQ16; break;
    case Ity_I32: cmp = Iop_CmpEQ32; break;
    case Ity_I64: cmp = Iop_CmpEQ64; break;
    default: VG_(printf)("Unhandled type in dg_dot_compare.\n"); tl_assert(False); break;
  }
  return IRExpr_Binop(cmp,arg1,arg2);
}

static void* dg_dot_ite(DiffEnv* diffenv, IRExpr* cond, void* dtrue, void* dfalse){
  return IRExpr_ITE(cond,dtrue,dfalse);
}

void* dg_dot_operation(DiffEnv* diffenv, IROp op,
                         IRExpr* arg1, IRExpr* arg2, IRExpr* arg3, IRExpr* arg4,
                         void* d1, void* d2, void* d3, void* d4){
  switch(op){
    #include "dg_dot_operations.c"
    default: return NULL;
  }
}

const ExpressionHandling dg_dot_expressionhandling = {
  &dg_dot_wrtmp,&dg_dot_rdtmp,
  &dg_dot_puti,&dg_dot_geti,
  &dg_dot_store,&dg_dot_load,
  &dg_dot_dirty_storeF80le,&dg_dot_dirty_loadF80le,
  &dg_dot_constant,&dg_dot_default_,
  &dg_dot_compare,&dg_dot_ite,
  &dg_dot_operation
};

void dg_dot_handle_statement(DiffEnv* diffenv, IRStmt* st_orig){
  add_statement_modified(diffenv,dg_dot_expressionhandling,st_orig);
}

void dg_dot_initialize(void){
  dg_dot_shadow_mem_buffer = VG_(malloc)("dg_dot_shadow_mem_buffer",sizeof(V256));
  dg_dot_shadowInit();
  if(diffquotdebug) dg_dot_diffquotdebug_initialize(diffquotdebug_directory);
}

void dg_dot_finalize(void){
  if(diffquotdebug) dg_dot_diffquotdebug_finalize();
  VG_(free)(dg_dot_shadow_mem_buffer);
  dg_dot_shadowFini();
}

