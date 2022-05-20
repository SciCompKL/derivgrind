
/*--------------------------------------------------------------------*/
/*--- Nulgrind: The minimal Valgrind tool.               nl_main.c ---*/
/*--------------------------------------------------------------------*/

/*
   This file is part of Nulgrind, the minimal Valgrind tool,
   which does no instrumentation or analysis.

   Copyright (C) 2002-2017 Nicholas Nethercote
      njn@valgrind.org

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
#include "valgrind.h"


#include "shadow-memory/src/shadow.h"
// ----------------------------------------------------------------------------
// The following might end up in its own header file eventually, but for now
// only the application can really know how to set the right types and system
// calls.
/*#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stddef.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>

void  shadow_free(void* addr) { VG_(free)(addr); }
void *shadow_malloc(size_t size) { return VG_(malloc)(size); }
void *shadow_calloc(size_t nmemb, size_t size) { return VG_(calloc)(nmemb, size); }
void  shadow_memcpy(void* dst, void* src, size_t size) { VG_(memcpy)(dst,src,size); }
void  shadow_out_of_memory() {
  VG_(printf)("ERROR: Ran out of memory while allocating shadow memory.\n");
  exit(1);
}*/
// ----------------------------------------------------------------------------
inline void  shadow_free(void* addr) { VG_(free)(addr); }
inline void *shadow_malloc(size_t size) { return VG_(malloc)("Test",size); }
inline void *shadow_calloc(size_t nmemb, size_t size) { return VG_(calloc)("test", nmemb, size); }
inline void  shadow_memcpy(void* dst, void* src, size_t size) { VG_(memcpy)(dst,src,size); }
inline void  shadow_out_of_memory() {
  VG_(printf)("ERROR: Ran out of memory while allocating shadow memory.\n");
	VG_(exit)(1);
}


ShadowMap* my_sm = NULL;

static void nl_post_clo_init(void)
{
}

static void handle_expression(IRExpr* ex, Int recursive_level){
  for(int i=0; i<recursive_level; i++)
    VG_(printf)(" ");
  ppIRExpr(ex);
  VG_(printf)("\n");
  switch(ex->tag){
    case Iex_Qop:
      handle_expression(ex->Iex.Qop.details->arg1, recursive_level+1);
      handle_expression(ex->Iex.Qop.details->arg2, recursive_level+1);
      handle_expression(ex->Iex.Qop.details->arg3, recursive_level+1);
      handle_expression(ex->Iex.Qop.details->arg4, recursive_level+1);
      break;
    case Iex_Triop:
      handle_expression(ex->Iex.Triop.details->arg1, recursive_level+1);
      handle_expression(ex->Iex.Triop.details->arg2, recursive_level+1);
      handle_expression(ex->Iex.Triop.details->arg3, recursive_level+1);
      break;
    case Iex_Binop:
      handle_expression(ex->Iex.Binop.arg1, recursive_level+1);
      handle_expression(ex->Iex.Binop.arg2, recursive_level+1);
      break;
    case Iex_Unop:
      handle_expression(ex->Iex.Unop.arg, recursive_level+1);
      break;
    case Iex_Const:
      break;
  }

  if(recursive_level>=1 &&
     (ex->tag==Iex_Qop||ex->tag==Iex_Triop||ex->tag==Iex_Binop||ex->tag==Iex_Unop)){
      VG_(printf)("Not flat!\n");
  }
}


static
VG_REGPARM(0) void nl_Store_diff(Addr addr, ULong derivative){
  VG_(printf)("nl_Store_diff %p %lf\n", addr, *(Double*)&derivative );
  for(int i=0; i<8; i++){
    shadow_set_bits(my_sm,((SM_Addr)addr)+i,  *( ((U8*)&derivative) + i ));
  }
}

static
VG_REGPARM(0) ULong nl_Load_diff( Addr addr){
  ULong derivative=0;
  VG_(printf)("nl_Load_diff %p\n", addr);
  for(int i=0; i<8; i++){
    VG_(printf)("Iter %d, will access addr=%p and write to %p\n", i, ((SM_Addr)addr)+i,  ((U8*)&derivative)+i);
    shadow_get_bits(my_sm,((SM_Addr)addr)+i, ((U8*)&derivative)+i);
  }
  VG_(printf)("nl_Load_diff return %lf\n", *(double*)&derivative);
  return derivative;
}

