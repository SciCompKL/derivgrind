/*--------------------------------------------------------------------*/
/*--- Definition of recording-mode expression handling.   dg_bar.c ---*/
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

/*! \file dg_bar.c
 *  Define statement handling for the recording / reverse mode of AD.
 */

#include "../dg_expressionhandling.h"

#include "pub_tool_basics.h"
#include "pub_tool_libcassert.h"
#include "pub_tool_tooliface.h"

#include "../dg_shadow.h"
#include "dg_bar_shadow.h"

#include "dg_bar.h"
#include "dg_bar_bitwise.h"
#include "dg_bar_tape.h"
#include "dg_utils.h"

//! Whether to return 0xff..f for unhandled operations, otherwise 0x00..0.
Bool typegrind = False;

//! Whether to  record values of results besides indices and partial derivatives.
Bool bar_record_values = False;

//! Data is copied to/from shadow memory via this buffer of 2x V256.
V256* dg_bar_shadow_mem_buffer;

#define dg_rounding_mode IRExpr_Const(IRConst_U32(0))

/* --- Define ExpressionHandling. --- */
// some functions are not static because the trick-instrumention also needs them

void dg_bar_wrtmp(DiffEnv* diffenv, IRTemp temp, void* expr){
  IRStmt* spLo = IRStmt_WrTmp(temp+diffenv->tmp_offset, ((IRExpr**)expr)[0]);
  addStmtToIRSB(diffenv->sb_out,spLo);
  IRStmt* spHi = IRStmt_WrTmp(temp+2*diffenv->tmp_offset, ((IRExpr**)expr)[1]);
  addStmtToIRSB(diffenv->sb_out,spHi);
}
void* dg_bar_rdtmp(DiffEnv* diffenv, IRTemp temp){
  IRExpr* exLo = IRExpr_RdTmp(temp+diffenv->tmp_offset);
  IRExpr* exHi = IRExpr_RdTmp(temp+2*diffenv->tmp_offset);
  return (void*)mkIRExprVec_2(exLo,exHi);
}

void dg_bar_puti(DiffEnv* diffenv, Int offset, void* expr, IRRegArray* descr, IRExpr* ix){
  if(descr){ // PutI
    IRRegArray* shadow_descrLo = mkIRRegArray(descr->base+diffenv->gs_offset, descr->elemTy, descr->nElems);
    IRStmt* spLo = IRStmt_PutI(mkIRPutI(shadow_descrLo,ix,offset+diffenv->gs_offset,((IRExpr**)expr)[0]));
    addStmtToIRSB(diffenv->sb_out, spLo);
    IRRegArray* shadow_descrHi = mkIRRegArray(descr->base+2*diffenv->gs_offset, descr->elemTy, descr->nElems);
    IRStmt* spHi = IRStmt_PutI(mkIRPutI(shadow_descrHi,ix,offset+2*diffenv->gs_offset,((IRExpr**)expr)[1]));
    addStmtToIRSB(diffenv->sb_out, spHi);
  } else { // Put
    IRStmt* spLo = IRStmt_Put(offset+diffenv->gs_offset, ((IRExpr**)expr)[0]);
    addStmtToIRSB(diffenv->sb_out, spLo);
    IRStmt* spHi = IRStmt_Put(offset+2*diffenv->gs_offset, ((IRExpr**)expr)[1]);
    addStmtToIRSB(diffenv->sb_out, spHi);
  }
}
void* dg_bar_geti(DiffEnv* diffenv, Int offset, IRType type, IRRegArray* descr, IRExpr* ix){
  if(descr){ // GetI
    IRRegArray* shadow_descrLo = mkIRRegArray(descr->base+diffenv->gs_offset,descr->elemTy,descr->nElems);
    IRExpr* exLo = IRExpr_GetI(shadow_descrLo,ix,offset+diffenv->gs_offset);
    IRRegArray* shadow_descrHi = mkIRRegArray(descr->base+2*diffenv->gs_offset,descr->elemTy,descr->nElems);
    IRExpr* exHi = IRExpr_GetI(shadow_descrHi,ix,offset+2*diffenv->gs_offset);
    return (void*)mkIRExprVec_2(exLo,exHi);
  } else { // Get
    IRExpr* exLo = IRExpr_Get(offset+diffenv->gs_offset,type);
    IRExpr* exHi = IRExpr_Get(offset+2*diffenv->gs_offset,type);
    return (void*)mkIRExprVec_2(exLo,exHi);
  }
}

