
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
  }

  if(recursive_level>=1 &&
     (ex->tag==Iex_Qop||ex->tag==Iex_Triop||ex->tag==Iex_Binop||ex->tag==Iex_Unop)){
      VG_(printf)("Not flat!\n");
  }
}

typedef struct {
  IRTemp t_offset; // add this to a temporary index to get the shadow temporaty index
  IRTypeEnv* tyenv; // contains typemap of the target SB
  Int total_sizeB; // add this to a register index to get the shadow register index
} DiffEnv;

static
IRExpr* differentiate_expr(IRExpr const* ex, DiffEnv diffenv ){
  switch(ex->tag){
    case Iex_Triop: {
      IRTriop* rex = ex->Iex.Triop.details;
      switch(rex->op){
        case Iop_AddF64: {
          IRExpr* d2 = differentiate_expr(rex->arg2,diffenv);
          IRExpr* d3 = differentiate_expr(rex->arg3,diffenv);
          if(d2==NULL || d3==NULL) return NULL;
          else return IRExpr_Triop(Iop_AddF64,rex->arg1,d2,d3);
        } break;
        case Iop_SubF64: {
          IRExpr* d2 = differentiate_expr(rex->arg2,diffenv);
          IRExpr* d3 = differentiate_expr(rex->arg3,diffenv);
          if(d2==NULL || d3==NULL) return NULL;
          else return IRExpr_Triop(Iop_SubF64,rex->arg1,d2,d3);
        } break;
        case Iop_MulF64: {
          IRExpr* d2 = differentiate_expr(rex->arg2,diffenv);
          IRExpr* d3 = differentiate_expr(rex->arg3,diffenv);
          if(d2==NULL || d3==NULL) return NULL;
          else return IRExpr_Triop(Iop_AddF64,rex->arg1,
            IRExpr_Triop(Iop_MulF64, rex->arg1, d2,rex->arg3),
            IRExpr_Triop(Iop_MulF64, rex->arg1, d3,rex->arg2)
          );
        } break;
        case Iop_DivF64: {
          IRExpr* d2 = differentiate_expr(rex->arg2,diffenv);
          IRExpr* d3 = differentiate_expr(rex->arg3,diffenv);
          if(d2==NULL || d3==NULL) return NULL;
          else return IRExpr_Triop(Iop_DivF64,rex->arg1,
            IRExpr_Triop(Iop_SubF64, rex->arg1,
              IRExpr_Triop(Iop_MulF64, rex->arg1, d2,rex->arg3),
              IRExpr_Triop(Iop_MulF64, rex->arg1, d3,rex->arg2)
            ),
            IRExpr_Triop(Iop_MulF64, rex->arg1, rex->arg3, rex->arg3)
          );
        } break;
        default:
          return NULL;
      }
      break;
    }
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
    }
    case Iex_RdTmp: {
      IRTemp t = ex->Iex.RdTmp.tmp;
      switch(diffenv.tyenv->types[t]){
        case Ity_F64:
          return IRExpr_RdTmp(t+diffenv.t_offset);
        default:
          return NULL;
      }
    case Iex_Get: {
      switch(ex->Iex.Get.ty){
        case Ity_F64:
          return IRExpr_Get(ex->Iex.Get.offset+diffenv.total_sizeB,ex->Iex.Get.ty);
        default:
          return NULL;
      }
    }
    default:
      return NULL;
    }
  }
}

static
VG_REGPARM(0) void nl_Store_diff(Addr addr, Double diff){
  VG_(printf)("nl_Store_diff %d %lf", addr, diff);
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

  diffenv.tyenv = sb_out->tyenv;

  // copy until IMark
  i = 0;
  while (i < sb_in->stmts_used && sb_in->stmts[i]->tag != Ist_IMark) {
     addStmtToIRSB(sb_out, sb_in->stmts[i]);
     i++;
  }
  for (/* use current i*/; i < sb_in->stmts_used; i++) {
    IRStmt* st = sb_in->stmts[i];
    switch(st->tag){
      case Ist_WrTmp: {
        // AD treatment only if a floating point type is written
        IRType type = sb_in->tyenv->types[st->Ist.WrTmp.tmp];
        if(type==Ity_F64){
          IRExpr* differentiated_expr = differentiate_expr(st->Ist.WrTmp.data, diffenv);
          if(differentiated_expr){ // we were able to differentiate expression
            IRStmt* sp = IRStmt_WrTmp(st->Ist.WrTmp.tmp+diffenv.t_offset, differentiated_expr);
            addStmtToIRSB(sb_out, sp);
            VG_(printf)("new statement: "); ppIRStmt(sp); VG_(printf)("\n");
          } else {
            VG_(printf)("Warning: Expression\n");
            ppIRExpr(st->Ist.WrTmp.data);
            VG_(printf)("could not be differentiated, putting 0 instead.\n\n");
            IRStmt* sp = IRStmt_WrTmp(st->Ist.WrTmp.tmp+diffenv.t_offset, IRExpr_Const(IRConst_F64(0.)));
            addStmtToIRSB(sb_out, sp);
          }
        }
        addStmtToIRSB(sb_out, st);
        break;
      }
      case Ist_Put: {
        IRExpr* differentiated_expr = differentiate_expr(st->Ist.Put.data, diffenv);
        if(differentiated_expr){
          IRStmt* sp = IRStmt_Put(st->Ist.Put.offset + diffenv.total_sizeB, differentiated_expr);
          addStmtToIRSB(sb_out, sp);
          VG_(printf)("new statement: "); ppIRStmt(sp); VG_(printf)("\n");
        }
        addStmtToIRSB(sb_out, st);
        break;
      }
      case Ist_Store: {
        IRExpr* differentiated_expr = differentiate_expr(st->Ist.Store.data, diffenv);
        // The Store.data is an IREXpr_Const or IRExpr_Tmp, so this holds
        // for its derivative as well. Compare this to Memcheck's IRAtom.
        if(differentiated_expr){
          IRDirty* di = unsafeIRDirty_0_N(
                0,
                "nl_Store_diff", VG_(fnptr_to_fnentry)(nl_Store_diff),
                mkIRExprVec_2(st->Ist.Store.addr, differentiated_expr));
          IRStmt* sp = IRStmt_Dirty(di);
          addStmtToIRSB(sb_out, sp);
          VG_(printf)("new statement store: "); ppIRStmt(sp); VG_(printf)("\n");
        }
        addStmtToIRSB(sb_out, st);
        break;
      }

      default: {
        addStmtToIRSB(sb_out, st);
        break;
      }
    }

  }

  U8 tmp = 0;
   shadow_get_bits(my_sm, 0xffff1111, &tmp);
  VG_(printf)("shadow bits: %d\n", tmp);
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

  shadow_set_bits(my_sm, 0xffff1111, 0xab);

}

VG_DETERMINE_INTERFACE_VERSION(nl_pre_clo_init)

/*--------------------------------------------------------------------*/
/*--- end                                                          ---*/
/*--------------------------------------------------------------------*/