static VG_REGPARM(0) void nl_Print_double(ULong value){ VG_(printf)("Value: %lf\n", *(double*)&value); }
static VG_REGPARM(0) void nl_Print_unsignedlong(ULong value){ VG_(printf)("Value: %lf\n", *(unsigned long*)&value); }

/*! Add dirty statement to IRSB, which prints the value of expr.
 */
static
void nl_add_print_stmt(IRSB* sb_out, IRExpr* expr){
  IRType type = typeOfIRExpr(sb_out->tyenv, expr);
  char* fname;
  void* fptr;
  IRExpr* expr_to_print;
  switch(type){
    case Ity_F64:
      fname = "nl_Print_double";
      fptr = nl_Print_double;
      expr_to_print = IRExpr_Unop(Iop_ReinterpF64asI64,expr);
      break;
    case Ity_I64:
      fname = "nl_Print_unsignedlong";
      fptr = nl_Print_unsignedlong;
      expr_to_print = expr;
      break;
    default:
      VG_(printf)("Bad type in nl_add_print_stmt.\n");
      return;
  }
  IRDirty* di = unsafeIRDirty_0_N(
        0,
        fname, VG_(fnptr_to_fnentry)(fptr),
        mkIRExprVec_1(expr_to_print));
  addStmtToIRSB(sb_out, IRStmt_Dirty(di));

}

static
Bool nl_handle_gdb_monitor_command(ThreadId tid, HChar* req){
  HChar s[VG_(strlen)(req)+1]; //!< copy of req for strtok_r
  VG_(strcpy)(s, req);
  HChar* ssaveptr; //!< internal state of strtok_r

  const HChar commands[] = "help get set"; //!< list of possible commands
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
        "  get <addr>       - Prints derivative\n"
        "  set <addr> <val> - Sets derivative\n"
      );
      return True;
    case 1: { // get
      HChar* address_str = VG_(strtok_r)(NULL, " ", &ssaveptr);
      Addr address;
      if(!VG_(parse_Addr)(&address_str, &address)){
        VG_(gdb_printf)("Usage: get <addr>\n");
        return False;
      }
      double derivative=0;
      for(int i=0; i<8; i++){
        shadow_get_bits(my_sm,((SM_Addr)address)+i, ((U8*)&derivative)+i);
      }
      VG_(gdb_printf)("Derivative: %lf\n", derivative);
      return True;
    }
    case 2: { // set
      HChar* address_str = VG_(strtok_r)(NULL, " ", &ssaveptr);
      Addr address;
      if(!VG_(parse_Addr)(&address_str, &address)){
        VG_(gdb_printf)("Usage: set <addr> <derivative>\n");
        return False;
      }
      HChar* derivative_str = VG_(strtok_r)(NULL, " ", &ssaveptr);
      double derivative = VG_(strtod)(derivative_str, NULL);
      for(int i=0; i<8; i++){
        shadow_set_bits( my_sm,((SM_Addr)address)+i, *( ((U8*)&derivative)+i ) );
      }
      return True;
    }
    default:
      VG_(printf)("Error in nl_handle_gdb_monitor_command.\n");
      return False;
    }

}

static
Bool nl_handle_client_request(ThreadId tid, UWord* arg, UWord* ret){
  if(arg[0]==VG_USERREQ__GDB_MONITOR_COMMAND){
    Bool handled = nl_handle_gdb_monitor_command(tid, (HChar*)arg[1]);
    if(handled){
      *ret = 1;
    } else {
      *ret = 0;
    }
    return handled;
  } else {
    VG_(printf)("Unhandled user request.\n");
    return True;
  }
}

typedef struct {
  IRTemp t_offset; // add this to a temporary index to get the shadow temporary index
  /*! layout argument to nl_instrument.
   *  Add layout->total_sizeB to a register index to get the shadow register index. */
  const VexGuestLayout* layout;
  IRSB* sb_out;
} DiffEnv;

