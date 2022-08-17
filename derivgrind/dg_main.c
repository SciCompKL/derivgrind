/*--------------------------------------------------------------------*/
/*--- DerivGrind: Forward-mode algorithmic               dg_main.c ---*/
/*--- differentiation using Valgrind.                              ---*/
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
#include "pub_tool_mallocfree.h"
#include "pub_tool_libcassert.h"
#include "pub_tool_gdbserver.h"
#include "pub_tool_libcbase.h"
#include "pub_tool_options.h"
#include "valgrind.h"
#include "derivgrind.h"

#include "dg_logical.h"
#include "dg_utils.h"
#include "dg_paragrind.h"

#include "dg_shadow.h"
#include "dg_dot_expressions.h"

/*! \page storage_convention Storage convention for shadow memory
 *
 *  To each variable x, we assign a shadow variable xd of the same size
 *  and type, with the following purpose:
 *
 *  If x is a floating-point variable (float or double), xd stores the
 *  gradient of x. If x is of another type and arose from bit-wise copying
 *  of (parts of) a floating-point variable y, xd stores a copy of the
 *  respective parts of yd. Otherwise, the value of xd is unspecified.
 *
 *  In VEX IR, there are three locations where variables can exist:
 *  - For temporary variables t_i, the shadow variables are shifted
 *    temporary variables t_(i+d1) for some offset d1.
 *  - For registers r, the shadow variables are shifted registers r+d2
 *    for some offset d2.
 *  - For memory addresses p, the shadow variables are stored by the
 *    help of a *shadow memory* library. This library allows to store
 *    another byte for each byte of "original" memory.
 */

//! Shadow memory for the dot values.
void* sm_dot = NULL;

//! Can be used to tag dg_add_print_stmt outputs.
static unsigned long stmt_counter = 0;

//! Debugging output.
static Bool warn_about_unwrapped_expressions = False;

/*! If True, instead of differentiated expressions, the original expressions
 *  are evaluated.
 */
Bool paragrind = False;

static void dg_post_clo_init(void)
{
  if(paragrind){
    dg_paragrind_pre_clo_init();
  }
}

static Bool dg_process_cmd_line_option(const HChar* arg)
{
   if VG_BOOL_CLO(arg, "--warn-unwrapped", warn_about_unwrapped_expressions) {}
   else if VG_BOOL_CLO(arg, "--paragrind", paragrind) {}
   else
      return False;
   return True;
}

static void dg_print_usage(void)
{
   VG_(printf)(
"    --warn-unwrapped=no|yes   warn about unwrapped expressions\n"
   );
}

static void dg_print_debug_usage(void)
{
   VG_(printf)(
"    (none)\n"
   );
}

#include <VEX/priv/guest_generic_x87.h>
UChar monitor_command_mode = 'd'; //! Specifies shadow map accessed by monitor commands: d=dot, p=paragrind
/*! React to gdb monitor commands.
 */
