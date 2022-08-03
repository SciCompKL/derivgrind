/*--------------------------------------------------------------------*/
/*--- Shadow memory stuff.                             dg_shadow.c ---*/
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

/*! \file dg_shadow.c
 *  Shadow memory stuff for DerivGrind.
 */


/*! \page loading_and_storing Loading and storing tangent values in memory
 *
 *  When translating
 *  - an Ist_Store or Ist_StoreG statement to store data in memory, or
 *  - an Iex_Load expression or Iex_LoadG statements to load data from memory,
 *
 *  DerivGrind has to add instrumentation accessing the shadow memory.
 *  To this end, DerivGrind emits Ist_Dirty statements calling the
 *  functions dg_Store_diffN, dg_Load_diffN.
 *
 *  Dirty calls can only call functions that accept and return 4- and 8-byte
 *  integers arguments. Therefore we have to "encode" all types, including
 *  floating-point and SIMD types, by up to four 8-byte integers (Ity_I64 on
 *  the VEX side, ULong on the C side).
 *
 *  Storing derivative information
 *  ------------------------------
 *  During translation, we call convertToInteger on the IRExpr for the
 *  derivative. This wraps some instructions around the IRExpr, giving
 *  an array of four IRExpr's that evaluate to the type Ity_I64. These
 *  are used to create up to four Ist_Dirty statements for dg_Store_diffN.
 *
 *  When the translated IRSB is executed, the wrapper
 *  VEX instructions encode the value on the simulated CPU and then the
 *  Ist_Dirty mechanism calls dg_Store_diffN with the encoded values of
 *  type ULong. Inside dg_Store_diffN we decode the data and store it in
 *  shadow memory.
 *
 *  Loading derivative information
 *  ------------------------------
 *  Ist_Dirty statements calling dg_Load_diffN are added during translation.
 *  When the translated IRSB is executed, the Ist_Dirty mechanism calls
 *  dg_Load_diffN, which reads the gradient information from shadow memory,
 *  encodes it, and returns a ULong which is stored as Ity_I64 in a temporary.
 *  In the transation phase we also wrap these up to four temporaries by
 *  convertFromInteger, which adds VEX instructions that convert the Ity_I64
 *  data back to the original type.
 *
 *  How the encoding works
 *  ----------------------
 *  For most types, this "encoding" is a binary reinterpretation, and padding
 *  by zeros. On the VEX side, this is accomplished by instructions like
 *  Iop_ReinterpF64asI64, Iop_32HLto64, Iop_128to64. On the C side, no explicit
 *  conversion is required because the gradient information is anyways accessed
 *  per byte.
 *
 *  For the types Ity_F16 and Ity_D32, we miss the corresponding VEX
 *  reinterpretation instructions. One way to circumvent this could be
 *  to encode them in a different way, e.g. first cast Ity_D32 to Ity_D64
 *  and then reinterpret this as Ity_I64. However, in order to revert this
 *  on the C side, Valgrind must be compiled with a compiler that supports
 *  the C type _Decimal32. To avoid this restriction, we just drop any gradient
 *  information associated to such expressions when they are stored in memory.
 *
 *  Issues
 *  ------
 *  We always assume a little-endian storage order, so the code might be incorrect
 *  for architectures other than x86/amd64.
 *
 */

#include "dg_shadow.h"

#ifdef BUILD_32BIT
#include "shadow-memory/src/shadow.h"
#else
#include "shadow-memory/src/shadow-64.h"
#endif

inline void  shadow_free(void* addr) { VG_(free)(addr); }
inline void *shadow_malloc(size_t size) { return VG_(malloc)("Test",size); }
inline void *shadow_calloc(size_t nmemb, size_t size) { return VG_(calloc)("test", nmemb, size); }
inline void  shadow_memcpy(void* dst, void* src, size_t size) { VG_(memcpy)(dst,src,size); }
inline void  shadow_out_of_memory() {
  VG_(printf)("ERROR: Ran out of memory while allocating shadow memory.\n");
        VG_(exit)(1);
}

/*! The shadow map we are currently working on. */
static ShadowMap* sm_current;

void setCurrentShadowMap(void* sm){
  sm_current = sm;
}

void* initializeShadowMap(){
  ShadowMap* sm = (ShadowMap*) VG_(malloc)("allocate_shadow_memory", sizeof(ShadowMap));
  #ifdef BUILD_32BIT
  if(sm==NULL) VG_(printf)("Error allocating space for ShadowMap.\n");
  sm->shadow_bits = 1;
  sm->application_bits = 1;
  sm->num_distinguished = 1;
  shadow_initialize_map(sm);
  #else
  shadow_initialize_with_mmap(sm);
  #endif
  return sm;
}