static
IRExpr* differentiate_expr(IRExpr const* ex, DiffEnv diffenv ){
  IRExpr* DEFAULT_ROUNDING = IRExpr_Const(IRConst_U32(Irrm_NEAREST));
  switch(ex->tag){
    case Iex_Triop: {
      IRTriop* rex = ex->Iex.Triop.details;
      IRExpr* arg1 = rex->arg1;
      IRExpr* arg2 = rex->arg2;
      IRExpr* arg3 = rex->arg3;
      IRExpr* d2 = differentiate_expr(arg2,diffenv);
      IRExpr* d3 = differentiate_expr(arg3,diffenv);
      if(d2==NULL || d3==NULL) return NULL;
      switch(rex->op){
        case Iop_AddF64: {
          return IRExpr_Triop(Iop_AddF64,arg1,d2,d3);
        } break;
        case Iop_SubF64: {
         return IRExpr_Triop(Iop_SubF64,arg1,d2,d3);
        } break;
        case Iop_MulF64: {
          return IRExpr_Triop(Iop_AddF64,arg1,
            IRExpr_Triop(Iop_MulF64, arg1, d2,arg3),
            IRExpr_Triop(Iop_MulF64, arg1, d3,arg2)
          );
        } break;
        case Iop_DivF64: {
          return IRExpr_Triop(Iop_DivF64,arg1,
            IRExpr_Triop(Iop_SubF64, arg1,
              IRExpr_Triop(Iop_MulF64, arg1, d2,arg3),
              IRExpr_Triop(Iop_MulF64, arg1, d3,arg2)
            ),
            IRExpr_Triop(Iop_MulF64, arg1, arg3, arg3)
          );
        } break;
        default:
          return NULL;
      }
    } break;
    case Iex_Binop: {
      IROp op = ex->Iex.Binop.op;
      IRExpr* arg1 = ex->Iex.Binop.arg1;
      IRExpr* arg2 = ex->Iex.Binop.arg2;
      IRExpr* d2 = differentiate_expr(arg2,diffenv);
      if(d2==NULL) return NULL;
      switch(op){
        case Iop_SqrtF64: {
          IRExpr* numerator = d2;
          IRExpr* consttwo = IRExpr_Const(IRConst_F64(2.0));
          IRExpr* denominator =  IRExpr_Triop(Iop_MulF64, arg1, consttwo, IRExpr_Binop(Iop_SqrtF64, arg1, arg2) );
          nl_add_print_stmt(diffenv.sb_out, numerator);
          nl_add_print_stmt(diffenv.sb_out, denominator);
          return IRExpr_Triop(Iop_DivF64, arg1, numerator, denominator);
        } break;
        default:
          return NULL;
      }
    } break;
    case Iex_Unop: {
      IROp op = ex->Iex.Unop.op;
      IRExpr* arg = ex->Iex.Unop.arg;
      IRExpr* d = differentiate_expr(arg,diffenv);
      if(d==NULL) return NULL;
      switch(op){
        case Iop_NegF64: {
          return IRExpr_Unop(Iop_NegF64,d);
        } break;
        case Iop_AbsF64: {
          // if arg evaluates positive, the Iop_CmpF64 evaluates to 0 i.e. false
          IRExpr* cond = IRExpr_Binop(Iop_CmpF64, arg, IRExpr_Const(IRConst_F64(0.)));
          IRExpr* minus_d = IRExpr_Unop(Iop_NegF64, d);
          return IRExpr_ITE(cond, minus_d, d);
        } break;
        default:
          return NULL;
      }
    } break;
    case Iex_Const: {
      IRConst* rex = ex->Iex.Const.con;
      switch(rex->tag){
        case Ico_F64:
          return IRExpr_Const(IRConst_F64(0.));
        case Ico_F64i:
          return IRExpr_Const(IRConst_F64i(0.));
        default:
          return NULL;
      }
    } break;
    case Iex_ITE: {
      IRExpr* dtrue = differentiate_expr(ex->Iex.ITE.iftrue, diffenv);
      IRExpr* dfalse = differentiate_expr(ex->Iex.ITE.iffalse, diffenv);
      if(dtrue==NULL || dfalse==NULL) return NULL;
      else return IRExpr_ITE(ex->Iex.ITE.cond, dtrue, dfalse);
    } break;
    case Iex_RdTmp: {
      IRTemp t = ex->Iex.RdTmp.tmp;
      switch(diffenv.sb_out->tyenv->types[t]){
        case Ity_F64:
          return IRExpr_RdTmp(t+diffenv.t_offset);
        default:
          return NULL;
      }
    } break;
    case Iex_Get: {
      switch(ex->Iex.Get.ty){
        case Ity_F64:
          return IRExpr_Get(ex->Iex.Get.offset+diffenv.layout->total_sizeB,ex->Iex.Get.ty);
        default:
          return NULL;
      }
    } break;
    case Iex_GetI: {
      IRRegArray* descr = ex->Iex.GetI.descr;
      IRExpr* ix = ex->Iex.GetI.ix;
      Int bias = ex->Iex.GetI.bias;
      switch(descr->elemTy){
        case Ity_F64: {
          IRRegArray* descr_diff = mkIRRegArray(descr->base+diffenv.layout->total_sizeB,descr->elemTy,descr->nElems);
          return IRExpr_GetI(descr_diff,ix,bias+diffenv.layout->total_sizeB);
        }
        default:
          return NULL;
      }
    } break;
    case Iex_Load: {
      switch(ex->Iex.Load.ty){
        case Ity_F64: {
          // load data
          IRTemp loadAddr = newIRTemp(diffenv.sb_out->tyenv, Ity_I64);
          IRDirty* di = unsafeIRDirty_1_N(
                loadAddr,
                0,
                "nl_Load_diff", VG_(fnptr_to_fnentry)(nl_Load_diff),
                mkIRExprVec_1(ex->Iex.Load.addr));
          addStmtToIRSB(diffenv.sb_out, IRStmt_Dirty(di));
          // convert into F64
          IRTemp loadAddr_reinterpreted = newIRTemp(diffenv.sb_out->tyenv, Ity_F64);
          addStmtToIRSB(diffenv.sb_out, IRStmt_WrTmp(loadAddr_reinterpreted,
            IRExpr_Unop(Iop_ReinterpI64asF64,IRExpr_RdTmp(loadAddr))));
          // return this
          return IRExpr_RdTmp(loadAddr_reinterpreted);
        } break;
        default:
          return NULL;
      }
    } break;
    default:
      return NULL;
  }
}


