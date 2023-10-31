/*--------------------------------------------------------------------*/
/*--- Derivgrind expression handling       dg_expressionhandling.c ---*/
/*--- mechanism used in forward and recording mode.                ---*/
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

#include "dg_expressionhandling.h"

/*! \file dg_expressionhandling.c
 * AD-mode independent handling of VEX statements and expressions.
 */

extern Bool warn_about_unwrapped_expressions;

void* dg_modify_expression(DiffEnv* diffenv, ExpressionHandling eh, IRExpr* ex){
  if(ex == NULL){
    return NULL;
  } else if(ex->tag==Iex_Qop){
    IRQop* rex = ex->Iex.Qop.details;
    return eh.operation(diffenv,
      rex->op,rex->arg1,rex->arg2,rex->arg3,rex->arg4,
      dg_modify_expression(diffenv, eh,rex->arg1),
      dg_modify_expression(diffenv, eh,rex->arg2),
      dg_modify_expression(diffenv, eh,rex->arg3),
      dg_modify_expression(diffenv, eh,rex->arg4) );
  } else if(ex->tag==Iex_Triop){
    IRTriop* rex = ex->Iex.Triop.details;
    return eh.operation(diffenv,
      rex->op,rex->arg1,rex->arg2,rex->arg3,(IRExpr*)NULL,
      dg_modify_expression(diffenv, eh,rex->arg1),
      dg_modify_expression(diffenv, eh,rex->arg2),
      dg_modify_expression(diffenv, eh,rex->arg3),
      NULL );
  } else if(ex->tag==Iex_Binop) {
    return eh.operation(diffenv,
      ex->Iex.Binop.op,ex->Iex.Binop.arg1,ex->Iex.Binop.arg2,(IRExpr*)NULL,(IRExpr*)NULL,
      dg_modify_expression(diffenv, eh,ex->Iex.Binop.arg1),
      dg_modify_expression(diffenv, eh,ex->Iex.Binop.arg2),
      NULL, NULL);
  } else if(ex->tag==Iex_Unop) {
    return eh.operation(diffenv,
      ex->Iex.Unop.op,ex->Iex.Unop.arg,(IRExpr*)NULL,(IRExpr*)NULL,(IRExpr*)NULL,
      dg_modify_expression(diffenv, eh,ex->Iex.Unop.arg),
      NULL, NULL, NULL);
  } else if(ex->tag==Iex_Const) {
    return eh.constant(diffenv,ex->Iex.Const.con->tag);
  } else if(ex->tag==Iex_ITE) {
    void* dtrue = dg_modify_expression(diffenv, eh, ex->Iex.ITE.iftrue);
    void* dfalse = dg_modify_expression(diffenv, eh, ex->Iex.ITE.iffalse);
    if(dtrue==NULL || dfalse==NULL) return NULL;
    else return eh.ite(diffenv,ex->Iex.ITE.cond,dtrue,dfalse);
  } else if(ex->tag==Iex_RdTmp) {
    return eh.rdtmp(diffenv,ex->Iex.RdTmp.tmp);
  } else if(ex->tag==Iex_Get) {
    return eh.geti(diffenv,ex->Iex.Get.offset,ex->Iex.Get.ty,(IRRegArray*)NULL,(IRExpr*)NULL);
  } else if(ex->tag==Iex_GetI) {
    return eh.geti(diffenv,ex->Iex.GetI.bias,Ity_INVALID,ex->Iex.GetI.descr,ex->Iex.GetI.ix);
  } else if (ex->tag==Iex_Load) {
    return eh.load(diffenv,ex->Iex.Load.addr,ex->Iex.Load.ty);
  } else if (ex->tag==Iex_CCall) {
    IRExpr** args = ex->Iex.CCall.args;
    int nargs = 0;
    while(args[nargs]!=NULL) nargs++;
    void** modified_args = LibVEX_Alloc(nargs*sizeof(void*));
    for(int i=0; i<nargs; i++){
      modified_args[i] = dg_modify_expression(diffenv,eh,args[i]);
    }
    return eh.ccall(diffenv,ex->Iex.CCall.cee,ex->Iex.CCall.retty,args,modified_args);
  } else {
    return NULL;
  }
}