void destroyShadowMap(void* sm){
  shadow_destroy_map(sm);
  VG_(free)(sm);
}

void shadowGet(void* sm_address, void* real_address, int size){
  for(int i=0; i<size; i++){
    shadow_get_bits(sm_current, (SM_Addr)(sm_address+i), (U8*)real_address+i);
  }
}
void shadowSet(void* sm_address, void* real_address, int size){
  for(int i=0; i<size; i++){
    shadow_set_bits(sm_current, (SM_Addr)(sm_address+i), *( (U8*)real_address+i ));
  }
}

/*! Store an 8-byte tangent value in shadow memory.
 *
 *  To be invoked by dirty helper functions dg_Store_diffN that are
 *  themselves invoked by a dirty call during execution of the generated code.
 *  \param[in] addr - Address of the variable.
 *  \param[in] derivative - Tangent value, reinterpreted as 8-byte integer.
 *  \param[in] size - Original size of the variable.
 */
static void dg_Store_diff(Addr addr, ULong derivative, UChar size){
  shadowSet((void*)addr, (void*)&derivative, size);
}

static VG_REGPARM(0) void dg_Store_diff1(Addr addr, ULong derivative){dg_Store_diff(addr,derivative,1);}
static VG_REGPARM(0) void dg_Store_diff2(Addr addr, ULong derivative){dg_Store_diff(addr,derivative,2);}
static VG_REGPARM(0) void dg_Store_diff4(Addr addr, ULong derivative){dg_Store_diff(addr,derivative,4);}
static VG_REGPARM(0) void dg_Store_diff8(Addr addr, ULong derivative){dg_Store_diff(addr,derivative,8);}

/*! Load a tangent value from shadow memory.
 *
 *  To be invoked by dirty helper functions dg_Load_diffN that are
 *  themselves invoked by a dirty call during execution of the generated code.
 *  \param[in] addr - Address of the variable.
 *  \param[in] size - Original size of the variable
 *  \returns Tangent value, reinterpreted as 8-byte integer.
 */
static ULong dg_Load_diff(Addr addr, UChar size){
  ULong derivative=0;
  shadowGet((void*)addr, (void*)&derivative, size);
  for(int i=size; i<8; i++){
    *((U8*)&derivative+i) = 0;
  }
  return derivative;
}

static VG_REGPARM(0) ULong dg_Load_diff1(Addr addr){return dg_Load_diff(addr,1);}
static VG_REGPARM(0) ULong dg_Load_diff2(Addr addr){return dg_Load_diff(addr,2);}
static VG_REGPARM(0) ULong dg_Load_diff4(Addr addr){return dg_Load_diff(addr,4);}
static VG_REGPARM(0) ULong dg_Load_diff8(Addr addr){return dg_Load_diff(addr,8);}

/*! Reinterpret given expression as 4xI64 in VEX.
 *  This function complements convertFromInteger.
 *  \param[in] expr - Expression whose value should be reinterpreted as 4xI64.
 *  \param[out] converted - 4xI64 reinterpreted expressions.
 *    Unnecessary expressions can be NULL.
 *  \param[in] type - Type of expr.
 *  \returns Expression reinterpreted as 4xI64.
 */
