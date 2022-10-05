/*! \file dg_bar.c
 *  Define statement handling for the recording / reverse mode of AD.
 */

#include "dg_expressionhandling.h"

#include "pub_tool_basics.h"
#include "pub_tool_libcassert.h"
#include "pub_tool_tooliface.h"
#include "dg_dot_bitwise.h"

#include "dg_shadow.h"

#include "dg_bar.h"

//#include "dg_dot_bitwise.h"

//! First layer of shadow memory for the lower 4 byte of the 8-byte indices.
void* sm_barLo = NULL;
//! Second layer of shadow memory for the higher 4 byte of the 8-byte indices.
void* sm_barHi = NULL;


/* --- Define ExpressionHandling. --- */

static void dg_bar_wrtmp(DiffEnv* diffenv, IRTemp temp, void* expr){
  IRStmt* spLo = IRStmt_WrTmp(temp+diffenv->tmp_offset, ((IRExpr**)expr)[0]);
  addStmtToIRSB(diffenv->sb_out,spLo);
  IRStmt* spHi = IRStmt_WrTmp(temp+2*diffenv->tmp_offset, ((IRExpr**)expr)[1]);
  addStmtToIRSB(diffenv->sb_out,spHi);
}
static void* dg_bar_rdtmp(DiffEnv* diffenv, IRTemp temp){
  IRExpr* exLo = IRExpr_RdTmp(temp+diffenv->tmp_offset);
  IRExpr* exHi = IRExpr_RdTmp(temp+2*diffenv->tmp_offset);
  return (void*)mkIRExprVec_2(exLo,exHi);
}

static void dg_bar_puti(DiffEnv* diffenv, Int offset, void* expr, IRRegArray* descr, IRExpr* ix){
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
static void* dg_bar_geti(DiffEnv* diffenv, Int offset, IRType type, IRRegArray* descr, IRExpr* ix){
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

static void dg_bar_store(DiffEnv* diffenv, IRExpr* addr, void* expr, IRExpr* guard){
  storeShadowMemory(sm_barLo,diffenv->sb_out,addr,((IRExpr**)expr)[0],guard);
  storeShadowMemory(sm_barHi,diffenv->sb_out,addr,((IRExpr**)expr)[1],guard);
}
static void* dg_bar_load(DiffEnv* diffenv, IRExpr* addr, IRType type){
  IRExpr* exLo = loadShadowMemory(sm_barLo,diffenv->sb_out,addr,type);
  IRExpr* exHi = loadShadowMemory(sm_barHi,diffenv->sb_out,addr,type);
  return (void*)mkIRExprVec_2(exLo,exHi);
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
   shadowSet(sm_barLo,(void*)addrU,(void*)i64,4);
}
void dg_bar_x86g_amd64g_dirtyhelper_storeF80le_Hi ( Addr addrU, ULong i64 )
{
   shadowSet(sm_barHi,(void*)addrU,(void*)i64,4);
}
/*! Dirtyhelper for the extra AD logic to dirty calls to
 *  x86g_dirtyhelper_loadF80le / amd64g_dirtyhelper_loadF80le.
 *
 *  It just reads the lower 4 bytes of the index from the beginning
 *  of the 80-bit blocks in the lower layer of shadow memory.
 */
ULong dg_bar_x86g_amd64g_dirtyhelper_loadF80le_Lo ( Addr addrU )
{
   ULong i64;
   shadowGet(sm_barLo,(void*)addrU, (void*)&i64, 4);
   return i64;
}
ULong dg_bar_x86g_amd64g_dirtyhelper_loadF80le_Hi ( Addr addrU )
{
   ULong i64;
   shadowGet(sm_barHi,(void*)addrU, (void*)&i64, 4);
   return i64;
}

static void dg_bar_dirty_storeF80le(DiffEnv* diffenv, IRExpr* addr, void* expr){
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

static void dg_bar_dirty_loadF80le(DiffEnv* diffenv, IRExpr* addr, IRTemp temp){
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

static void* dg_bar_constant(DiffEnv* diffenv, IRConstTag type){
  IRExpr* zero;
  switch(type){
    case Ico_F64: zero = IRExpr_Unop(Iop_ReinterpI64asF64,IRExpr_Const(IRConst_U64(0))); break;
    case Ico_F64i: zero = IRExpr_Const(IRConst_F64i(0)); break;
    case Ico_F32: zero = IRExpr_Unop(Iop_ReinterpI32asF32,IRExpr_Const(IRConst_U32(0))); break;
    case Ico_F32i: zero = IRExpr_Const(IRConst_F32i(0)); break;
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

static void* dg_bar_default_(DiffEnv* diffenv, IRType type){
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
    case Ity_F32: zero = IRExpr_Const(IRConst_F32i(0)); break;
    case Ity_F64: zero = IRExpr_Const(IRConst_F64i(0)); break;
    case Ity_D64: zero = IRExpr_Unop(Iop_ReinterpI64asD64,zeroU); break;
    case Ity_F128: zero = IRExpr_Unop(Iop_ReinterpI128asF128,IRExpr_Const(IRConst_U128(0))); break;
    case Ity_V128: zero = IRExpr_Binop(Iop_64HLtoV128, zeroU, zeroU); break;
    case Ity_V256: zero = IRExpr_Qop(Iop_64x4toV256, zeroU, zeroU, zeroU, zeroU); break;
    default: tl_assert(False); return NULL;
  }
  return mkIRExprVec_2(zero,zero);
}

static IRExpr* dg_bar_compare(DiffEnv* diffenv, void* arg1, void* arg2){
  IROp cmp;
  IRType type = typeOfIRExpr(diffenv->sb_out->tyenv,((IRExpr**)arg1)[0]);
  tl_assert(type == typeOfIRExpr(diffenv->sb_out->tyenv,((IRExpr**)arg2)[0]));
  switch(type){
    case Ity_I8: cmp = Iop_CmpEQ8; break;
    case Ity_I16: cmp = Iop_CmpEQ16; break;
    case Ity_I32: cmp = Iop_CmpEQ32; break;
    case Ity_I64: cmp = Iop_CmpEQ64; break;
    default: VG_(printf)("Unhandled type in dg_dot_compare.\n"); tl_assert(False); break;
  }
  IRExpr* cmpLo = IRExpr_Binop(cmp,((IRExpr**)arg1)[0],((IRExpr**)arg2)[0]);
  IRExpr* cmpHi = IRExpr_Binop(cmp,((IRExpr**)arg1)[1],((IRExpr**)arg2)[1]);
  return IRExpr_Binop(Iop_And1,cmpLo,cmpHi);
}

static void* dg_bar_ite(DiffEnv* diffenv, IRExpr* cond, void* dtrue, void* dfalse){
  IRExpr* exLo = IRExpr_ITE(cond,((IRExpr**)dtrue)[0],((IRExpr**)dfalse)[0]);
  IRExpr* exHi = IRExpr_ITE(cond,((IRExpr**)dtrue)[1],((IRExpr**)dfalse)[1]);
  return (void*)mkIRExprVec_2(exLo,exHi);
}

void* dg_bar_operation(DiffEnv* diffenv, IROp op,
                         IRExpr* arg1, IRExpr* arg2, IRExpr* arg3, IRExpr* arg4,
                         void* d1, void* d2, void* d3, void* d4){
  switch(op){
    #include "dg_bar_operations.c"
    default: return NULL;
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
  sm_barLo = initializeShadowMap();
  sm_barHi = initializeShadowMap();
}

void dg_bar_finalize(void){
  destroyShadowMap(sm_barLo);
  destroyShadowMap(sm_barHi);
}

