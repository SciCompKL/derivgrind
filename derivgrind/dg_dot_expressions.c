#include "dg_dot_expressions.h"

#include "pub_tool_basics.h"
#include "pub_tool_libcassert.h"
#include "pub_tool_tooliface.h"
#include "dg_logical.h"
#include "dg_utils.h"

#include "dg_shadow.h"


IRExpr* differentiate_op(IROp op,IRExpr* arg1, IRExpr* arg2, IRExpr* arg3, IRExpr* arg4, DiffEnv* diffenv){
  IRExpr *d1, *d2, *d3, *d4;
  if(arg1) d1 = differentiate_expr(arg1, diffenv);
  if(arg2) d2 = differentiate_expr(arg2, diffenv);
  if(arg3) d3 = differentiate_expr(arg3, diffenv);
  if(arg4) d4 = differentiate_expr(arg4, diffenv);
  switch(op){
    #include "dg_dot_operations.c"
    default: return NULL;
  }
}


extern void* sm_dot;

IRExpr* differentiate_expr(IRExpr const* ex, DiffEnv* diffenv ){
  if(ex == NULL){
    return NULL;
  } else if(ex->tag==Iex_Qop){
    IRQop* rex = ex->Iex.Qop.details;
    return differentiate_op(rex->op,rex->arg1,rex->arg2,rex->arg3,rex->arg4, diffenv);
  } else if(ex->tag==Iex_Triop){
    IRTriop* rex = ex->Iex.Triop.details;
    return differentiate_op(rex->op,rex->arg1,rex->arg2,rex->arg3,NULL,diffenv);
  } else if(ex->tag==Iex_Binop) {
    return differentiate_op(ex->Iex.Binop.op,ex->Iex.Binop.arg1,ex->Iex.Binop.arg2,NULL,NULL,diffenv);
  } else if(ex->tag==Iex_Unop) {
    return differentiate_op(ex->Iex.Unop.op,ex->Iex.Unop.arg,NULL,NULL,NULL,diffenv);
  } else if(ex->tag==Iex_Const) {
    IRConstTag type = ex->Iex.Const.con->tag;
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
  } else if(ex->tag==Iex_ITE) {
    IRExpr* dtrue = differentiate_expr(ex->Iex.ITE.iftrue, diffenv);
    IRExpr* dfalse = differentiate_expr(ex->Iex.ITE.iffalse, diffenv);
    if(dtrue==NULL || dfalse==NULL) return NULL;
    else return IRExpr_ITE(ex->Iex.ITE.cond, dtrue, dfalse);
  } else if(ex->tag==Iex_RdTmp) {
    return IRExpr_RdTmp( ex->Iex.RdTmp.tmp + diffenv->tmp_offset[1] );
  } else if(ex->tag==Iex_Get) {
    return IRExpr_Get(ex->Iex.Get.offset+diffenv->gs_offset[1],ex->Iex.Get.ty);
  } else if(ex->tag==Iex_GetI) {
    IRRegArray* descr = ex->Iex.GetI.descr;
    IRExpr* ix = ex->Iex.GetI.ix;
    Int bias = ex->Iex.GetI.bias;
    IRRegArray* descr_diff = mkIRRegArray(descr->base+diffenv->gs_offset[1],descr->elemTy,descr->nElems);
    return IRExpr_GetI(descr_diff,ix,bias+diffenv->gs_offset[1]);
  } else if (ex->tag==Iex_Load) {
    return loadShadowMemory(sm_dot,diffenv->sb_out,ex->Iex.Load.addr,ex->Iex.Load.ty);
  } else {
    return NULL;
  }
}


IRExpr* differentiate_or_zero(IRExpr* expr, DiffEnv* diffenv, Bool warn, const char* operation){
  IRExpr* diff = differentiate_expr(expr,diffenv);
  if(diff){
    return diff;
  } else {
    if(warn){
      VG_(printf)("Warning: Expression\n");
      ppIRExpr(expr);
      VG_(printf)("\ncould not be differentiated, %s'ing zero instead.\n\n", operation);
    }
    return mkIRConst_zero(typeOfIRExpr(diffenv->sb_out->tyenv,expr));
  }
}