static void convertToInteger(IRExpr* expr, IRExpr** converted, IRType type){
  converted[0] = converted[1] = converted[2] = converted[3] = NULL;
  IRExpr* zero8 = IRExpr_Const(IRConst_U8(0));
  IRExpr* zero16 = IRExpr_Const(IRConst_U16(0));
  IRExpr* zero32 = IRExpr_Const(IRConst_U32(0));
  switch(type){
    case Ity_INVALID:
      VG_(printf)("Ity_INVALID encountered in convertToInteger.\n");
      tl_assert(False); break;
    case Ity_I1:
      // It would be very strange if this type transfers parts of an
      // active variable, but let's still treat it.
      converted[0] = IRExpr_Binop(Iop_32HLto64, zero32,
               IRExpr_Binop(Iop_16HLto32, zero16,
                 IRExpr_Binop(Iop_8HLto16, zero8,
                   IRExpr_Unop(Iop_1Uto8, expr)
             )));
      break;
    case Ity_F32:
      converted[0] = IRExpr_Binop(Iop_32HLto64, zero32, IRExpr_Unop(Iop_ReinterpF32asI32, expr));
      break;
    case Ity_F64:
      converted[0] = IRExpr_Unop(Iop_ReinterpF64asI64, expr);
      break;
    case Ity_I8:
      converted[0] = IRExpr_Binop(Iop_32HLto64, zero32,
               IRExpr_Binop(Iop_16HLto32, zero16,
                 IRExpr_Binop(Iop_8HLto16, zero8,
                   expr
             )));
      break;
    case Ity_I16:
      converted[0] = IRExpr_Binop(Iop_32HLto64, zero32,
               IRExpr_Binop(Iop_16HLto32, zero16,
                 expr
             ));
      break;
    case Ity_I32:
      converted[0] = IRExpr_Binop(Iop_32HLto64, zero32,
               expr
             );
      break;
    case Ity_I64:
      converted[0] = expr;
      break;
    case Ity_I128:
      converted[0] = IRExpr_Unop(Iop_128HIto64, expr);
      converted[1] = IRExpr_Unop(Iop_128to64, expr);
      break;
    case Ity_V128:
      converted[0] = IRExpr_Unop(Iop_V128to64, expr);
      converted[1] = IRExpr_Unop(Iop_V128HIto64, expr);
      break;
    case Ity_V256:
      converted[0] = IRExpr_Unop(Iop_V256to64_0, expr);
      converted[1] = IRExpr_Unop(Iop_V256to64_1, expr);
      converted[2] = IRExpr_Unop(Iop_V256to64_2, expr);
      converted[3] = IRExpr_Unop(Iop_V256to64_3, expr);
      break;
    case Ity_D64:
      converted[0] = IRExpr_Unop(Iop_ReinterpD64asI64, expr);
      break;
    case Ity_D32:
      // We're missing the appropriate VEX instruction
      // for reinterpretation.
      converted[0] = IRExpr_Const(IRConst_U64(0));
      break;
    case Ity_D128:
      // no idea if this is correct
      converted[0] = IRExpr_Unop(Iop_ReinterpD64asI64, IRExpr_Unop(Iop_D128HItoD64, expr));
      converted[1] = IRExpr_Unop(Iop_ReinterpD64asI64, IRExpr_Unop(Iop_D128toD64, expr));
      break;
    case Ity_F16:
      // We're missing the appropriate VEX instruction
      // for reinterpretation
      converted[0] = IRExpr_Const(IRConst_U64(0));
      break;
    case Ity_F128:
      converted[0] = IRExpr_Unop(Iop_128HIto64, IRExpr_Unop(Iop_ReinterpF128asI128, expr));
      converted[1] = IRExpr_Unop(Iop_128to64, IRExpr_Unop(Iop_ReinterpF128asI128, expr));
      break;
    default:
      VG_(printf)("Bad type encountered in convertToInteger.\n");
      tl_assert(False);
      break;
  }
}

/*! Reinterpret 4xI64 as expression of a given type in VEX.
 *  This function complements convertToInteger.
 *  \param[in] expr - 4xI64 expressions whose value should be reinterpreted.
 *     Unnecessary expressions can be NULL.
 *  \param[in] type - Type to convert to.
 *  \returns 4xI64 expression reinterpreted in another type.
 */
static IRExpr* convertFromInteger(IRExpr** expr, IRType type){
  switch(type){
    case Ity_INVALID:
      VG_(printf)("Ity_INVALID encountered in convertFromInteger.\n");
      tl_assert(False); break;
    case Ity_I1:
      return IRExpr_Unop(Iop_32to1, IRExpr_Unop(Iop_64to32,expr[0]));
    case Ity_F32:
      return IRExpr_Unop(Iop_ReinterpI32asF32, IRExpr_Unop(Iop_64to32,expr[0]));
    case Ity_F64:
      return IRExpr_Unop(Iop_ReinterpI64asF64, expr[0]);
    case Ity_I8:
      return IRExpr_Unop(Iop_16to8,
               IRExpr_Unop(Iop_32to16,
                 IRExpr_Unop(Iop_64to32,
                   expr[0]
             )));
    case Ity_I16:
      return IRExpr_Unop(Iop_32to16,
               IRExpr_Unop(Iop_64to32,
                 expr[0]
             ));
    case Ity_I32:
      return IRExpr_Unop(Iop_64to32,
               expr[0]
             );
    case Ity_I64:
      return expr[0];
    case Ity_I128:
      return IRExpr_Binop(Iop_64HLto128,expr[0], expr[1]);
    case Ity_V128:
      return IRExpr_Binop(Iop_64HLtoV128,expr[1],expr[0]);
    case Ity_V256:
      return IRExpr_Qop(Iop_64x4toV256,expr[3],expr[2],expr[1],expr[0]);
    case Ity_D64:
      return IRExpr_Unop(Iop_ReinterpI64asD64, expr[0]);
    case Ity_D32:
      // Ity_D32 and Ity_F16 lose gradient information.
      // See convertToInteger.
      return mkIRConst_zero(Ity_D32);
    case Ity_D128:
      // no idea whether this is correct
      return IRExpr_Binop(Iop_D64HLtoD128, IRExpr_Unop(Iop_ReinterpI64asD64,expr[0]), IRExpr_Unop(Iop_ReinterpI64asD64,expr[1]));
    case Ity_F16:
      // Ity_D32 and Ity_F16 lose gradient information.
      // See convertToInteger.
      return mkIRConst_zero(Ity_F16);
    case Ity_F128:
      return IRExpr_Unop(Iop_ReinterpI128asF128, IRExpr_Binop(Iop_64HLtoV128,expr[0],expr[1]));
    default:
      VG_(printf)("Bad type encountered in convertFromInteger.\n");
      tl_assert(False);
      return NULL;
  }
}