static
Bool dg_handle_gdb_monitor_command(ThreadId tid, HChar* req){
  HChar s[VG_(strlen)(req)+1]; //!< copy of req for strtok_r
  VG_(strcpy)(s, req);
  HChar* ssaveptr; //!< internal state of strtok_r

  const HChar commands[] = "help get set fget fset lget lset mode"; //!< list of possible commands
  HChar* wcmd = VG_(strtok_r)(s, " ", &ssaveptr); //!< User command
  int key = VG_(keyword_id)(commands, wcmd, kwd_report_duplicated_matches);
  switch(key){
    case -2: // multiple matches
      return True;
    case -1: // not found
      return False;
    case 0: // help
      VG_(gdb_printf)(
        "monitor commands:\n"
        "  mode <mode>       - Select which shadow map to access:\n"
        "                      dot (mode=d) or parallel (mode=p)\n"
        "  get  <addr>       - Prints shadow of binary64 (e.g. C double)\n"
        "  set  <addr> <val> - Sets shadow of binary64 (e.g. C double)\n"
        "  fget <addr>       - Prints shadow of binary32 (e.g. C float)\n"
        "  fset <addr> <val> - Sets shadow of binary32 (e.g. C float)\n"
        "  lget <addr>       - Prints shadow of x87 double extended\n"
        "  lset <addr> <val> - Sets shadow of x87 double extended\n"
      );
      return True;
    case 1: case 3: case 5: { // get, fget, lget
      HChar* address_str = VG_(strtok_r)(NULL, " ", &ssaveptr);
      HChar const* address_str_const = address_str;
      Addr address;
      if(!VG_(parse_Addr)(&address_str_const, &address)){
        VG_(gdb_printf)("Usage: get  <addr>\n"
                        "       fget <addr>\n"
                        "       lget <addr>\n");
        return False;
      }
      int size;
      switch(key){
        case 1: size = 8; break;
        case 3: size = 4; break;
        case 5: size = 10; break;
      }
      union {unsigned char l[10]; double d; float f;} shadow, init;
      if(monitor_command_mode=='d'){
        shadowGet(sm_dot,(void*)address, (void*)&shadow, size);
        VG_(gdb_printf)("dot value: ");
      } else if(monitor_command_mode=='p'){
        shadowGet(sm_pginit,(void*)address, (void*)&init, size);
        Bool show_original_value = False;
        for(int i=0; i<size; i++){
          if(init.l[i]!=0xff)
            show_original_value = True;
        }
        if(show_original_value){
          for(int i=0; i<size; i++)
            shadow.l[i] = ((unsigned char*)address)[i];
          VG_(gdb_printf)("original value: ");
        } else {
          shadowGet(sm_pgdata,(void*)address, (void*)&shadow, size);
          VG_(gdb_printf)("parallel value: ");
        }
      }
      switch(key){
        case 1:
          VG_(gdb_printf)("%.16lf\n", shadow.d);
          break;
        case 3:
          VG_(gdb_printf)("%.9f\n", shadow.f);
          break;
        case 5: {
          // convert x87 double extended to 64-bit double
          // so we can use the ordinary I/O.
          double tmp;
          convert_f80le_to_f64le(shadow.l,(unsigned char*)&tmp);
          VG_(gdb_printf)("%.16lf\n", (double)tmp);
          break;
        }
      }
      return True;
    }
    case 2: case 4: case 6: { // set, fset, lset
      HChar* address_str = VG_(strtok_r)(NULL, " ", &ssaveptr);
      HChar const* address_str_const = address_str;
      Addr address;
      if(!VG_(parse_Addr)(&address_str_const, &address)){
        VG_(gdb_printf)("Usage: set  <addr> <shadow value>\n"
                        "       fset <addr> <shadow value>\n"
                        "       lset <addr> <shadow value>\n");
        return False;
      }
      HChar* derivative_str = VG_(strtok_r)(NULL, " ", &ssaveptr);
      union {unsigned char l[10]; double d; float f;} shadow;
      shadow.d = VG_(strtod)(derivative_str, NULL);
      int size;
      switch(key){
        case 2: size = 8; break;
        case 4: size = 4; shadow.f = (float) shadow.d; break;
        case 6: {
          // read as ordinary double and convert to x87 double extended
          // so we can use the ordinary I/O
          size = 10;
          double tmp = shadow.d;
          convert_f64le_to_f80le((unsigned char*)&tmp,shadow.l);
          break;
        }
      }
      if(monitor_command_mode=='d'){
        shadowSet(sm_dot,(void*)address,(void*)&shadow,size);
      } else if(monitor_command_mode=='p'){
        shadowSet(sm_pgdata,(void*)address,(void*)&shadow,size);
        unsigned char ones[10] = {0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff};
        shadowSet(sm_pginit,(void*)address,(void*)&ones,size);
      }
      return True;
    }
    case 7: { // mode
      HChar* mode_str = VG_(strtok_r)(NULL, " ", &ssaveptr);
      if(mode_str[0]=='d') monitor_command_mode='d';
      else if(mode_str[0]=='p'){
        if(paragrind){
          monitor_command_mode='p';
        } else  {
          VG_(gdb_printf)("Run DerivGrind with --paragrind=yes to use ParaGrind.\n");
        }
      }
      else {
        VG_(gdb_printf)("Usage: mode <mode>\n"
                        "where <mode> is 'd' or 'p'.\n");
      }
      return True;
    }
    default:
      VG_(printf)("Error in dg_handle_gdb_monitor_command.\n");
      return False;
    }

}