static
IRSB* nl_instrument ( VgCallbackClosure* closure,
                      IRSB* sb_in,
                      const VexGuestLayout* layout, 
                      const VexGuestExtents* vge,
                      const VexArchInfo* archinfo_host,
                      IRType gWordTy, IRType hWordTy )
{
  int i;
  DiffEnv diffenv;
  IRSB* sb_out = deepCopyIRSBExceptStmts(sb_in);
  // append the "gradient temporaries" to the "value temporaries",
  // doubling the number of temporaries
  diffenv.t_offset = sb_in->tyenv->types_used;
  for(i=0; i<sb_in->stmts_used; i++){
    if(sb_in->stmts[i]->tag == Ist_WrTmp){
      tl_assert( sb_in->stmts[i]->Ist.WrTmp.tmp < diffenv.t_offset );
    }
  }
  for(IRTemp t=0; t<diffenv.t_offset; t++){
    newIRTemp(sb_out->tyenv, sb_out->tyenv->types[t]);
  }

  diffenv.sb_out = sb_out;
  diffenv.layout = layout;

  // copy until IMark
  i = 0;
  while (i < sb_in->stmts_used && sb_in->stmts[i]->tag != Ist_IMark) {
     addStmtToIRSB(sb_out, sb_in->stmts[i]);
     i++;
  }
  for (/* use current i*/; i < sb_in->stmts_used; i++) {
    IRStmt* st = sb_in->stmts[i];
    //VG_(printf)("next stmt:"); ppIRStmt(st); VG_(printf)("\n");
    switch(st->tag){
      case Ist_WrTmp: {
        // AD treatment only if a floating point type is written
        IRType type = sb_in->tyenv->types[st->Ist.WrTmp.tmp];
        if(type==Ity_F64){
          IRExpr* differentiated_expr = differentiate_expr(st->Ist.WrTmp.data, diffenv);
          if(!differentiated_expr){
            differentiated_expr = IRExpr_Const(IRConst_F64(0.));
            VG_(printf)("Warning: Expression\n");
            ppIRExpr(st->Ist.WrTmp.data);
            VG_(printf)("could not be differentiated, WrTmp'ting 0 instead.\n\n");
          }
          IRStmt* sp = IRStmt_WrTmp(st->Ist.WrTmp.tmp+diffenv.t_offset, differentiated_expr);
          addStmtToIRSB(sb_out, sp);
        }
        break;
      }
      case Ist_Put: {
        IRType type = typeOfIRExpr(sb_in->tyenv,st->Ist.Put.data);
        if(type==Ity_F64){
          IRExpr* differentiated_expr = differentiate_expr(st->Ist.Put.data, diffenv);
          if(!differentiated_expr){
            differentiated_expr = IRExpr_Const(IRConst_F64(0.));
            VG_(printf)("Warning: Expression\n");
            ppIRExpr(st->Ist.Put.data);
            VG_(printf)("could not be differentiated, Put'ting 0 instead.\n\n");
          }
          IRStmt* sp = IRStmt_Put(st->Ist.Put.offset + diffenv.layout->total_sizeB, differentiated_expr);
          addStmtToIRSB(sb_out, sp);
        }
        break;
      }
      case Ist_PutI: {
        IRType type = typeOfIRExpr(sb_in->tyenv,st->Ist.PutI.details->data);
        if(type==Ity_F64){
          IRExpr* differentiated_expr = differentiate_expr(st->Ist.PutI.details->data, diffenv);
          if(!differentiated_expr){
            differentiated_expr = IRExpr_Const(IRConst_F64(0.));
            VG_(printf)("Warning: Expression\n");
            ppIRExpr(st->Ist.PutI.details->data);
            VG_(printf)("could not be differentiated, PutI'ing 0 instead.\n\n");
          }
          IRRegArray* descr = st->Ist.PutI.details->descr;
          IRRegArray* descr_diff = mkIRRegArray(descr->base+diffenv.layout->total_sizeB, descr->elemTy, descr->nElems);
          Int bias = st->Ist.PutI.details->bias;
          IRExpr* ix = st->Ist.PutI.details->ix;
          IRStmt* sp = IRStmt_PutI(mkIRPutI(descr_diff,ix,bias+diffenv.layout->total_sizeB,differentiated_expr));
          addStmtToIRSB(sb_out, sp);
        }
        break;
      }
      case Ist_Store: case Ist_StoreG: {
        IREndness end; IRExpr* addr; IRExpr* data; Bool guarded;
        if(st->tag == Ist_Store){
          end = st->Ist.Store.end;
          addr = st->Ist.Store.addr;
          data = st->Ist.Store.data;
          guarded = False;
        } else {
          end = st->Ist.StoreG.details->end;
          addr = st->Ist.StoreG.details->addr;
          data = st->Ist.StoreG.details->data;
          guarded = True;
        }
        IRType type = typeOfIRExpr(sb_in->tyenv,data);
        if(type==Ity_F64){
          IRExpr* differentiated_expr = differentiate_expr(data, diffenv);
          // The Store.data is an IREXpr_Const or IRExpr_Tmp, so this holds
          // for its derivative as well. Compare this to Memcheck's IRAtom.
          // Still general treatment here.
          if(!differentiated_expr){
            differentiated_expr = IRExpr_Const(IRConst_F64(0.));
            VG_(printf)("Warning: Expression\n");
            ppIRExpr(data);
            VG_(printf)("could not be differentiated, Store'ing 0 instead.\n\n");
          }
          IRExpr* differentiated_expr_reinterpreted =
              IRExpr_Unop(Iop_ReinterpF64asI64, differentiated_expr);
          IRDirty* di = unsafeIRDirty_0_N(
                  0,
                  "nl_Store_diff", VG_(fnptr_to_fnentry)(nl_Store_diff),
                  mkIRExprVec_2(addr,differentiated_expr_reinterpreted));
          if(guarded){
            di->guard = st->Ist.StoreG.details->guard;
          }
          IRStmt* sp = IRStmt_Dirty(di);
          addStmtToIRSB(sb_out, sp);
        }
        break;
      }
      case Ist_LoadG: {
        IRLoadG* det = st->Ist.LoadG.details;
        IRType type = sb_in->tyenv->types[det->dst];
        if(type==Ity_F64){
          tl_assert(det->cvt == ILGop_Ident64); // what else could you load into double?
          IRExpr* differentiated_expr_alt = differentiate_expr(det->alt,diffenv);
          if(!differentiated_expr_alt){
            differentiated_expr_alt = IRExpr_Const(IRConst_F64(0.));
            VG_(printf)("Warning: Expression\n");
            ppIRExpr(det->alt);
            VG_(printf)("could not be differentiated, alternative-LoadG'ing 0 instead.\n\n");
          }
          // compare the following to Iop_Load
          IRTemp loadAddr = newIRTemp(diffenv.sb_out->tyenv, Ity_I64);
          IRDirty* di = unsafeIRDirty_1_N(
                loadAddr,
                0,
                "nl_Load_diff", VG_(fnptr_to_fnentry)(nl_Load_diff),
                mkIRExprVec_1(det->addr));
          addStmtToIRSB(diffenv.sb_out, IRStmt_Dirty(di));
          // convert into F64
          IRTemp loadAddr_reinterpreted = newIRTemp(diffenv.sb_out->tyenv, Ity_F64);
          addStmtToIRSB(diffenv.sb_out, IRStmt_WrTmp(loadAddr_reinterpreted,
            IRExpr_Unop(Iop_ReinterpI64asF64,IRExpr_RdTmp(loadAddr))));
          // copy it into shadow variable for temporary dst,
          // or the derivative of alt, depending on the guard
          addStmtToIRSB(diffenv.sb_out, IRStmt_WrTmp(det->dst+diffenv.t_offset,
            IRExpr_ITE(det->guard,IRExpr_RdTmp(loadAddr_reinterpreted),differentiated_expr_alt)
          ));
        }
      }

      case Ist_CAS: {
        VG_(printf)("Did not instrument Ist_CAS statement.\n");
        break;
      }

      case Ist_LLSC: {
        VG_(printf)("Did not instrument Ist_LLSC statement.\n");
        break;
      }

      case Ist_Dirty: {
        VG_(printf)("Cannot instrument Ist_Dirty statement.\n");
        break;
      }

      // the following statement types can be ignored by each tool
      case Ist_NoOp: case Ist_IMark: case Ist_AbiHint:
        break;
      // the following statements have no relevance for AD
      case Ist_Exit: case Ist_MBE:
        break;

      default: {
        tl_assert(False);
        break;
      }
    }
    addStmtToIRSB(sb_out, st);

  }

   return sb_out;
}