IRExpr* loadShadowMemory(IRSB* sb_out, IRExpr* addr, IRType type){

  UChar n64Blocks; // necessary number of loads of Ity_I64: 1, 2 or 4
  const char* fname; // name of function that Ist_Dirty will call
  void* fn; // pointer to function that Ist_Dirty will call
  switch(sizeofIRType(type)){
    case 1:  n64Blocks=1; fname="dg_Load_diff1"; fn=dg_Load_diff1; break;
    case 2:  n64Blocks=1; fname="dg_Load_diff2"; fn=dg_Load_diff2; break;
    case 4:  n64Blocks=1; fname="dg_Load_diff4"; fn=dg_Load_diff4; break;
    case 8:  n64Blocks=1; fname="dg_Load_diff8"; fn=dg_Load_diff8; break;
    case 16: n64Blocks=2; fname="dg_Load_diff8"; fn=dg_Load_diff8; break;
    case 32: n64Blocks=4; fname="dg_Load_diff8"; fn=dg_Load_diff8; break;
    default: tl_assert(False);
  }
  // reserve Ity_I64 temporaries to load into
  IRTemp loadAddr[4];
  loadAddr[1] = loadAddr[2] = loadAddr[3] = IRTemp_INVALID;
  loadAddr[0] = newIRTemp(sb_out->tyenv, Ity_I64);
  if(n64Blocks>=2){
    loadAddr[1] = newIRTemp(sb_out->tyenv, Ity_I64);
    if(n64Blocks==4){
      loadAddr[2] = newIRTemp(sb_out->tyenv, Ity_I64);
      loadAddr[3] = newIRTemp(sb_out->tyenv, Ity_I64);
    }
  }
  // find Iop and offsets to shift address
  IROp add;
  IRExpr* offsets[4];
  switch(typeOfIRExpr(sb_out->tyenv,addr)){
    case Ity_I32:
      add = Iop_Add32;
      for(int i=0; i<4; i++) offsets[i] = IRExpr_Const(IRConst_U32(i*8));
      break;
    case Ity_I64:
      add = Iop_Add64;
      for(int i=0; i<4; i++) offsets[i] = IRExpr_Const(IRConst_U64(i*8));
      break;
    default:
      VG_(printf)("Unhandled type for address in loadShadowMemory.\n");
      tl_assert(False);
      break;
  }
  // load via Ist_Dirty
  for(UChar i64Block=0; i64Block<n64Blocks; i64Block++){
    IRDirty* di = unsafeIRDirty_1_N(
      loadAddr[i64Block],
      0,
      fname, VG_(fnptr_to_fnentry)(fn),
      mkIRExprVec_1(IRExpr_Binop(add,addr,offsets[i64Block])));
    addStmtToIRSB(sb_out, IRStmt_Dirty(di));
  }
  // prepare IRExpr's that read from these
  IRExpr* loadAddr_RdTmp[4];
  for(int i=0; i<4; i++){
    if(loadAddr[i]!=IRTemp_INVALID)
      loadAddr_RdTmp[i] = IRExpr_RdTmp(loadAddr[i]);
    else
      loadAddr_RdTmp[i] = NULL;
  }
  // decode
  return convertFromInteger(loadAddr_RdTmp,type);
}