void* dg_modify_expression_or_default(DiffEnv* diffenv, ExpressionHandling eh, IRExpr* expr, Bool warn, const char* operation){
  void* diff = dg_modify_expression(diffenv, eh, expr);
  if(diff){
    return diff;
  } else {
    if(warn){
      VG_(printf)("Warning: Expression\n");
      ppIRExpr(expr);
      VG_(printf)("\ncould not be modified, %s'ing zero instead.\n\n", operation);
    }
    return eh.default_(diffenv,typeOfIRExpr(diffenv->sb_out->tyenv,expr));
  }
}



void add_statement_modified(DiffEnv* diffenv, ExpressionHandling eh, IRStmt* st_orig){
  const IRStmt* st = st_orig;
  if(st->tag==Ist_WrTmp) {
    void* modified_expr = dg_modify_expression_or_default(diffenv,eh,st->Ist.WrTmp.data,warn_about_unwrapped_expressions,"WrTmp");
    eh.wrtmp(diffenv,st->Ist.WrTmp.tmp,modified_expr);
  } else if(st->tag==Ist_Put) {
    void* modified_expr = dg_modify_expression_or_default(diffenv,eh,st->Ist.Put.data,warn_about_unwrapped_expressions,"Put");
    eh.puti(diffenv,st->Ist.Put.offset,modified_expr,(IRRegArray*)NULL,(IRExpr*)NULL);
  } else if(st->tag==Ist_PutI) {
    void* modified_expr = dg_modify_expression_or_default(diffenv,eh,st->Ist.PutI.details->data,warn_about_unwrapped_expressions,"PutI");
    eh.puti(diffenv,st->Ist.PutI.details->bias,modified_expr,st->Ist.PutI.details->descr,st->Ist.PutI.details->ix);
  } else if(st->tag==Ist_Store){
    void* modified_expr = dg_modify_expression_or_default(diffenv,eh,st->Ist.Store.data, warn_about_unwrapped_expressions,"Store");
    eh.store(diffenv,st->Ist.Store.addr,modified_expr,(IRExpr*)NULL);
  } else if(st->tag==Ist_StoreG){
    IRStoreG* det = st->Ist.StoreG.details;
    IRExpr* modified_expr = dg_modify_expression_or_default(diffenv,eh,det->data, warn_about_unwrapped_expressions,"StoreG");
    eh.store(diffenv,det->addr,modified_expr,det->guard);
  } else if(st->tag==Ist_LoadG) {
    IRLoadG* det = st->Ist.LoadG.details;
    // discard det->cvt; extra bits for widening should
    // never be interpreted as derivative information
    IRType type = diffenv->sb_out->tyenv->types[det->dst];
    // load differentiated value from memory
    void* modified_data_read = eh.load(diffenv,det->addr,type);
    // differentiate alternative value
    void* modified_expr_alt = dg_modify_expression_or_default(diffenv,eh,det->alt,warn_about_unwrapped_expressions,"alternative-LoadG");
    // depending on the guard, copy either the derivative stored
    // in shadow memory, or the derivative of the alternative value,
    // to the shadow temporary.
    eh.wrtmp(diffenv,det->dst, eh.ite(diffenv,det->guard, modified_data_read, modified_expr_alt));
  } else if(st->tag==Ist_CAS) {

    IRCAS* det = st->Ist.CAS.details;
    IRType type = typeOfIRExpr(diffenv->sb_out->tyenv,det->expdLo);
    Bool double_element = (det->expdHi!=NULL);

    // As we add some instrumentation now, note that the complete
    // translation of the Ist_CAS is not atomic any more, so it's
    // possible that we create a race condition here.
    // This issue also exists in do_shadow_CAS in mc_translate.c.
    // There, the comment states that because Valgrind runs only one
    // thread at a time and there are no context switches within a
    // single IRSB, this is not a problem.

    // Find addresses of Hi and Lo part.
    IRExpr* addr_Lo;
    IRExpr* addr_Hi;
    addressesOfCAS(det,diffenv->sb_out,&addr_Lo,&addr_Hi);

    // Find out if CAS succeeded.
    IROp cmp;
    switch(type){
      case Ity_I8: cmp = Iop_CmpEQ8; break;
      case Ity_I16: cmp = Iop_CmpEQ16; break;
      case Ity_I32: cmp = Iop_CmpEQ32; break;
      case Ity_I64: cmp = Iop_CmpEQ64; break;
      default: VG_(printf)("Unhandled type in translation of Ist_CAS.\n"); tl_assert(False); break;
    }
    // Check whether expected low values and shadow values agree.
    // We assume that the shadow expression can always be formed,
    // otherways the CAS will never succeed with the current implementation.
    IRExpr* equal_values_Lo = IRExpr_Binop(cmp,det->expdLo,IRExpr_Load(det->end,type,addr_Lo));
    void* modified_expdLo = dg_modify_expression_or_default(diffenv,eh,det->expdLo,False,"");
    IRExpr* equal_modifiedvalues_Lo = eh.compare(diffenv,modified_expdLo,eh.load(diffenv,addr_Lo,type));
    IRExpr* equal_Lo = IRExpr_Binop(Iop_And1,equal_values_Lo,equal_modifiedvalues_Lo);
    IRExpr* equal_Hi = IRExpr_Const(IRConst_U1(1));
    if(double_element){
      IRExpr* equal_values_Hi = IRExpr_Binop(cmp,det->expdHi,IRExpr_Load(det->end,type,addr_Hi));
      void* modified_expdHi = dg_modify_expression_or_default(diffenv,eh,det->expdHi,False,"");
      IRExpr* equal_modifiedvalues_Hi = eh.compare(diffenv,modified_expdHi,eh.load(diffenv,addr_Hi,type));
      equal_Hi = IRExpr_Binop(Iop_And1,equal_values_Hi,equal_modifiedvalues_Hi);
    }
    diffenv->cas_succeeded = newIRTemp(diffenv->sb_out->tyenv, Ity_I1);
    addStmtToIRSB(diffenv->sb_out, IRStmt_WrTmp(diffenv->cas_succeeded,
      IRExpr_Binop(Iop_And1, equal_Lo, equal_Hi)
    ));

    // Set shadows of oldLo and possibly oldHi.
    eh.wrtmp(diffenv,det->oldLo,eh.load(diffenv,addr_Lo,type));
    if(double_element){
        eh.wrtmp(diffenv,det->oldHi,eh.load(diffenv,addr_Hi,type));
    }
    // Guarded write of Lo part to shadow memory.
    IRExpr* modified_dataLo = dg_modify_expression_or_default(diffenv,eh,det->dataLo,False,"");
    eh.store(diffenv,addr_Lo,modified_dataLo,IRExpr_RdTmp(diffenv->cas_succeeded));
    // Possibly guarded write of Hi part to shadow memory.
    if(double_element){
      IRExpr* modified_dataHi = dg_modify_expression_or_default(diffenv,eh,det->dataHi,False,"");
      eh.store(diffenv,addr_Hi,modified_dataHi,IRExpr_RdTmp(diffenv->cas_succeeded));
    }
  } else if(st->tag==Ist_LLSC) {
    VG_(printf)("Did not instrument Ist_LLSC statement.\n");
  } else if(st->tag==Ist_Dirty) {
    // We should have a look at all Ist_Dirty statements that
    // are added to the VEX IR in guest_x86_to_IR.c. Maybe
    // some of them need AD treatment.

    IRDirty* det = st->Ist.Dirty.details;
    const HChar* name = det->cee->name;

    // The x86g_dirtyhelper_storeF80le dirty call converts a 64-bit
    // floating-point register to a 80-bit x87 extended double and
    // stores it in 10 bytes of guest memory.
    // We have to convert the 64-bit derivative information to 80 bit
    // and store them in 10 bytes of shadow memory.
    // The same applies on amd64.
    if(!VG_(strcmp)(name, "x86g_dirtyhelper_storeF80le") ||
       !VG_(strcmp)(name, "amd64g_dirtyhelper_storeF80le") ){
      IRExpr** args = det->args;
      IRExpr* addr = args[0];
      IRExpr* expr = args[1];
      IRExpr* modified_expr = dg_modify_expression_or_default(diffenv,eh,expr,False,"");
      eh.dirty_storeF80le(diffenv,addr, modified_expr);
    }
    // The x86g_dirtyhelper_loadF80le dirty call loads 80 bit from
    // memory, converts them to a 64 bit double and stores it in a
    // Ity_I64 temporary. We have to do the same with the derivative
    // information in the shadow memory.
    // The temporary will later be reinterpreted as float and likely
    // stored in a register, but the AD logic for this part is
    // as usual.
    // The same applies on amd64.
    else if(!VG_(strcmp)(name, "x86g_dirtyhelper_loadF80le") ||
            !VG_(strcmp)(name, "amd64g_dirtyhelper_loadF80le") ){
      IRExpr** args = det->args;
      IRExpr* addr = args[0];
      eh.dirty_loadF80le(diffenv,addr,det->tmp);
    }
    /*! \page dirty_calls_no_ad Dirty calls without relevance for AD
     *  The following dirty calls do not handle AD-active bytes,
     *  therefore no specific AD instrumentation is necessary. If
     *  there is an output temporary, we set the shadow temporary to
     *  zero in case it is further copied around.
     * - The CPUID dirty calls set some registers in the guest state.
     *   As these should never end up as floating-point data, we don't
     *   need to do anything about AD.
     * - The RDTSC instruction loads a 64-bit time-stamp counter into
     *   the (lower 32 bit of the) guest registers EAX and EDX (and
     *   clears the higher 32 bit on amd64). The dirty call just
     *   stores an Ity_I64 in its return temporary. We put a zero in
     *   the shadow temporary.
     * - The XRSTOR_COMPONENT_1_EXCLUDING_XMMREGS, XSAVE_.. dirty calls
     *   (re)store a SSE state, this seems to be a completely discrete thing.
     * - The PCMPxSTRx dirty calls account for SSE 4.2 string instructions,
     *   also a purely discrete thing.
     * - amd64g_dirtyhelper_FSTENV and amd64g_dirtyhelper_FLDENV save status,
     *   pointers and the like, but not the content of the x87 registers.
     *
     * For other dirty calls, a warning is emitted.
     */
    else {
      if(det->tmp!=IRTemp_INVALID){
        IRType type = typeOfIRTemp(diffenv->sb_out->tyenv,det->tmp);
        eh.wrtmp(diffenv,det->tmp,eh.default_(diffenv,type));
      }

      // warn if dirty call is unknown
      if(VG_(strncmp(name, "x86g_dirtyhelper_CPUID_",23)) &&
         VG_(strncmp(name, "amd64g_dirtyhelper_CPUID_",25)) &&
         VG_(strcmp(name, "amd64g_dirtyhelper_XRSTOR_COMPONENT_1_EXCLUDING_XMMREGS")) &&
         VG_(strcmp(name, "amd64g_dirtyhelper_XSAVE_COMPONENT_1_EXCLUDING_XMMREGS")) &&
         VG_(strcmp)(name,"x86g_dirtyhelper_RDTSC") &&
         VG_(strcmp)(name,"amd64g_dirtyhelper_RDTSC") &&
         VG_(strcmp)(name,"amd64g_dirtyhelper_PCMPxSTRx") &&
         VG_(strcmp)(name,"amd64g_dirtyhelper_FSTENV") &&
         VG_(strcmp)(name,"amd64g_dirtyhelper_FLDENV")
      ){
        VG_(printf)("Cannot instrument Ist_Dirty statement:\n");
        ppIRStmt(st);
        VG_(printf)("\n");
      }
    }
  } else if(st->tag==Ist_NoOp || st->tag==Ist_IMark || st->tag==Ist_AbiHint){
    // no relevance for any tool, do nothing
  } else if(st->tag==Ist_Exit || st->tag==Ist_MBE) {
    // no relevance for AD, do nothing
  } else {
    tl_assert(False);
  }
}

/* --- Helper functions. ---*/