static void nl_fini(Int exitcode)
{
  VG_(printf)("%d %d %d %d %d %d %d %d %d %d %d %d \n",
              Iop_DivF64, Iop_DivF32, Iop_DivF64r32,
              Iop_DivF128, Iop_DivD64, Iop_DivD128,
              Iop_Div32Fx4, Iop_Div32F0x4, Iop_Div64Fx2,
              Iop_Div64F0x2, Iop_Div64Fx4, Iop_Div32Fx8   );
  shadow_destroy_map(my_sm);
  VG_(free)(my_sm);

}

static void nl_pre_clo_init(void)
{


   VG_(details_name)            ("Nulgrind");
   VG_(details_version)         (NULL);
   VG_(details_description)     ("the minimal Valgrind tool");
   VG_(details_copyright_author)(
      "Copyright (C) 2002-2017, and GNU GPL'd, by Nicholas Nethercote.");
   VG_(details_bug_reports_to)  (VG_BUGS_TO);

   VG_(details_avg_translation_sizeB) ( 275 );

   VG_(basic_tool_funcs)        (nl_post_clo_init,
                                 nl_instrument,
                                 nl_fini);

   /* No needs, no core events to track */
   VG_(printf)("Allocate SM...");
   my_sm = (ShadowMap*) VG_(malloc)("Some text", sizeof(ShadowMap));
   if(my_sm==NULL) VG_(printf)("Error\n");
   my_sm->shadow_bits = 1;
   my_sm->application_bits = 1;
   my_sm->num_distinguished = 1;
   shadow_initialize_map(my_sm);
   VG_(printf)("done\n");

   VG_(needs_client_requests)     (nl_handle_client_request);

  shadow_set_bits(my_sm, 0xffff1111, 0xab);

}

VG_DETERMINE_INTERFACE_VERSION(nl_pre_clo_init)

/*--------------------------------------------------------------------*/
/*--- end                                                          ---*/
/*--------------------------------------------------------------------*/