void storeShadowMemory(IRSB* sb_out, IRExpr* addr, IRExpr* expr, IRExpr* guard){
  IRType type = typeOfIRExpr(sb_out->tyenv,expr);
  UChar n64Blocks; // necessary number of stores of Ity_I64: 1, 2 or 4
  const char* fname; // name of function that Ist_Dirty will call
  void* fn; // pointer to function that Ist_Dirty will call
  switch(sizeofIRType(type)){
    case 1:  n64Blocks=1; fname="dg_Store_diff1"; fn=dg_Store_diff1; break;
    case 2:  n64Blocks=1; fname="dg_Store_diff2"; fn=dg_Store_diff2; break;
    case 4:  n64Blocks=1; fname="dg_Store_diff4"; fn=dg_Store_diff4; break;
    case 8:  n64Blocks=1; fname="dg_Store_diff8"; fn=dg_Store_diff8; break;
    case 16: n64Blocks=2; fname="dg_Store_diff8"; fn=dg_Store_diff8; break;
    case 32: n64Blocks=4; fname="dg_Store_diff8"; fn=dg_Store_diff8; break;
    default: tl_assert(False);
  }
  // convert to up to four expressions of type Ity_I64
  IRExpr* converted[4];
  convertToInteger(expr,converted,type);
  // find Iop and offsets to shift address
  IROp add;
  IRExpr* offsets[4];
  switch(typeOfIRExpr(sb_out->tyenv,addr)){
    case Ity_I32:
      add = Iop_Add32;
      for(int i=0; i<4; i++) offsets[i] = IRExpr_Const(IRConst_U32(i*8));
      break;
    case Ity_I64:
      add = Iop_Add64;
      for(int i=0; i<4; i++) offsets[i] = IRExpr_Const(IRConst_U64(i*8));
      break;
    default:
      VG_(printf)("Unhandled type for address in storeShadowMemory.\n");
      tl_assert(False);
      break;
  }
  // store via Ist_Dirty
  for(UChar i64Block=0; i64Block<n64Blocks; i64Block++){
    IRDirty* di = unsafeIRDirty_0_N(
      0,
      fname, VG_(fnptr_to_fnentry)(fn),
      mkIRExprVec_2(IRExpr_Binop(add,addr,offsets[i64Block]), converted[i64Block]));
    if(guard)
      di->guard = guard;
    addStmtToIRSB(sb_out, IRStmt_Dirty(di));
  }
}

/*! Print a double value during execution of the generated code, for debugging purposes.
 *  The Ist_Dirty calling this function is produced by dg_add_print_stmt.
 *  \param[in] tag - Tag that is also printed.
 *  \param[in] value - Printed value.
 */
static VG_REGPARM(0) void dg_Print_double(ULong tag, ULong value){ VG_(printf)("Value for %d : ", tag); VG_(printf)("%lf\n", *(double*)&value); }
static VG_REGPARM(0) void dg_Print_unsignedlong(ULong tag, ULong value){ VG_(printf)("Value for %d : ", tag); VG_(printf)("%p\n", (void*)value); }
static VG_REGPARM(0) void dg_Print_unsignedint(ULong tag, Int value){ VG_(printf)("Value for %d : ", tag); VG_(printf)("%p\n", (void*)value); }

/*! Debugging help. Add a dirty statement to IRSB that prints the value of expr whenever it is run.
 *  \param[in] tag - Tag of your choice, will be printed alongside.
 *  \param[in] sb_out - IRSB to which the dirty statement is added.
 *  \param[in] expr - Expression.
 */
void dg_add_print_stmt(ULong tag, IRSB* sb_out, IRExpr* expr){
  IRType type = typeOfIRExpr(sb_out->tyenv, expr);
  char* fname;
  void* fptr;
  IRExpr* expr_to_print;
  switch(type){
    case Ity_F64:
      fname = "dg_Print_double";
      fptr = dg_Print_double;
      expr_to_print = IRExpr_Unop(Iop_ReinterpF64asI64,expr);
      break;
    case Ity_I64:
      fname = "dg_Print_unsignedlong";
      fptr = dg_Print_unsignedlong;
      expr_to_print = expr;
      break;
    case Ity_I32:
      fname = "dg_Print_unsignedint";
      fptr = dg_Print_unsignedint;
      expr_to_print = expr;
      break;
    default:
      VG_(printf)("Bad type in dg_add_print_stmt.\n");
      return;
  }
  IRDirty* di = unsafeIRDirty_0_N(
        0,
        fname, VG_(fnptr_to_fnentry)(fptr),
        mkIRExprVec_2(IRExpr_Const(IRConst_U64(tag)), expr_to_print));
  addStmtToIRSB(sb_out, IRStmt_Dirty(di));
}