/*! React to client requests like gdb monitor commands.
 */
static
Bool dg_handle_client_request(ThreadId tid, UWord* arg, UWord* ret){
  if(arg[0]==VG_USERREQ__GDB_MONITOR_COMMAND){
    Bool handled = dg_handle_gdb_monitor_command(tid, (HChar*)arg[1]);
    if(handled){
      *ret = 1;
    } else {
      *ret = 0;
    }
    return handled;
  } else if(arg[0]==VG_USERREQ__GET_DERIVATIVE) {
    void* addr = (void*) arg[1];
    void* daddr = (void*) arg[2];
    UWord size = arg[3];
    shadowGet(sm_dot,(void*)addr,(void*)daddr,size);
    *ret = 1;
    return True;
  } else if(arg[0]==VG_USERREQ__SET_DERIVATIVE) {
    void* addr = (void*) arg[1];
    void* daddr = (void*) arg[2];
    UWord size = arg[3];
    shadowSet(sm_dot,addr,daddr,size);
    *ret = 1;
    return True;
  } else {
    VG_(printf)("Unhandled user request.\n");
    return True;
  }
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
ULong x86g_amd64g_dirtyhelper_loadF80le_dot ( Addr addrU )
{
   ULong f64, f128[2];
   shadowGet(sm_dot,(void*)addrU, (void*)f128, 10);
   convert_f80le_to_f64le ( (UChar*)f128, (UChar*)&f64 );
   return f64;
}
ULong x86g_amd64g_dirtyhelper_loadF80le_para ( Addr addrU )
{
  // masking after reading from memory
  ULong f64, f128_data[2], f128_init[2];
  shadowGet(sm_pgdata,(void*)addrU, (void*)f128_data, 10);
  shadowGet(sm_pginit,(void*)addrU, (void*)f128_init, 10);
  for(int i=0; i<2; i++){
    ULong f128_orig = ((ULong*)addrU)[i];
    f128_data[i] = (f128_init[i] & f128_data[i]) | (~f128_init[i] & f128_orig);
  }
  convert_f80le_to_f64le ( (UChar*)f128_data, (UChar*)&f64 );
  return f64;
}
/*! Dirtyhelper for the extra AD logic to dirty calls to
 *  x86g_dirtyhelper_storeF80le / amd64g_dirtyhelper_storeF80le.
 *
 *  It's very similar, but writes to shadow memory instead
 *  of guest memory.
 */
void x86g_amd64g_dirtyhelper_storeF80le_dot ( Addr addrU, ULong f64 )
{
   ULong f128[2];
   convert_f64le_to_f80le( (UChar*)&f64, (UChar*)f128 );
   shadowSet(sm_dot,(void*)addrU,(void*)f128,10);
}
void x86g_amd64g_dirtyhelper_storeF80le_para ( Addr addrU, ULong f64 )
{
   ULong f128[2];
   ULong ones[2] = {0xffffffffffffffff,0xffffffffffffffff};
   convert_f64le_to_f80le( (UChar*)&f64, (UChar*)f128 );
   shadowSet(sm_pgdata,(void*)addrU,(void*)f128,10);
   shadowSet(sm_pginit,(void*)addrU,(void*)ones,10);
}

/*! Helper to extract high/low addresses of CAS statement.
 *
 *  One of addr_Lo, addr_Hi is det->addr,
 *  the other one is det->addr + offset.
 *  \param[in] det - CAS statement details.
 *  \param[in] diffenv - Differentiation environment.
 *  \param[out] addr_Lo - Low address.
 *  \param[out] addr_Hi - High address.
 */
static void addressesOfCAS(IRCAS const* det, IRSB* sb_out, IRExpr** addr_Lo, IRExpr** addr_Hi){
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


/*! Add what the original statement did, to output IRSB.
 *
 *  CAS needs special treatment: If success has already been
 *  tested in the DerivGrind instrumentation, use the result
 *  of this test.
 *  \param[in,out] sb_out - Output IRSB.
 *  \param[in] st_orig - Original statement.
 *  \param[in] diffenv - Additional data.
 */
static void add_statement_original(IRSB* sb_out, IRStmt* st_orig, DiffEnv* diffenv){
  const IRStmt* st = st_orig;
  if(st->tag==Ist_CAS && diffenv->cas_succeeded != IRTemp_INVALID){
    IRCAS* det = st->Ist.CAS.details;
    IRType type = typeOfIRExpr(sb_out->tyenv,det->expdLo);
    Bool double_element = (det->expdHi!=NULL);
    IRExpr* addr_Lo;
    IRExpr* addr_Hi;
    addressesOfCAS(det,sb_out,&addr_Lo,&addr_Hi);
    // Set oldLo and possibly oldHi.
    addStmtToIRSB(sb_out, IRStmt_WrTmp(det->oldLo,IRExpr_Load(det->end,type,addr_Lo)));
    if(double_element){
      addStmtToIRSB(sb_out, IRStmt_WrTmp(det->oldHi,IRExpr_Load(det->end,type,addr_Hi)));
    }
    // Guarded write of Lo part, and possibly Hi part.
    // As Ist_StoreG causes an isel error on x86, we use an if-then-else construct.
    IRExpr* store_Lo = IRExpr_ITE(IRExpr_RdTmp(diffenv->cas_succeeded),
      det->dataLo, IRExpr_Load(det->end,type,addr_Lo));
    addStmtToIRSB(sb_out, IRStmt_Store(det->end,addr_Lo,store_Lo));
    if(double_element){
      IRExpr* store_Hi = IRExpr_ITE(IRExpr_RdTmp(diffenv->cas_succeeded),
        det->dataHi, IRExpr_Load(det->end,type,addr_Hi));
      addStmtToIRSB(sb_out, IRStmt_Store(det->end,addr_Hi,store_Hi));
    }
  } else { // for all other IRStmt's, just copy them
    addStmtToIRSB(sb_out, st_orig);
  }
}


/*! Tuple of functions defining how to instrument a statement.
 */
typedef struct {
  /*! How to modify expressions in the statement. */
  IRExpr* (*modify_expression)(IRExpr*, DiffEnv*, Bool, const char*);
  /*! How to load data from memory. */
  IRExpr* (*load_expression)(IRSB*, IRExpr*, IRType);
  /*! How to store data in memory. */
  void (*store_expression)(IRSB*, IRExpr*, IRExpr*, IRExpr*);
  /*! Name of dirty call loadF80le. */
  const char* dirtyhelper_loadF80le_str;
  /*! Function for dirty call loadF80le. */
  ULong (*dirtyhelper_loadF80le_fun)(Addr);
  /*! Name of dirty call storeF80le. */
  const char* dirtyhelper_storeF80le_str;
  /*! Function for dirty call storeF80le. */
  void (*dirtyhelper_storeF80le_fun)(Addr, ULong);

} ExpressionHandling;

/*! Add statement with modified expressions to output IRSB.
 *  \param[in,out] sb_out - Output IRSB.
 *  \param[in] st_orig - Original statement.
 *  \param[in] diffenv - Additional data.
 *  \param[in] inst - Used to compute shifts of temporary indices.
 *  \param[in] modify_expression - Function that modifies expressions.
 *    and guest state offsets.
 *  \param[in] load_expression - Function to load from memory.
 *  \param[in] store_expression - Function to store in memory.
 */
static void add_statement_modified(IRSB* sb_out, IRStmt* st_orig, DiffEnv* diffenv,
    int inst, ExpressionHandling eh){
  const IRStmt* st = st_orig;
  if(st->tag==Ist_WrTmp) {
    IRType type = sb_out->tyenv->types[st->Ist.WrTmp.tmp];
    IRExpr* modified_expr = eh.modify_expression(st->Ist.WrTmp.data, diffenv,warn_about_unwrapped_expressions,"WrTmp");
    IRStmt* sp = IRStmt_WrTmp(st->Ist.WrTmp.tmp+diffenv->tmp_offset[inst], modified_expr);
    addStmtToIRSB(sb_out, sp);
  } else if(st->tag==Ist_Put) {
    IRType type = typeOfIRExpr(sb_out->tyenv,st->Ist.Put.data);
    IRExpr* modified_expr = eh.modify_expression(st->Ist.Put.data, diffenv,warn_about_unwrapped_expressions,"Put");
    IRStmt* sp = IRStmt_Put(st->Ist.Put.offset + diffenv->gs_offset[inst], modified_expr);
    addStmtToIRSB(sb_out, sp);
  } else if(st->tag==Ist_PutI) {
    IRType type = typeOfIRExpr(sb_out->tyenv,st->Ist.PutI.details->data);
    IRExpr* modified_expr = eh.modify_expression(st->Ist.PutI.details->data, diffenv,warn_about_unwrapped_expressions,"PutI");
    IRRegArray* descr = st->Ist.PutI.details->descr;
    IRRegArray* descr_diff = mkIRRegArray(descr->base+diffenv->gs_offset[inst], descr->elemTy, descr->nElems);
    Int bias = st->Ist.PutI.details->bias;
    IRExpr* ix = st->Ist.PutI.details->ix;
    IRStmt* sp = IRStmt_PutI(mkIRPutI(descr_diff,ix,bias+diffenv->gs_offset[inst],modified_expr));
    addStmtToIRSB(sb_out, sp);
  } else if(st->tag==Ist_Store){
    IRType type = typeOfIRExpr(sb_out->tyenv,st->Ist.Store.data);
    IRExpr* modified_expr = eh.modify_expression(st->Ist.Store.data, diffenv, warn_about_unwrapped_expressions,"Store");
    eh.store_expression(diffenv->sb_out,st->Ist.Store.addr,modified_expr,NULL);
  } else if(st->tag==Ist_StoreG){
    IRStoreG* det = st->Ist.StoreG.details;
    IRType type = typeOfIRExpr(sb_out->tyenv,det->data);
    IRExpr* modified_expr = eh.modify_expression(det->data, diffenv, warn_about_unwrapped_expressions,"StoreG");
    eh.store_expression(diffenv->sb_out,det->addr,modified_expr,det->guard);
  } else if(st->tag==Ist_LoadG) {
    IRLoadG* det = st->Ist.LoadG.details;
    // discard det->cvt; extra bits for widening should
    // never be interpreted as derivative information
    IRType type = sb_out->tyenv->types[det->dst];
    // differentiate alternative value
    IRExpr* modified_expr_alt = eh.modify_expression(det->alt,diffenv,warn_about_unwrapped_expressions,"alternative-LoadG");
    // depending on the guard, copy either the derivative stored
    // in shadow memory, or the derivative of the alternative value,
    // to the shadow temporary.
    addStmtToIRSB(diffenv->sb_out, IRStmt_WrTmp(det->dst+diffenv->tmp_offset[inst],
      IRExpr_ITE(det->guard,eh.load_expression(diffenv->sb_out,det->addr,type),modified_expr_alt)
    ));
  } else if(st->tag==Ist_CAS) { // TODO

    IRCAS* det = st->Ist.CAS.details;
    IRType type = typeOfIRExpr(sb_out->tyenv,det->expdLo);
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
    addressesOfCAS(det,sb_out,&addr_Lo,&addr_Hi);

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
    IRExpr* modified_expdLo = eh.modify_expression(det->expdLo,diffenv,False,"");
    IRExpr* equal_modifiedvalues_Lo = IRExpr_Binop(cmp,modified_expdLo,eh.load_expression(sb_out,addr_Lo,type));
    IRExpr* equal_Lo = IRExpr_Binop(Iop_And1,equal_values_Lo,equal_modifiedvalues_Lo);
    IRExpr* equal_Hi = IRExpr_Const(IRConst_U1(1));
    if(double_element){
      IRExpr* equal_values_Hi = IRExpr_Binop(cmp,det->expdHi,IRExpr_Load(det->end,type,addr_Hi));
      IRExpr* modified_expdHi = eh.modify_expression(det->expdHi,diffenv,False,"");
      IRExpr* equal_modifiedvalues_Hi = IRExpr_Binop(cmp,modified_expdHi,eh.load_expression(sb_out,addr_Hi,type));
      equal_Hi = IRExpr_Binop(Iop_And1,equal_values_Hi,equal_modifiedvalues_Hi);
    }
    diffenv->cas_succeeded = newIRTemp(sb_out->tyenv, Ity_I1);
    addStmtToIRSB(sb_out, IRStmt_WrTmp(diffenv->cas_succeeded,
      IRExpr_Binop(Iop_And1, equal_Lo, equal_Hi)
    ));

    // Set shadows of oldLo and possibly oldHi.
    addStmtToIRSB(sb_out, IRStmt_WrTmp(det->oldLo+diffenv->tmp_offset[inst],
      eh.load_expression(sb_out,addr_Lo,type)));
    if(double_element){
      addStmtToIRSB(sb_out, IRStmt_WrTmp(det->oldHi+diffenv->tmp_offset[inst],
        eh.load_expression(sb_out,addr_Hi,type)));
    }
    // Guarded write of Lo part to shadow memory.
    IRExpr* modified_dataLo = eh.modify_expression(det->dataLo,diffenv,False,"");
    eh.store_expression(sb_out,addr_Lo,modified_dataLo,IRExpr_RdTmp(diffenv->cas_succeeded));
    // Possibly guarded write of Hi part to shadow memory.
    if(double_element){
      IRExpr* modified_dataHi =  eh.modify_expression(det->dataHi,diffenv,False,"");
      eh.store_expression(sb_out,addr_Hi,modified_dataHi,IRExpr_RdTmp(diffenv->cas_succeeded));
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
      IRExpr* modified_expr = eh.modify_expression(expr,diffenv,False,"");
      IRDirty* dd = unsafeIRDirty_0_N(
            0,
            eh.dirtyhelper_storeF80le_str,
            eh.dirtyhelper_storeF80le_fun,
            mkIRExprVec_2(addr, modified_expr) );
      addStmtToIRSB(sb_out, IRStmt_Dirty(dd));
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
      IRTemp t = det->tmp;
      IRDirty* dd = unsafeIRDirty_1_N(
            t + diffenv->tmp_offset[inst],
            0,
            eh.dirtyhelper_loadF80le_str,
            eh.dirtyhelper_loadF80le_fun,
            mkIRExprVec_1(addr));
      addStmtToIRSB(sb_out, IRStmt_Dirty(dd));
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
        IRTemp shadow_tmp = det->tmp+diffenv->tmp_offset[inst];
        IRType type = typeOfIRTemp(diffenv->sb_out->tyenv,det->tmp);
        IRExpr* zero = mkIRConst_zero(type);
        addStmtToIRSB(sb_out,IRStmt_WrTmp(shadow_tmp,zero));
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

void store_expression_dot(IRSB* sb_out, IRExpr* addr, IRExpr* expr, IRExpr* guard){
  storeShadowMemory(sm_dot,sb_out,addr,expr,guard);
}
IRExpr* load_expression_dot(IRSB* sb_out, IRExpr* addr, IRType type){
  return loadShadowMemory(sm_dot,sb_out,addr,type);
}
/*! Add forward-mode instrumentation to output IRSB.
 *  \param[in,out] sb_out - Output IRSB.
 *  \param[in] st_orig - Original statement.
 *  \param[in] diffenv - Additional data.
 */
static void add_statement_forward(IRSB* sb_out, IRStmt* st_orig, DiffEnv* diffenv){
  ExpressionHandling eh_dot = {
    &differentiate_or_zero, &load_expression_dot, &store_expression_dot,
    "x86g_amd64g_dirtyhelper_loadF80le_dot", &x86g_amd64g_dirtyhelper_loadF80le_dot,
    "x86g_amd64g_dirtyhelper_storeF80le_dot", &x86g_amd64g_dirtyhelper_storeF80le_dot
  };
  add_statement_modified(sb_out, st_orig, diffenv, 1, eh_dot);
}

void store_expression_para(IRSB* sb_out, IRExpr* addr, IRExpr* expr, IRExpr* guard){
  storeLayeredShadowMemory(sm_pgdata,sm_pginit,sb_out,addr,expr,guard);
}
IRExpr* load_expression_para(IRSB* sb_out, IRExpr* addr, IRType type){
  return loadLayeredShadowMemory(sm_pgdata,sm_pginit,sb_out,addr,type);
}
/*! Add paragrind instrumentation to output IRSB.
 *  \param[in,out] sb_out - Output IRSB.
 *  \param[in] st_orig - Original statement.
 *  \param[in] diffenv - Additional data.
 */
static void add_statement_paragrind(IRSB* sb_out, IRStmt* st_orig, DiffEnv* diffenv){
  ExpressionHandling eh_para = {
    &parallel_or_zero, &load_expression_para, &store_expression_para,
    "x86g_amd64g_dirtyhelper_loadF80le_para", &x86g_amd64g_dirtyhelper_loadF80le_para,
    "x86g_amd64g_dirtyhelper_storeF80le_para", &x86g_amd64g_dirtyhelper_storeF80le_para
  };
  add_statement_modified(sb_out, st_orig, diffenv, 2, eh_para);
}

/*! Instrument an IRSB.
 */
static
IRSB* dg_instrument ( VgCallbackClosure* closure,
                      IRSB* sb_in,
                      const VexGuestLayout* layout, 
                      const VexGuestExtents* vge,
                      const VexArchInfo* archinfo_host,
                      IRType gWordTy, IRType hWordTy )
{
  int i;
  DiffEnv diffenv;
  IRSB* sb_out = deepCopyIRSBExceptStmts(sb_in);

  // allocate shadow temporaries and store offsets
  IRTemp nTmp =  sb_in->tyenv->types_used;
  diffenv.tmp_offset[1] = nTmp;
  for(IRTemp t=0; t<nTmp; t++){
    newIRTemp(sb_out->tyenv, sb_in->tyenv->types[t]);
  }
  if(paragrind){
    diffenv.tmp_offset[2] = 2*nTmp;
    for(IRTemp t=0; t<nTmp; t++){
      newIRTemp(sb_out->tyenv, sb_in->tyenv->types[t]);
    }
  }

  // shadow guest state (registers)
  diffenv.gs_offset[1] = layout->total_sizeB;
  diffenv.gs_offset[2] = 2*layout->total_sizeB;

  diffenv.sb_out = sb_out;

  // copy until IMark
  i = 0;
  while (i < sb_in->stmts_used && sb_in->stmts[i]->tag != Ist_IMark) {
     addStmtToIRSB(sb_out, sb_in->stmts[i]);
     i++;
  }
  for (/* use current i*/; i < sb_in->stmts_used; i++) {
    stmt_counter++;
    IRStmt* st_orig = sb_in->stmts[i];
     //VG_(printf)("next stmt %d :",stmt_counter); ppIRStmt(st_orig); VG_(printf)("\n");

    diffenv.cas_succeeded = IRTemp_INVALID;

    add_statement_forward(sb_out,st_orig,&diffenv);
    if(paragrind) add_statement_paragrind(sb_out,st_orig,&diffenv);
    add_statement_original(sb_out,st_orig, &diffenv);

  }
  //VG_(printf)("from stmt %d sb :",stmt_counter); ppIRSB(sb_out); VG_(printf)("\n");

  return sb_out;
}

static void dg_fini(Int exitcode)
{
  destroyShadowMap(sm_dot);
}

static void dg_pre_clo_init(void)
{


   VG_(details_name)            ("DerivGrind");
   VG_(details_version)         (NULL);
   VG_(details_description)     ("a tool for forward-mode AD of compiled programs.");
   VG_(details_copyright_author)(
      "Copyright (C) 2022, and GNU GPL'd, by the \n"
      "Chair for Scientific Computing at TU Kaiserslautern.");
   VG_(details_bug_reports_to)  ("derivgrind@scicomp.uni-kl.de");

   VG_(details_avg_translation_sizeB) ( 275 );

   VG_(basic_tool_funcs)        (dg_post_clo_init,
                                 dg_instrument,
                                 dg_fini);


   sm_dot = initializeShadowMap();

   VG_(needs_client_requests)     (dg_handle_client_request);

   VG_(needs_command_line_options)(dg_process_cmd_line_option,
                                   dg_print_usage,
                                   dg_print_debug_usage);

}

VG_DETERMINE_INTERFACE_VERSION(dg_pre_clo_init)

/*--------------------------------------------------------------------*/
/*--- end                                                          ---*/
/*--------------------------------------------------------------------*/