/*! Dirty call to copy shadow data from buffer into shadow memory.
 *  \param addr Address for memory location whose shadow should be written to.
 *  \param size Number of bytes per layer to be copied.
 */
void dg_bar_x86g_amd64g_dirtyhelper_store(Addr addr, ULong size){
  dg_bar_shadowSet((void*)addr,dg_bar_shadow_mem_buffer,dg_bar_shadow_mem_buffer+1,size);
}

/*! Dirty call to copy shadow data from shadow memory into buffer.
 *  \param addr Address for memory location whose shadow should be read from.
 *  \param size Number of bytes per layer to be copied.
 */
void dg_bar_x86g_amd64g_dirtyhelper_load(Addr addr, ULong size){
  dg_bar_shadowGet((void*)addr,dg_bar_shadow_mem_buffer,dg_bar_shadow_mem_buffer+1,size);
}

void dg_bar_store(DiffEnv* diffenv, IRExpr* addr, void* expr, IRExpr* guard){
  #ifdef BUILD_32BIT
  IRExpr* buffer_addr_Lo = IRExpr_Const(IRConst_U32((Addr)dg_bar_shadow_mem_buffer));
  IRExpr* buffer_addr_Hi = IRExpr_Const(IRConst_U32((Addr)(dg_bar_shadow_mem_buffer+1)));
  #else
  IRExpr* buffer_addr_Lo = IRExpr_Const(IRConst_U64((Addr)dg_bar_shadow_mem_buffer));
  IRExpr* buffer_addr_Hi = IRExpr_Const(IRConst_U64((Addr)(dg_bar_shadow_mem_buffer+1)));
  #endif
  addStmtToIRSB(diffenv->sb_out,IRStmt_Store(Iend_LE,buffer_addr_Lo,((IRExpr**)expr)[0]));
  addStmtToIRSB(diffenv->sb_out,IRStmt_Store(Iend_LE,buffer_addr_Hi,((IRExpr**)expr)[1]));
  IRType type = typeOfIRExpr(diffenv->sb_out->tyenv, ((IRExpr**)expr)[0]);
  tl_assert(type == typeOfIRExpr(diffenv->sb_out->tyenv, ((IRExpr**)expr)[1]));
  ULong size = sizeofIRType(type);
  IRDirty* dd = unsafeIRDirty_0_N(
        0, "dg_bar_x86g_amd64g_dirtyhelper_store",
        &dg_bar_x86g_amd64g_dirtyhelper_store,
        mkIRExprVec_2(addr,IRExpr_Const(IRConst_U64(size))) );
  if(guard) dd->guard=guard;
  addStmtToIRSB(diffenv->sb_out, IRStmt_Dirty(dd));
}
void* dg_bar_load(DiffEnv* diffenv, IRExpr* addr, IRType type){
  #ifdef BUILD_32BIT
  IRExpr* buffer_addr_Lo = IRExpr_Const(IRConst_U32((Addr)dg_bar_shadow_mem_buffer));
  IRExpr* buffer_addr_Hi = IRExpr_Const(IRConst_U32((Addr)(dg_bar_shadow_mem_buffer+1)));
  #else
  IRExpr* buffer_addr_Lo = IRExpr_Const(IRConst_U64((Addr)dg_bar_shadow_mem_buffer));
  IRExpr* buffer_addr_Hi = IRExpr_Const(IRConst_U64((Addr)(dg_bar_shadow_mem_buffer+1)));
  #endif
  ULong size = sizeofIRType(type);
  IRDirty* dd = unsafeIRDirty_0_N(
        0, "dg_bar_x86g_amd64g_dirtyhelper_load",
        &dg_bar_x86g_amd64g_dirtyhelper_load,
        mkIRExprVec_2(addr,IRExpr_Const(IRConst_U64(size))) );
  addStmtToIRSB(diffenv->sb_out, IRStmt_Dirty(dd));
  IRTemp exLo_tmp = newIRTemp(diffenv->sb_out->tyenv,type);
  IRTemp exHi_tmp = newIRTemp(diffenv->sb_out->tyenv,type);
  addStmtToIRSB(diffenv->sb_out,IRStmt_WrTmp(exLo_tmp,IRExpr_Load(Iend_LE,type,buffer_addr_Lo)));
  addStmtToIRSB(diffenv->sb_out,IRStmt_WrTmp(exHi_tmp,IRExpr_Load(Iend_LE,type,buffer_addr_Hi)));
  return (void*)mkIRExprVec_2(IRExpr_RdTmp(exLo_tmp),IRExpr_RdTmp(exHi_tmp));
}

#include <VEX/priv/guest_generic_x87.h>
/*! Dirtyhelper for the extra AD logic to dirty calls to
 *  x86g_dirtyhelper_storeF80le / amd64g_dirtyhelper_storeF80le.
 *
 *  It just writes the lower 4 bytes of the index to the beginning
 *  of the 80-bit blocks in the lower layer of shadow memory.
 */
void dg_bar_x86g_amd64g_dirtyhelper_storeF80le_Lo ( Addr addrU, ULong i64 )
{
  dg_bar_shadowSet((void*)addrU,(void*)&i64,NULL,4);
}
void dg_bar_x86g_amd64g_dirtyhelper_storeF80le_Hi ( Addr addrU, ULong i64 )
{
  dg_bar_shadowSet((void*)addrU,NULL,(void*)&i64,4);
}
/*! Dirtyhelper for the extra AD logic to dirty calls to
 *  x86g_dirtyhelper_loadF80le / amd64g_dirtyhelper_loadF80le.
 *
 *  It just reads the lower 4 bytes of the index from the beginning
 *  of the 80-bit blocks in the lower layer of shadow memory.
 */
ULong dg_bar_x86g_amd64g_dirtyhelper_loadF80le_Lo ( Addr addrU )
{
  ULong i64_Lo, i64_Hi;
  dg_bar_shadowGet((void*)addrU, (void*)&i64_Lo, (void*)&i64_Hi, 4);
  return i64_Lo;
}
ULong dg_bar_x86g_amd64g_dirtyhelper_loadF80le_Hi ( Addr addrU )
{
  ULong i64_Lo, i64_Hi;
  dg_bar_shadowGet((void*)addrU, (void*)&i64_Lo, (void*)&i64_Hi, 4);
  return i64_Hi;
}

void dg_bar_dirty_storeF80le(DiffEnv* diffenv, IRExpr* addr, void* expr){
  IRDirty* ddLo = unsafeIRDirty_0_N(
        0, "dg_bar_x86g_amd64g_dirtyhelper_storeF80le_Lo",
        &dg_bar_x86g_amd64g_dirtyhelper_storeF80le_Lo,
        mkIRExprVec_2(addr, ((IRExpr**)expr)[0]) );
  addStmtToIRSB(diffenv->sb_out, IRStmt_Dirty(ddLo));
  IRDirty* ddHi = unsafeIRDirty_0_N(
        0, "dg_bar_x86g_amd64g_dirtyhelper_storeF80le_Hi",
        &dg_bar_x86g_amd64g_dirtyhelper_storeF80le_Hi,
        mkIRExprVec_2(addr, ((IRExpr**)expr)[1]) );
  addStmtToIRSB(diffenv->sb_out, IRStmt_Dirty(ddHi));
}

void dg_bar_dirty_loadF80le(DiffEnv* diffenv, IRExpr* addr, IRTemp temp){
  IRDirty* ddLo = unsafeIRDirty_1_N(
        temp+diffenv->tmp_offset,
        0, "dg_bar_x86g_amd64g_dirtyhelper_loadF80le_Lo",
        &dg_bar_x86g_amd64g_dirtyhelper_loadF80le_Lo,
        mkIRExprVec_1(addr) );
  addStmtToIRSB(diffenv->sb_out, IRStmt_Dirty(ddLo));
  IRDirty* ddHi = unsafeIRDirty_1_N(
        temp+2*diffenv->tmp_offset,
        0, "dg_bar_x86g_amd64g_dirtyhelper_loadF80le_Hi",
        &dg_bar_x86g_amd64g_dirtyhelper_loadF80le_Hi,
        mkIRExprVec_1(addr) );
  addStmtToIRSB(diffenv->sb_out, IRStmt_Dirty(ddHi));
}

void* dg_bar_constant(DiffEnv* diffenv, IRConstTag type){
  IRExpr* zero;
  switch(type){
    case Ico_F64: zero = IRExpr_Unop(Iop_ReinterpI64asF64,IRExpr_Const(IRConst_U64(0))); break;
    case Ico_F64i: zero = IRExpr_Const(IRConst_F64i(0)); break;
    case Ico_F32: zero = IRExpr_Unop(Iop_ReinterpI32asF32,IRExpr_Const(IRConst_U32(0))); break;
    case Ico_F32i: zero = IRExpr_Unop(Iop_ReinterpI32asF32,IRExpr_Const(IRConst_U32(0))); break;
    case Ico_U1: zero = IRExpr_Const(IRConst_U1(0)); break;
    case Ico_U8: zero = IRExpr_Const(IRConst_U8(0)); break;
    case Ico_U16: zero = IRExpr_Const(IRConst_U16(0)); break;
    case Ico_U32: zero = IRExpr_Const(IRConst_U32(0)); break;
    case Ico_U64: zero = IRExpr_Const(IRConst_U64(0)); break;
    case Ico_U128: zero = IRExpr_Const(IRConst_U128(0)); break;
    case Ico_V128: zero = IRExpr_Const(IRConst_V128(0)); break;
    case Ico_V256: zero = IRExpr_Const(IRConst_V256(0)); break;
    default: tl_assert(False); return NULL;
  }
  return mkIRExprVec_2(zero,zero);
}

void* dg_bar_default_(DiffEnv* diffenv, IRType type){
  IRExpr* zeroU = IRExpr_Const(IRConst_U64(0));
  IRExpr* zero;
  switch(type){
    case Ity_INVALID: tl_assert(False); return NULL;
    case Ity_I1: zero = IRExpr_Const(IRConst_U1(0)); break;
    case Ity_I8: zero = IRExpr_Const(IRConst_U8(0)); break;
    case Ity_I16: zero = IRExpr_Const(IRConst_U16(0)); break;
    case Ity_I32: zero = IRExpr_Const(IRConst_U32(0)); break;
    case Ity_I64: zero = zeroU; break;
    case Ity_I128: zero = IRExpr_Const(IRConst_U128(0)); break;
    case Ity_F32: zero = IRExpr_Unop(Iop_ReinterpI32asF32,IRExpr_Const(IRConst_U32(0))); break;
    case Ity_F64: zero = IRExpr_Const(IRConst_F64i(0)); break;
    case Ity_D64: zero = IRExpr_Unop(Iop_ReinterpI64asD64,zeroU); break;
    case Ity_F128: zero = IRExpr_Unop(Iop_ReinterpI128asF128,IRExpr_Const(IRConst_U128(0))); break;
    case Ity_V128: zero = IRExpr_Binop(Iop_64HLtoV128, zeroU, zeroU); break;
    case Ity_V256: zero = IRExpr_Qop(Iop_64x4toV256, zeroU, zeroU, zeroU, zeroU); break;
    default: tl_assert(False); return NULL;
  }
  return mkIRExprVec_2(zero,zero);
}

IRExpr* dg_bar_compare(DiffEnv* diffenv, void* arg1, void* arg2){
  IROp cmp;
  IRType type = typeOfIRExpr(diffenv->sb_out->tyenv,((IRExpr**)arg1)[0]);
  tl_assert(type == typeOfIRExpr(diffenv->sb_out->tyenv,((IRExpr**)arg2)[0]));
  switch(type){
    case Ity_I8: cmp = Iop_CmpEQ8; break;
    case Ity_I16: cmp = Iop_CmpEQ16; break;
    case Ity_I32: cmp = Iop_CmpEQ32; break;
    case Ity_I64: cmp = Iop_CmpEQ64; break;
    default: VG_(printf)("Unhandled type in dg_bar_compare.\n"); tl_assert(False); break;
  }
  IRExpr* cmpLo = IRExpr_Binop(cmp,((IRExpr**)arg1)[0],((IRExpr**)arg2)[0]);
  IRExpr* cmpHi = IRExpr_Binop(cmp,((IRExpr**)arg1)[1],((IRExpr**)arg2)[1]);
  return IRExpr_Binop(Iop_And1,cmpLo,cmpHi);
}

void* dg_bar_ite(DiffEnv* diffenv, IRExpr* cond, void* dtrue, void* dfalse){
  IRExpr* exLo = IRExpr_ITE(cond,((IRExpr**)dtrue)[0],((IRExpr**)dfalse)[0]);
  IRExpr* exHi = IRExpr_ITE(cond,((IRExpr**)dtrue)[1],((IRExpr**)dfalse)[1]);
  return (void*)mkIRExprVec_2(exLo,exHi);
}

ULong dg_bar_writeToTape_call(ULong index1Lo, ULong index1Hi, ULong index2Lo, ULong index2Hi, ULong diff1, ULong diff2){
  // assemble 8-byte indices from 4-byte beginnings in both shadow layers
  UInt index1[2], index2[2];
  index1[0] = *(UInt*)&index1Lo;
  index1[1] = *(UInt*)&index1Hi;
  index2[0] = *(UInt*)&index2Lo;
  index2[1] = *(UInt*)&index2Hi;
  ULong returnindex = tapeAddStatement(*(ULong*)index1,*(ULong*)index2,*(double*)&diff1,*(double*)&diff2);
  return returnindex;
}

void dg_bar_writeToTape_value_call(ULong value, ULong index){
  if(index!=0){
    valuesAddStatement(*(double*)&value);
  }
}

/*! Add dirty call writing to tape.
 * 
 * This function is called from the code in dg_bar_operations.c, which is generated by
 * gen_operationhandling_code.py and #include'd in dg_bar_operation.
 * 
 * \param diffenv - General setup. 
 * \param index1Lo - IRExpr* of type I64 for the lower layer of the index of dependency 1
 * \param index1Hi - IRExpr* of type I64 for the higher layer of the index of dependency 1
 * \param index2Lo - IRExpr* of type I64 for the lower layer of the index of dependency 2
 * \param index2Hi - IRExpr* of type I64 for the higher layer of the index of dependency 2
 * \param diff1 - IRExpr* of type F64 for the partial derivative w.r.t. dependency 1
 * \param diff2 - IRExpr* of type F64 for the partial derivative w.r.t. dependency 2
 * \param value - IRExpr* of type F64 for the value of the result
 * \returns Array of two IRExpr*'s of type I64 for the lower and higher layer of the 
 *   new index assigned to the result.
 *
 */
IRExpr** dg_bar_writeToTape(DiffEnv* diffenv, IRExpr* index1Lo, IRExpr* index1Hi, IRExpr* index2Lo, IRExpr* index2Hi, IRExpr* diff1, IRExpr* diff2, IRExpr* value){
  IRTemp returnindex = newIRTemp(diffenv->sb_out->tyenv,Ity_I64);
  IRDirty* dd = unsafeIRDirty_1_N(
        returnindex,
        0, "dg_bar_writeToTape_call",
        &dg_bar_writeToTape_call,
        mkIRExprVec_6(index1Lo,index1Hi,index2Lo,index2Hi,
          IRExpr_Unop(Iop_ReinterpF64asI64,diff1),
          IRExpr_Unop(Iop_ReinterpF64asI64,diff2) )  );
  addStmtToIRSB(diffenv->sb_out, IRStmt_Dirty(dd));
  if(bar_record_values){
    IRDirty* dd_val = unsafeIRDirty_0_N(
          0, "dg_bar_writeToTape_value_call",
          &dg_bar_writeToTape_value_call,
          mkIRExprVec_2(IRExpr_Unop(Iop_ReinterpF64asI64,value), IRExpr_RdTmp(returnindex)) );
    addStmtToIRSB(diffenv->sb_out, IRStmt_Dirty(dd_val));
  }
  // split I64 returnindex into two I32 layers
  IRExpr* exLo_i32 = IRExpr_Unop(Iop_64to32, IRExpr_RdTmp(returnindex));
  IRExpr* exHi_i32 = IRExpr_Unop(Iop_64HIto32, IRExpr_RdTmp(returnindex));
  // convert to I64
  IRExpr* exLo = IRExpr_Binop(Iop_32HLto64,IRExpr_Const(IRConst_U32(0)),exLo_i32);
  IRExpr* exHi = IRExpr_Binop(Iop_32HLto64,IRExpr_Const(IRConst_U32(0)),exHi_i32);
  return mkIRExprVec_2(exLo,exHi);
}

void* dg_bar_operation(DiffEnv* diffenv, IROp op,
                         IRExpr* arg1, IRExpr* arg2, IRExpr* arg3, IRExpr* arg4,
                         void* i1, void* i2, void* i3, void* i4){
  IRExpr *i1Lo=NULL, *i1Hi=NULL, *i2Lo=NULL, *i2Hi=NULL, *i3Lo=NULL, *i3Hi=NULL, *i4Lo=NULL, *i4Hi=NULL;
  if(i1) { i1Lo = ((IRExpr**)i1)[0]; i1Hi = ((IRExpr**)i1)[1]; }
  if(i2) { i2Lo = ((IRExpr**)i2)[0]; i2Hi = ((IRExpr**)i2)[1]; }
  if(i3) { i3Lo = ((IRExpr**)i3)[0]; i3Hi = ((IRExpr**)i3)[1]; }
  if(i4) { i4Lo = ((IRExpr**)i4)[0]; i4Hi = ((IRExpr**)i4)[1]; }
  switch(op){
    #include "dg_bar_operations.c"
    default: {
      if(typegrind){
        IRType t_dst=Ity_INVALID, t_arg1=Ity_INVALID, t_arg2=Ity_INVALID, t_arg3=Ity_INVALID, t_arg4=Ity_INVALID;
        typeOfPrimop(op, &t_dst, &t_arg1, &t_arg2, &t_arg3, &t_arg4);
        /*if(t_dst==Ity_I128) return NULL; // TODO: Check whether index is zero.
        IRExpr* allInputsZero = IRExpr_Binop(Iop_And1,
          IRExpr_Binop(Iop_And1, isZero(arg1, t_arg1), isZero(arg2, t_arg2)),
          IRExpr_Binop(Iop_And1, isZero(arg3, t_arg3), isZero(arg4, t_arg4)) );
        IRExpr* indexLo = IRExpr_ITE(allInputsZero,mkIRConst_zero(t_dst), mkIRConst_ones(t_dst));
        */
        IRExpr* indexLo = mkIRConst_ones(t_dst);
        IRExpr* indexHi = indexLo;
        return mkIRExprVec_2(indexLo, indexHi);
      } else {
        return NULL;
      }
    }
  }
}

const ExpressionHandling dg_bar_expressionhandling = {
  &dg_bar_wrtmp,&dg_bar_rdtmp,
  &dg_bar_puti,&dg_bar_geti,
  &dg_bar_store,&dg_bar_load,
  &dg_bar_dirty_storeF80le,&dg_bar_dirty_loadF80le,
  &dg_bar_constant,&dg_bar_default_,
  &dg_bar_compare,&dg_bar_ite,
  &dg_bar_operation
};

void dg_bar_handle_statement(DiffEnv* diffenv, IRStmt* st_orig){
  add_statement_modified(diffenv,dg_bar_expressionhandling,st_orig);
}

void dg_bar_initialize(void){
  dg_bar_shadow_mem_buffer = VG_(malloc)("dg_bar_shadow_mem_buffer",2*sizeof(V256));
  dg_bar_shadowInit();
}

void dg_bar_finalize(void){
  VG_(free)(dg_bar_shadow_mem_buffer);
  dg_bar_shadowFini();
}

