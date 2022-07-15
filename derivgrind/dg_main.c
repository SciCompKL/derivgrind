
/*--------------------------------------------------------------------*/
/*--- DerivGrind: Forward-mode algorithmic               dg_main.c ---*/
/*--- differentiation using Valgrind.                              ---*/
/*--------------------------------------------------------------------*/

/*
   This file is part of DerivGrind, a tool performing forward-mode
   algorithmic differentiation of compiled programs implemented
   in the Valgrind framework.

   Copyright (C) 2022 Max Aehle
      max.aehle@scicomp.uni-kl.de

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
#include "derivgrind.h"

#include "dg_logical.h"

#ifndef __GNUC__
  #error "Only tested with GCC."
#endif

#if __x86_64__
#else
#define BUILD_32BIT
#endif

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

ShadowMap* my_sm = NULL; //!< Gives access to the shadow memory for the tangent variables.

static unsigned long stmt_counter = 0; //!< Can be used to tag dg_add_print_stmt outputs.

#define DEFAULT_ROUNDING IRExpr_Const(IRConst_U32(Irrm_NEAREST))

/*! Condition for writing out unknown expressions.
 */
#define UNWRAPPED_EXPRESSION_OUTPUT_FILTER False

static void dg_post_clo_init(void)
{
}


/*! Make zero constant of certain type.
 */
static IRExpr* mkIRConst_zero(IRType type){
  IRExpr* zeroF = IRExpr_Const(IRConst_F64(0.));
  IRExpr* zeroU = IRExpr_Const(IRConst_U64(0));
  switch(type){
    case Ity_INVALID: tl_assert(False); return NULL;
    case Ity_I1: return IRExpr_Const(IRConst_U1(0));
    case Ity_I8: return IRExpr_Const(IRConst_U8(0));
    case Ity_I16: return IRExpr_Const(IRConst_U16(0));
    case Ity_I32: return IRExpr_Const(IRConst_U32(0));
    case Ity_I64: return zeroU;
    case Ity_I128: return IRExpr_Const(IRConst_U128(0));
    case Ity_F16: return IRExpr_Binop(Iop_F64toF16,DEFAULT_ROUNDING,zeroF);
    case Ity_F32: return IRExpr_Const(IRConst_F32(0.));
    case Ity_F64: return zeroF;
    case Ity_D32: return IRExpr_Binop(Iop_F64toD32,DEFAULT_ROUNDING,zeroF);
    case Ity_D64: return IRExpr_Binop(Iop_F64toD64,DEFAULT_ROUNDING,zeroF);
    case Ity_D128: return IRExpr_Binop(Iop_F64toD128,DEFAULT_ROUNDING,zeroF);
    case Ity_F128: return IRExpr_Unop(Iop_F64toF128,zeroF);
    case Ity_V128: return IRExpr_Binop(Iop_64HLtoV128,zeroU,zeroU);
    case Ity_V256: return IRExpr_Qop(Iop_64x4toV256,zeroU,zeroU,zeroU,zeroU);
    default: tl_assert(False); return NULL;
  }
}

/*! \page loading_and_storing Loading and storing tangent values in memory
 *
 *  When translating
 *  - an Ist_Store or Ist_StoreG statement to store data in memory, or
 *  - an Iex_Load expression or Iex_LoadG statements to load data from memory,
 *
 *  we emit Ist_Dirty statements. When those are executed, the call the
 *  functions dg_Store_diffN, dg_Load_diffN which access the shadow memory.
 *
 *  These dg_Store_diffN and dg_Load_diffN functions can only accept and
 *  return 4- and 8-byte integers arguments. Therefore we have to "encode" all
 *  types, including floating-point and SIMD types, by up to four 8-byte
 *  integers (Ity_I64 on the VEX side, ULong on the C side).
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
 *  Not properly tested yet. Maybe error for big-endian storage order.
 *
 */

/*! Store an 8-byte tangent value in shadow memory. Invoked by an Ist_Dirty during execution
 *  of the generated code.
 *  \param[in] addr - Address of the variable.
 *  \param[in] derivative - Tangent value, reinterpreted as 8-byte integer.
 *  \param[in] size - Original size of the variable.
 */
static
VG_REGPARM(0) void dg_Store_diff(Addr addr, ULong derivative, UChar size){
  for(int i=0; i<size; i++){
    shadow_set_bits(my_sm,((SM_Addr)addr)+i,  *( ((U8*)&derivative) + i ));
  }
}

static VG_REGPARM(0) void dg_Store_diff1(Addr addr, ULong derivative){dg_Store_diff(addr,derivative,1);}
static VG_REGPARM(0) void dg_Store_diff2(Addr addr, ULong derivative){dg_Store_diff(addr,derivative,2);}
static VG_REGPARM(0) void dg_Store_diff4(Addr addr, ULong derivative){dg_Store_diff(addr,derivative,4);}
static VG_REGPARM(0) void dg_Store_diff8(Addr addr, ULong derivative){dg_Store_diff(addr,derivative,8);}

/*! Load a tangent value from shadow memory. Invokes by an Ist_Dirty during execution
 *  of the generated code.
 *  \param[in] addr - Address of the variable.
 *  \param[in] size - Original size of the variable
 *  \returns Tangent value, reinterpreted as 8-byte integer.
 */
static
VG_REGPARM(0) ULong dg_Load_diff(Addr addr, UChar size){
  ULong derivative=0;
  for(int i=0; i<size; i++){
    shadow_get_bits(my_sm,((SM_Addr)addr)+i, (U8*)&derivative+i);
  }
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

/*! Add VEX instructions to read the shadow memory at a given address.
 *  \param[in,out] sb_out - IRSB where VEX instruction can be added.
 *  \param[in] addr - Address to be read from.
 *  \param[in] type - Type of the variable stored at addr.
 *  \returns IRExpr that evaluates to the content of the shadow memory.
 */
static IRExpr* loadShadowMemory(IRSB* sb_out, IRExpr* addr, IRType type){

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


/*! Add VEX instructions to store the shadow memory at a given address.
 *  \param[in,out] sb_out - IRSB where VEX instruction can be added.
 *  \param[in] addr - Address to store to.
 *  \param[in] expr - Expression whose value is to be stored.
 *  \param[in] guard - Store guard, can be NULL.
 */
static void storeShadowMemory(IRSB* sb_out, IRExpr* addr, IRExpr* expr, IRExpr* guard){
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
static
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

#include <VEX/priv/guest_generic_x87.h>
/*! React to gdb monitor commands.
 */
static
Bool dg_handle_gdb_monitor_command(ThreadId tid, HChar* req){
  HChar s[VG_(strlen)(req)+1]; //!< copy of req for strtok_r
  VG_(strcpy)(s, req);
  HChar* ssaveptr; //!< internal state of strtok_r

  const HChar commands[] = "help get set fget fset lget lset"; //!< list of possible commands
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
        "  get  <addr>       - Prints derivative of double\n"
        "  set  <addr> <val> - Sets derivative of double\n"
        "  fget <addr>       - Prints derivative of float\n"
        "  fset <addr> <val> - Sets derivative of float\n"
        "  lget <addr>       - Prints derivative of x87 double extended\n"
        "  lset <addr> <val> - Sets derivative of x87 double extended\n"
      );
      return True;
    case 1: case 3: case 5: { // get, fget, lget
      HChar* address_str = VG_(strtok_r)(NULL, " ", &ssaveptr);
      Addr address;
      if(!VG_(parse_Addr)(&address_str, &address)){
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
      union {char l[10]; double d; float f;} derivative;
      for(int i=0; i<size; i++){
        shadow_get_bits(my_sm,(SM_Addr)address+i, (U8*)&derivative+i);
      }
      switch(key){
        case 1:
          VG_(gdb_printf)("Derivative: %.16lf\n", derivative.d);
          break;
        case 3:
          VG_(gdb_printf)("Derivative: %.9f\n", derivative.f);
          break;
        case 5: {
          // convert x87 double extended to 64-bit double
          // so we can use the ordinary I/O.
          double tmp;
          convert_f80le_to_f64le(derivative.l,&tmp);
          VG_(gdb_printf)("Derivative: %.16lf\n", (double)tmp);
          break;
        }
      }
      return True;
    }
    case 2: case 4: case 6: { // set, fset, lset
      HChar* address_str = VG_(strtok_r)(NULL, " ", &ssaveptr);
      Addr address;
      if(!VG_(parse_Addr)(&address_str, &address)){
        VG_(gdb_printf)("Usage: set  <addr> <derivative>\n"
                        "       fset <addr> <derivative>\n"
                        "       lset <addr> <derivative>\n");
        return False;
      }
      HChar* derivative_str = VG_(strtok_r)(NULL, " ", &ssaveptr);
      union {char l[10]; double d; float f;} derivative;
      derivative.d = VG_(strtod)(derivative_str, NULL);
      int size;
      switch(key){
        case 2: size = 8; break;
        case 4: size = 4; derivative.f = (float) derivative.d; break;
        case 6: {
          // read as ordinary double and convert to x87 double extended
          // so we can use the ordinary I/O
          size = 10;
          double tmp = derivative.d;
          convert_f64le_to_f80le(&tmp,derivative.l);
          break;
        }
      }
      for(int i=0; i<size; i++){
        shadow_set_bits( my_sm,(SM_Addr)address+i, *( (U8*)&derivative+i ) );
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
    for(UWord i=0; i<size; i++){
      shadow_get_bits(my_sm,(SM_Addr)addr+i, (U8*)daddr+i);
    }
    *ret = 1;
    return True;
  } else if(arg[0]==VG_USERREQ__SET_DERIVATIVE) {
    void* addr = (void*) arg[1];
    void* daddr = (void*) arg[2];
    UWord size = arg[3];
    for(UWord i=0; i<size; i++){
      shadow_set_bits(my_sm,(SM_Addr)addr+i, *((U8*)daddr+i));
    }
    *ret = 1;
    return True;
  } else {
    VG_(printf)("Unhandled user request.\n");
    return True;
  }
}

/*! \struct DiffEnv
 *  Data required for differentiation, is passed to differentiate_expr.
 */
typedef struct {
  /*! Shadow offset for indices of temporaries.
   */
  IRTemp t_offset;
  /*! Layout argument to dg_instrument.
   *  layout->total_sizeB is the shadow offset for register indices. */
  const VexGuestLayout* layout;
  /*! Add helper statements to this IRSB.
   */
  IRSB* sb_out;
} DiffEnv;

static IRExpr* differentiate_or_zero(IRExpr*, DiffEnv, Bool, const char*);

// Some valid pieces of VEX IR cannot be translated back to machine code by
// Valgrind, but end up with an "ISEL" error. Therefore we sometimes need
// workarounds using convertToF64 and convertFromF64.
/*! Convert F32 and F64 expressions to F64.
 *  \param[in] expr - F32 or F64 expression.
 *  \param[in] diffenv - Differentiation environment.
 *  \param[out] originaltype - Original type is stored here so convertFromF64 can go back.
 *  \return F64 expression.
 */
static IRExpr* convertToF64(IRExpr* expr, DiffEnv diffenv, IRType* originaltype){
  *originaltype = typeOfIRExpr(diffenv.sb_out->tyenv, expr);
  switch(*originaltype){
    case Ity_F64: return expr;
    case Ity_F32: return IRExpr_Unop(Iop_F32toF64, expr);
    default: VG_(printf)("Bad type in convertToF64.\n"); tl_assert(False); return NULL;
  }
}
/*! Convert F64 expressions to F32 or F64.
 *  \param[in] expr - F64 expression.
 *  \param[in] originaltype - Original type is stored here from convertToF64.
 *  \return F32 or F64 expression.
 */
static IRExpr* convertFromF64(IRExpr* expr, IRType originaltype){
  switch(originaltype){
    case Ity_F64: return expr;
    case Ity_F32: return IRExpr_Binop(Iop_F64toF32, IRExpr_Const(IRConst_U32(Irrm_ZERO)), expr);
    default: VG_(printf)("Bad type in convertFromF64.\n"); tl_assert(False); return NULL;
  }
}

/*! Differentiate an expression.
 *
 *  - For arithmetic expressions involving float or double variables, we
 *    use the respective differentiation rules. Not all kinds of operations
 *    have been implemented already.
 *  - For expressions that just copy bytes, we copy the respective shadow
 *    bytes.
 *  - Otherwise, we return NULL. The function differentiate_or_zero has
 *    been created to return zero bytes and possibly output a warning.
 *
 *  The function might add helper statements to diffenv.sb_out.
 *
 *  \param[in] ex - Expression.
 *  \param[in,out] diffenv - Additional data necessary for differentiation.
 *  \returns Differentiated expression or NULL.
 */
static
IRExpr* differentiate_expr(IRExpr const* ex, DiffEnv diffenv ){
  if(ex->tag==Iex_Qop){
    IRQop* rex = ex->Iex.Qop.details;
    IRExpr* arg1 = rex->arg1;
    IRExpr* arg2 = rex->arg2;
    IRExpr* arg3 = rex->arg3;
    IRExpr* arg4 = rex->arg4;
    IRExpr* d2 = differentiate_expr(arg2,diffenv);
    IRExpr* d3 = differentiate_expr(arg3,diffenv);
    IRExpr* d4 = differentiate_expr(arg4,diffenv);
    if(d2==NULL || d3==NULL || d4==NULL) return NULL;
    switch(rex->op){
      /*! Define derivative of e.g. Iop_MSubF32.
       * \param addsub - Add or Sub
       * \param suffix - F32 or F64
       */
      #define DERIVATIVE_OF_MADDSUB_QOP(addsub, suffix) \
      case Iop_M##addsub##suffix: {\
        IRType originaltype; \
        IRExpr* arg2_f64 = convertToF64(arg2,diffenv,&originaltype); \
        IRExpr* arg3_f64 = convertToF64(arg3,diffenv,&originaltype); \
        IRExpr* d2_f64 = convertToF64(d2,diffenv,&originaltype); \
        IRExpr* d3_f64 = convertToF64(d3,diffenv,&originaltype); \
        IRExpr* d4_f64 = convertToF64(d4,diffenv,&originaltype); \
        IRExpr* res = IRExpr_Triop(Iop_##addsub##F64,arg1, \
          IRExpr_Triop(Iop_MulF64,arg1,d2_f64,arg3_f64), \
          IRExpr_Qop(Iop_M##addsub##F64,arg1,arg2_f64,d3_f64,d4_f64) \
         ); \
        return convertFromF64(res, originaltype); \
      }
      DERIVATIVE_OF_MADDSUB_QOP(Add, F64)
      DERIVATIVE_OF_MADDSUB_QOP(Sub, F64)
      DERIVATIVE_OF_MADDSUB_QOP(Add, F32)
      DERIVATIVE_OF_MADDSUB_QOP(Sub, F32)
      case Iop_64x4toV256: {
        IRExpr* d1 = differentiate_expr(arg2,diffenv);
        if(d1)
          return IRExpr_Qop(rex->op, d1,d2,d3,d4);
        else
          return NULL;
      }

      default:
        return NULL;
    }
  } else if(ex->tag==Iex_Triop){
    IRTriop* rex = ex->Iex.Triop.details;
    IRExpr* arg1 = rex->arg1;
    IRExpr* arg2 = rex->arg2;
    IRExpr* arg3 = rex->arg3;
    IRExpr* d2 = differentiate_expr(arg2,diffenv);
    IRExpr* d3 = differentiate_expr(arg3,diffenv);
    if(d2==NULL || d3==NULL) return NULL;
    /*! Define derivative for addition IROp.
     *  \param[in] suffix - Specifies addition IROp, e.g. F64 gives Iop_AddF64.
     */
    #define DERIVATIVE_OF_TRIOP_ADD(suffix) \
      case Iop_Add##suffix: \
        return IRExpr_Triop(Iop_Add##suffix, arg1, d2, d3);
    /*! Define derivative for subtraction IROp.
     */
    #define DERIVATIVE_OF_TRIOP_SUB(suffix) \
      case Iop_Sub##suffix: \
        return IRExpr_Triop(Iop_Sub##suffix, arg1, d2, d3);
    /*! Define derivative for multiplication IROp.
     */
    #define DERIVATIVE_OF_TRIOP_MUL(suffix) \
      case Iop_Mul##suffix: \
        return IRExpr_Triop(Iop_Add##suffix,arg1, \
          IRExpr_Triop(Iop_Mul##suffix, arg1, d2,arg3), \
          IRExpr_Triop(Iop_Mul##suffix, arg1, d3,arg2) \
        );
    /*! Define derivative for division IROp.
     */
    #define DERIVATIVE_OF_TRIOP_DIV(suffix) \
      case Iop_Div##suffix: \
        return IRExpr_Triop(Iop_Div##suffix,arg1, \
          IRExpr_Triop(Iop_Sub##suffix, arg1, \
            IRExpr_Triop(Iop_Mul##suffix, arg1, d2,arg3), \
            IRExpr_Triop(Iop_Mul##suffix, arg1, d3,arg2) \
          ), \
          IRExpr_Triop(Iop_Mul##suffix, arg1, arg3, arg3) \
        );
    /*! Define derivatives for four basic arithmetic operations.
     */
    #define DERIVATIVE_OF_BASICOP_ALL(suffix) \
      DERIVATIVE_OF_TRIOP_ADD(suffix) \
      DERIVATIVE_OF_TRIOP_SUB(suffix) \
      DERIVATIVE_OF_TRIOP_MUL(suffix) \
      DERIVATIVE_OF_TRIOP_DIV(suffix) \

    switch(rex->op){
      DERIVATIVE_OF_BASICOP_ALL(F64) // e.g. Iop_AddF64
      DERIVATIVE_OF_BASICOP_ALL(F32) // e.g. Iop_AddF32
      DERIVATIVE_OF_BASICOP_ALL(64Fx2) // e.g. Iop_Add64Fx2
      DERIVATIVE_OF_BASICOP_ALL(64Fx4) // e.g. Iop_Add64Fx4
      DERIVATIVE_OF_BASICOP_ALL(32Fx4) // e.g. Iop_Add32Fx4
      DERIVATIVE_OF_BASICOP_ALL(32Fx8) // e.g. Iop_Add32Fx8
      // there is no Iop_Div32Fx2
      DERIVATIVE_OF_TRIOP_ADD(32Fx2) DERIVATIVE_OF_TRIOP_SUB(32Fx2) DERIVATIVE_OF_TRIOP_MUL(32Fx2)
      case Iop_AtanF64: {
        IRExpr* fraction = IRExpr_Triop(Iop_DivF64,arg1,arg2,arg3);
        IRExpr* fraction_d = IRExpr_Triop(Iop_DivF64,arg1,
          IRExpr_Triop(Iop_SubF64, arg1,
            IRExpr_Triop(Iop_MulF64, arg1, d2,arg3),
            IRExpr_Triop(Iop_MulF64, arg1, d3,arg2)
          ),
          IRExpr_Triop(Iop_MulF64, arg1, arg3, arg3)
        );
        return IRExpr_Triop(Iop_DivF64,arg1,
          fraction_d,
          IRExpr_Triop(Iop_AddF64,arg1,
            IRExpr_Const(IRConst_F64(1.)),
            IRExpr_Triop(Iop_MulF64,arg1,fraction,fraction)
          )
        );
      }
      case Iop_ScaleF64:
        return IRExpr_Triop(Iop_ScaleF64,arg1,d2,arg3);
      case Iop_Yl2xF64:
        return IRExpr_Triop(Iop_AddF64,arg1,
          IRExpr_Triop(Iop_Yl2xF64,arg1,d2,arg3),
          IRExpr_Triop(Iop_DivF64,arg1,
            IRExpr_Triop(Iop_MulF64,arg1,arg2,d3),
            IRExpr_Triop(Iop_MulF64,arg1,
              IRExpr_Const(IRConst_F64(0.6931471805599453094172321214581)),
              arg3))
        );
      case Iop_Yl2xp1F64:
        return IRExpr_Triop(Iop_AddF64,arg1,
          IRExpr_Triop(Iop_Yl2xp1F64,arg1,d2,arg3),
          IRExpr_Triop(Iop_DivF64,arg1,
            IRExpr_Triop(Iop_MulF64,arg1,arg2,d3),
            IRExpr_Triop(Iop_MulF64,arg1,
              IRExpr_Const(IRConst_F64(0.6931471805599453094172321214581)),
              IRExpr_Triop(Iop_AddF64, arg1, arg3, IRExpr_Const(IRConst_F64(1.)))))
        );

      default:
        return NULL;
    }
  } else if(ex->tag==Iex_Binop) {
    IROp op = ex->Iex.Binop.op;
    IRExpr* arg1 = ex->Iex.Binop.arg1;
    IRExpr* arg2 = ex->Iex.Binop.arg2;
    IRExpr* d2 = differentiate_expr(arg2,diffenv);
    if(d2==NULL) return NULL;
    switch(op){
      /*! Handle logical "and", "or", "xor" on
       *  32, 64, 128 and 256 bit.
       *  \param[in] Op - And, Or or Xor
       *  \param[in] op - and, or or xor
       */
      #define DG_HANDLE_LOGICAL(Op, op) \
      case Iop_##Op##32: { \
        IRExpr* d1 = differentiate_expr(arg1,diffenv); \
        IRExpr* zero32 = IRExpr_Const(IRConst_U32(0)); \
        IRExpr* arg1_64 = IRExpr_Binop(Iop_32HLto64, zero32, arg1); \
        IRExpr* d1_64 = IRExpr_Binop(Iop_32HLto64, zero32, d1); \
        IRExpr* arg2_64 = IRExpr_Binop(Iop_32HLto64, zero32, arg2); \
        IRExpr* d2_64 = IRExpr_Binop(Iop_32HLto64, zero32, d2); \
        IRExpr* res = mkIRExprCCall(Ity_I64, \
          0, \
          "dg_logical_" #op "64", &dg_logical_##op##64, \
          mkIRExprVec_4(arg1_64, d1_64, arg2_64, d2_64) \
        ); \
        return IRExpr_Unop(Iop_64to32, res); \
      } \
      case Iop_##Op##64: { \
        IRExpr* d1 = differentiate_expr(arg1,diffenv); \
        IRExpr* res = mkIRExprCCall(Ity_I64, \
          0, \
          "dg_logical_" #op "64", &dg_logical_##op##64, \
          mkIRExprVec_4(arg1, d1, arg2, d2) \
        ); \
        return res; \
      } \
      case Iop_##Op##V128: { \
        IRExpr* d1 = differentiate_expr(arg1,diffenv); \
        IRExpr *res[2]; \
        for(int i=0; i<2; i++){ /* 0=low half, 1=high half */ \
          IROp selector = i==0? Iop_V128to64 : Iop_V128HIto64; \
          IRExpr* arg1_part = IRExpr_Unop(selector, arg1); \
          IRExpr* arg2_part = IRExpr_Unop(selector, arg2); \
          IRExpr* d1_part = IRExpr_Unop(selector, d1); \
          IRExpr* d2_part = IRExpr_Unop(selector, d2); \
          res[i] = mkIRExprCCall(Ity_I64, \
                0, \
                "dg_logical_" #op "64", &dg_logical_##op##64, \
                mkIRExprVec_4(arg1_part, d1_part, arg2_part, d2_part) \
             ); \
        } \
        return IRExpr_Binop(Iop_64HLtoV128, res[1], res[0]); \
      } \
      case Iop_##Op##V256: { \
        IRExpr* d1 = differentiate_expr(arg1,diffenv); \
        IRExpr *res[4]; \
        for(int i=0; i<4; i++){ /* 0=low quarter, ..., 3=high quarter */ \
          IROp selector; \
          switch(i){ \
            case 0: selector = Iop_V256to64_0; break; \
            case 1: selector = Iop_V256to64_1; break; \
            case 2: selector = Iop_V256to64_2; break; \
            case 3: selector = Iop_V256to64_3; break; \
          } \
          IRExpr* arg1_part = IRExpr_Unop(selector, arg1); \
          IRExpr* arg2_part = IRExpr_Unop(selector, arg2); \
          IRExpr* d1_part = IRExpr_Unop(selector, d1); \
          IRExpr* d2_part = IRExpr_Unop(selector, d2); \
          res[i] = mkIRExprCCall(Ity_I64, \
                0, \
                "dg_logical_" #op "64", &dg_logical_##op##64, \
                mkIRExprVec_4(arg1_part, d1_part, arg2_part, d2_part) \
             ); \
        } \
        return IRExpr_Qop(Iop_64x4toV256, res[3], res[2], res[1], res[0]); \
      } 
      DG_HANDLE_LOGICAL(And, and)
      DG_HANDLE_LOGICAL(Or, or)
      DG_HANDLE_LOGICAL(Xor, xor)

      /*! Define derivative for square root IROp.
       */
      #define DERIVATIVE_OF_BINOP_SQRT(suffix, consttwo) \
        case Iop_Sqrt##suffix: { \
          IRExpr* numerator = d2; \
          IRExpr* denominator =  IRExpr_Triop(Iop_Mul##suffix, arg1, consttwo, IRExpr_Binop(Iop_Sqrt##suffix, arg1, arg2) ); \
          return IRExpr_Triop(Iop_Div##suffix, arg1, numerator, denominator); \
        }
      DERIVATIVE_OF_BINOP_SQRT(F64, IRExpr_Const(IRConst_F64(2.)))
      DERIVATIVE_OF_BINOP_SQRT(F32, IRExpr_Const(IRConst_F32(2.)))
      DERIVATIVE_OF_BINOP_SQRT(64Fx2, IRExpr_Binop(Iop_64HLtoV128,  \
        IRExpr_Unop(Iop_ReinterpF64asI64,IRExpr_Const(IRConst_F64(2.))), \
        IRExpr_Unop(Iop_ReinterpF64asI64,IRExpr_Const(IRConst_F64(2.))) ))
      DERIVATIVE_OF_BINOP_SQRT(64Fx4, IRExpr_Qop(Iop_64x4toV256,  \
        IRExpr_Unop(Iop_ReinterpF64asI64,IRExpr_Const(IRConst_F64(2.))), \
        IRExpr_Unop(Iop_ReinterpF64asI64,IRExpr_Const(IRConst_F64(2.))), \
        IRExpr_Unop(Iop_ReinterpF64asI64,IRExpr_Const(IRConst_F64(2.))), \
        IRExpr_Unop(Iop_ReinterpF64asI64,IRExpr_Const(IRConst_F64(2.))) ))
      DERIVATIVE_OF_BINOP_SQRT(32Fx4, IRExpr_Binop(Iop_64HLtoV128,  \
        IRExpr_Binop(Iop_32HLto64, \
          IRExpr_Unop(Iop_ReinterpF32asI32,IRExpr_Const(IRConst_F32(2.))), \
          IRExpr_Unop(Iop_ReinterpF32asI32,IRExpr_Const(IRConst_F32(2.))) ), \
        IRExpr_Binop(Iop_32HLto64,
          IRExpr_Unop(Iop_ReinterpF32asI32,IRExpr_Const(IRConst_F32(2.))), \
          IRExpr_Unop(Iop_ReinterpF32asI32,IRExpr_Const(IRConst_F32(2.))) ) ))
      DERIVATIVE_OF_BINOP_SQRT(32Fx8, IRExpr_Qop(Iop_64x4toV256,  \
        IRExpr_Binop(Iop_32HLto64, \
          IRExpr_Unop(Iop_ReinterpF32asI32,IRExpr_Const(IRConst_F32(2.))), \
          IRExpr_Unop(Iop_ReinterpF32asI32,IRExpr_Const(IRConst_F32(2.))) ), \
        IRExpr_Binop(Iop_32HLto64, \
          IRExpr_Unop(Iop_ReinterpF32asI32,IRExpr_Const(IRConst_F32(2.))), \
          IRExpr_Unop(Iop_ReinterpF32asI32,IRExpr_Const(IRConst_F32(2.))) ), \
        IRExpr_Binop(Iop_32HLto64, \
          IRExpr_Unop(Iop_ReinterpF32asI32,IRExpr_Const(IRConst_F32(2.))), \
          IRExpr_Unop(Iop_ReinterpF32asI32,IRExpr_Const(IRConst_F32(2.))) ), \
        IRExpr_Binop(Iop_32HLto64,
          IRExpr_Unop(Iop_ReinterpF32asI32,IRExpr_Const(IRConst_F32(2.))), \
          IRExpr_Unop(Iop_ReinterpF32asI32,IRExpr_Const(IRConst_F32(2.))) ) ))


      case Iop_F64toF32: {
        return IRExpr_Binop(Iop_F64toF32, arg1, d2);
      }
      case Iop_2xm1F64:
        return IRExpr_Triop(Iop_MulF64, arg1,
          IRExpr_Triop(Iop_MulF64, arg1,
            IRExpr_Const(IRConst_F64(0.6931471805599453094172321214581)),
            d2),
          IRExpr_Triop(Iop_AddF64, arg1,
            IRExpr_Const(IRConst_F64(1.)),
            IRExpr_Binop(Iop_2xm1F64,arg1,arg2)
        ));
      case Iop_Mul64F0x2: {
        IRExpr* d1 = differentiate_or_zero(arg1,diffenv,False,"");
        return IRExpr_Binop(Iop_Add64F0x2,
          IRExpr_Binop(Iop_Mul64F0x2,d1,arg2), // the order is important here
          IRExpr_Binop(Iop_Mul64F0x2,arg1,d2)
        );
      }
      case Iop_Mul32F0x4: {
        IRExpr* d1 = differentiate_or_zero(arg1,diffenv,False,"");
        return IRExpr_Binop(Iop_Add32F0x4,
          IRExpr_Binop(Iop_Mul32F0x4,d1,arg2), // the order is important here
          IRExpr_Binop(Iop_Mul32F0x4,arg1,d2)
        );
      }
      case Iop_Div64F0x2: {
        IRExpr* d1 = differentiate_or_zero(arg1,diffenv,False,"");
        return IRExpr_Binop(Iop_Div64F0x2,
          IRExpr_Binop(Iop_Sub64F0x2,
            IRExpr_Binop(Iop_Mul64F0x2,d1,arg2), // the order is important here
            IRExpr_Binop(Iop_Mul64F0x2,arg1,d2)
          ),
          IRExpr_Binop(Iop_Mul64F0x2,arg2,arg2)
        );
      }
      case Iop_Div32F0x4: {
        IRExpr* d1 = differentiate_or_zero(arg1,diffenv,False,"");
        return IRExpr_Binop(Iop_Div32F0x4,
          IRExpr_Binop(Iop_Sub32F0x4,
            IRExpr_Binop(Iop_Mul32F0x4,d1,arg2), // the order is important here
            IRExpr_Binop(Iop_Mul32F0x4,arg1,d2)
          ),
          IRExpr_Binop(Iop_Mul32F0x4,arg2,arg2)
        );
      }
      case Iop_Min64F0x2: {
        IRExpr* d1 = differentiate_or_zero(arg1,diffenv,False,"");
        IRExpr* d1_lo = IRExpr_Unop(Iop_V128to64, d1);
        IRExpr* d2_lo = IRExpr_Unop(Iop_V128to64, d2);
        IRExpr* d1_hi = IRExpr_Unop(Iop_V128HIto64, d1);
        IRExpr* arg1_lo_f = IRExpr_Unop(Iop_ReinterpI64asF64, IRExpr_Unop(Iop_V128to64, arg1));
        IRExpr* arg2_lo_f = IRExpr_Unop(Iop_ReinterpI64asF64, IRExpr_Unop(Iop_V128to64, arg2));
        IRExpr* cond = IRExpr_Binop(Iop_CmpF64, arg1_lo_f, arg2_lo_f);
        return IRExpr_Binop(Iop_64HLtoV128,
          d1_hi,
          IRExpr_ITE(IRExpr_Unop(Iop_32to1,cond),d1_lo,d2_lo)
        );
      }
      // the following operations produce an F64 zero derivative
      case Iop_I64StoF64:
      case Iop_I64UtoF64:
      case Iop_RoundF64toInt:
        return mkIRConst_zero(Ity_F64);
      // the following operations produce an F32 zero derivative
      case Iop_I64StoF32:
      case Iop_I64UtoF32:
      case Iop_I32StoF32:
      case Iop_I32UtoF32:
        return mkIRConst_zero(Ity_F32);
      // the following operations only "transport", so they are applied
      // on the derivatives in the same way as for primal values
      case Iop_64HLto128: case Iop_32HLto64:
      case Iop_16HLto32: case Iop_8HLto16:
      case Iop_64HLtoV128: case Iop_V128HLtoV256:
      case Iop_Add64F0x2: case Iop_Sub64F0x2:
      case Iop_Add32F0x4: case Iop_Sub32F0x4:
      case Iop_SetV128lo32: case Iop_SetV128lo64:
      case Iop_InterleaveHI8x16: case Iop_InterleaveHI16x8:
      case Iop_InterleaveHI32x4: case Iop_InterleaveHI64x2:
      case Iop_InterleaveLO8x16: case Iop_InterleaveLO16x8:
      case Iop_InterleaveLO32x4: case Iop_InterleaveLO64x2:
        {
          IRExpr* d1 = differentiate_or_zero(arg1,diffenv,False,"");
          if(d1)
            return IRExpr_Binop(op, d1,d2);
          else
            return NULL;
        }
      default:
        return NULL;
    }
  } else if(ex->tag==Iex_Unop) {
    IROp op = ex->Iex.Unop.op;
    IRExpr* arg = ex->Iex.Unop.arg;
    IRExpr* d = differentiate_expr(arg,diffenv);
    if(d==NULL) return NULL;
    switch(op){
      case Iop_AbsF64: {
        // If arg >= 0, we get Ircr_GT or Ircr_EQ, thus the Iop_32to1 gives a 0 bit.
        IRExpr* cond = IRExpr_Binop(Iop_CmpF64, arg, IRExpr_Const(IRConst_F64(0.)));
        IRExpr* minus_d = IRExpr_Unop(Iop_NegF64, d);
        return IRExpr_ITE(IRExpr_Unop(Iop_32to1,cond), minus_d, d);
      }
      case Iop_AbsF32: {
        IRExpr* cond = IRExpr_Binop(Iop_CmpF32, arg, IRExpr_Const(IRConst_F32(0.)));
        IRExpr* minus_d = IRExpr_Unop(Iop_NegF32, d);
        return IRExpr_ITE(IRExpr_Unop(Iop_32to1,cond), minus_d, d);
      }

      /*! Define derivative for square root IROp.
       */
      #define DERIVATIVE_OF_UNOP_SQRT(suffix,consttwo) \
        case Iop_Sqrt##suffix: { \
          IRExpr* numerator = d; \
          IRExpr* consttwo_32 = IRExpr_Unop(Iop_ReinterpF32asI32, IRExpr_Const(IRConst_F32(2.))); \
          IRExpr* consttwo_32x2 = IRExpr_Binop(Iop_32HLto64, consttwo_32,consttwo_32); \
          IRExpr* consttwo_64 = IRExpr_Unop(Iop_ReinterpF64asI64, IRExpr_Const(IRConst_F64(2.))); \
          IRExpr* default_rounding = IRExpr_Const(IRConst_U32(Irrm_ZERO)); \
          IRExpr* denominator =  IRExpr_Triop(Iop_Mul##suffix, default_rounding, consttwo, IRExpr_Unop(Iop_Sqrt##suffix, arg) ); \
          return IRExpr_Triop(Iop_Div##suffix, default_rounding, numerator, denominator); \
        }
      DERIVATIVE_OF_UNOP_SQRT(64Fx2, IRExpr_Binop(Iop_64HLtoV128,consttwo_64,consttwo_64))
      DERIVATIVE_OF_UNOP_SQRT(64Fx4, IRExpr_Qop(Iop_64x4toV256,consttwo_64,consttwo_64,consttwo_64,consttwo_64))
      DERIVATIVE_OF_UNOP_SQRT(32Fx4, IRExpr_Binop(Iop_64HLtoV128, consttwo_32x2,consttwo_32x2))
      DERIVATIVE_OF_UNOP_SQRT(32Fx8, IRExpr_Qop(Iop_64x4toV256,consttwo_32x2,consttwo_32x2,consttwo_32x2,consttwo_32x2))

      case Iop_Sqrt64F0x2: {
        IRExpr* numerator = d;
        IRExpr* consttwo_f64 = IRExpr_Const(IRConst_F64(2.0));
        IRExpr* consttwo_i64 = IRExpr_Unop(Iop_ReinterpF64asI64, consttwo_f64);
        IRExpr* consttwo_v128 = IRExpr_Binop(Iop_64HLtoV128, consttwo_i64, consttwo_i64);
        IRExpr* denominator =  IRExpr_Binop(Iop_Mul64F0x2, consttwo_v128, IRExpr_Unop(Iop_Sqrt64F0x2, arg) );
        return IRExpr_Binop(Iop_Div64F0x2, numerator, denominator);
        // fortunately, this is also right on the upper half of the V128
      }
      case Iop_Sqrt32F0x4: {
        IRExpr* numerator = d;
        IRExpr* consttwo_f32 = IRExpr_Const(IRConst_F32(2.0));
        IRExpr* consttwo_i32 = IRExpr_Unop(Iop_ReinterpF32asI32, consttwo_f32);
        IRExpr* consttwo_i64 = IRExpr_Binop(Iop_32HLto64, consttwo_i32, consttwo_i32);
        IRExpr* consttwo_v128 = IRExpr_Binop(Iop_64HLtoV128, consttwo_i64, consttwo_i64);
        IRExpr* denominator =  IRExpr_Binop(Iop_Mul32F0x4, consttwo_v128, IRExpr_Unop(Iop_Sqrt32F0x4, arg) );
        return IRExpr_Binop(Iop_Div32F0x4, numerator, denominator);
        // fortunately, this is also right on the upper 3/4 of the V128
      }
      case Iop_I32StoF64:
      case Iop_I32UtoF64:
        return IRExpr_Const(IRConst_F64(0.));
      // the following instructions are simply applied to the derivative as well
      case Iop_F32toF64:
      case Iop_ReinterpI64asF64: case Iop_ReinterpF64asI64:
      case Iop_ReinterpI32asF32: case Iop_ReinterpF32asI32:
      case Iop_NegF64: case Iop_NegF32:
      case Iop_64to8: case Iop_32to8: case Iop_64to16:
      case Iop_16to8: case Iop_16HIto8:
      case Iop_32to16: case Iop_32HIto16:
      case Iop_64to32: case Iop_64HIto32:
      case Iop_V128to64: case Iop_V128HIto64:
      case Iop_V256toV128_0: case Iop_V256toV128_1:
      case Iop_8Uto16: case Iop_8Uto32: case Iop_8Uto64:
      case Iop_16Uto32: case Iop_16Uto64:
      case Iop_32Uto64:
      case Iop_8Sto16: case Iop_8Sto32: case Iop_8Sto64:
      case Iop_16Sto32: case Iop_16Sto64:
      case Iop_32Sto64:
      case Iop_ZeroHI64ofV128: case Iop_ZeroHI96ofV128:
      case Iop_ZeroHI112ofV128: case Iop_ZeroHI120ofV128:
      case Iop_64UtoV128: case Iop_32UtoV128:
      case Iop_V128to32:
      case Iop_128HIto64:
      case Iop_128to64:
        return IRExpr_Unop(op, d);

      default:
        return NULL;
    }
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
    return IRExpr_RdTmp( ex->Iex.RdTmp.tmp + diffenv.t_offset );
  } else if(ex->tag==Iex_Get) {
    return IRExpr_Get(ex->Iex.Get.offset+diffenv.layout->total_sizeB,ex->Iex.Get.ty);
  } else if(ex->tag==Iex_GetI) {
    IRRegArray* descr = ex->Iex.GetI.descr;
    IRExpr* ix = ex->Iex.GetI.ix;
    Int bias = ex->Iex.GetI.bias;
    IRRegArray* descr_diff = mkIRRegArray(descr->base+diffenv.layout->total_sizeB,descr->elemTy,descr->nElems);
    return IRExpr_GetI(descr_diff,ix,bias+diffenv.layout->total_sizeB);
  } else if (ex->tag==Iex_Load) {
    return loadShadowMemory(diffenv.sb_out,ex->Iex.Load.addr,ex->Iex.Load.ty);
  } else {
    return NULL;
  }
}


/*! Compute derivative. If this fails, return a zero expression.
 *  \param[in] expr - expression to be differentiated
 *  \param[in] diffenv - Additional data necessary for differentiation.
 *  \param[in] warn - If true, a warning message will be printed if differentiation fails.
 *  \param[in] operation - Word for how the derivative is used, e.g. 'WrTmp' or 'Store'.
 * \returns Differentiated expression or zero expression.
 */
static
IRExpr* differentiate_or_zero(IRExpr* expr, DiffEnv diffenv, Bool warn, const char* operation){
  IRExpr* diff = differentiate_expr(expr,diffenv);
  if(diff){
    return diff;
  } else {
    if(warn){
      VG_(printf)("Warning: Expression\n");
      ppIRExpr(expr);
      VG_(printf)("could not be differentiated, %s'ing zero instead.\n\n", operation);
    }
    return mkIRConst_zero(typeOfIRExpr(diffenv.sb_out->tyenv,expr));
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
ULong x86g_amd64g_diff_dirtyhelper_loadF80le ( Addr addrU )
{
   ULong f64, f128[2];
   f128[0] = dg_Load_diff8(addrU);
   f128[1] = dg_Load_diff2(addrU+8);
   convert_f80le_to_f64le ( (UChar*)f128, (UChar*)&f64 );
   return f64;
}
/*! Dirtyhelper for the extra AD logic to dirty calls to
 *  x86g_dirtyhelper_storeF80le / amd64g_dirtyhelper_storeF80le.
 *
 *  It's very similar, but writes to shadow memory instead
 *  of guest memory.
 */
void x86g_amd64g_diff_dirtyhelper_storeF80le ( Addr addrU, ULong f64 )
{
   ULong f128[2];
   convert_f64le_to_f80le( (UChar*)&f64, (UChar*)f128 );
   dg_Store_diff8(addrU,f128[0]);
   dg_Store_diff2(addrU+8,f128[1]);
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
  // append the "gradient temporaries" to the "value temporaries",
  // doubling the number of temporaries
  diffenv.t_offset = sb_in->tyenv->types_used;
  for(IRTemp t=0; t<diffenv.t_offset; t++){
    newIRTemp(sb_out->tyenv, sb_in->tyenv->types[t]);
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
    stmt_counter++;
    IRStmt* st_orig = sb_in->stmts[i]; // will be added to sb_out in the end
    const IRStmt* st = st_orig; // const version for differentiation
    //VG_(printf)("next stmt %d :",stmt_counter); ppIRStmt(st); VG_(printf)("\n");
    if(st->tag==Ist_WrTmp) {
      IRType type = sb_in->tyenv->types[st->Ist.WrTmp.tmp];
      IRExpr* differentiated_expr = differentiate_or_zero(st->Ist.WrTmp.data, diffenv,UNWRAPPED_EXPRESSION_OUTPUT_FILTER,"WrTmp");
      IRStmt* sp = IRStmt_WrTmp(st->Ist.WrTmp.tmp+diffenv.t_offset, differentiated_expr);
      addStmtToIRSB(sb_out, sp);
      addStmtToIRSB(sb_out, st_orig);
    } else if(st->tag==Ist_Put) {
      IRType type = typeOfIRExpr(sb_in->tyenv,st->Ist.Put.data);
      IRExpr* differentiated_expr = differentiate_or_zero(st->Ist.Put.data, diffenv,UNWRAPPED_EXPRESSION_OUTPUT_FILTER,"Put");
      IRStmt* sp = IRStmt_Put(st->Ist.Put.offset + diffenv.layout->total_sizeB, differentiated_expr);
      addStmtToIRSB(sb_out, sp);
      addStmtToIRSB(sb_out, st_orig);
    } else if(st->tag==Ist_PutI) {
      IRType type = typeOfIRExpr(sb_in->tyenv,st->Ist.PutI.details->data);
      IRExpr* differentiated_expr = differentiate_or_zero(st->Ist.PutI.details->data, diffenv,UNWRAPPED_EXPRESSION_OUTPUT_FILTER,"PutI");
      IRRegArray* descr = st->Ist.PutI.details->descr;
      IRRegArray* descr_diff = mkIRRegArray(descr->base+diffenv.layout->total_sizeB, descr->elemTy, descr->nElems);
      Int bias = st->Ist.PutI.details->bias;
      IRExpr* ix = st->Ist.PutI.details->ix;
      IRStmt* sp = IRStmt_PutI(mkIRPutI(descr_diff,ix,bias+diffenv.layout->total_sizeB,differentiated_expr));
      addStmtToIRSB(sb_out, sp);
      addStmtToIRSB(sb_out, st_orig);
    } else if(st->tag==Ist_Store){
      IRType type = typeOfIRExpr(sb_in->tyenv,st->Ist.Store.data);
      IRExpr* differentiated_expr = differentiate_or_zero(st->Ist.Store.data, diffenv, UNWRAPPED_EXPRESSION_OUTPUT_FILTER,"Store");
      storeShadowMemory(diffenv.sb_out,st->Ist.Store.addr,differentiated_expr,NULL);
      addStmtToIRSB(sb_out, st_orig);
    } else if(st->tag==Ist_StoreG){
      IRStoreG* det = st->Ist.StoreG.details;
      IRType type = typeOfIRExpr(sb_in->tyenv,det->data);
      IRExpr* differentiated_expr = differentiate_or_zero(det->data, diffenv, UNWRAPPED_EXPRESSION_OUTPUT_FILTER,"StoreG");
      storeShadowMemory(diffenv.sb_out,det->addr,differentiated_expr,det->guard);
      addStmtToIRSB(sb_out, st_orig);
    } else if(st->tag==Ist_LoadG) {
      IRLoadG* det = st->Ist.LoadG.details;
      // discard det->cvt; extra bits for widening should
      // never be interpreted as derivative information
      IRType type = sb_in->tyenv->types[det->dst];
      // differentiate alternative value
      IRExpr* differentiated_expr_alt = differentiate_or_zero(det->alt,diffenv,UNWRAPPED_EXPRESSION_OUTPUT_FILTER,"alternative-LoadG");
      // depending on the guard, copy either the derivative stored
      // in shadow memory, or the derivative of the alternative value,
      // to the shadow temporary.
      addStmtToIRSB(diffenv.sb_out, IRStmt_WrTmp(det->dst+diffenv.t_offset,
        IRExpr_ITE(det->guard,loadShadowMemory(diffenv.sb_out,det->addr,type),differentiated_expr_alt)
      ));
      addStmtToIRSB(sb_out, st_orig);
    } else if(st->tag==Ist_CAS) {

      IRCAS* det = st->Ist.CAS.details;
      IRType type = typeOfIRExpr(sb_out->tyenv,det->expdLo);
      Bool double_element = (det->expdHi!=NULL);

      // add original statement before AD treatment, because we need to know
      // if the CAS succeeded
      addStmtToIRSB(sb_out, st_orig);

      // Now we will add a couple more statements. Note that the complete
      // translation of the Ist_CAS is not atomic any more, so it's
      // possible that we create a race condition here.
      // This issue also exists in do_shadow_CAS in mc_translate.c.
      // There, the comment states that because Valgrind runs only one
      // thread at a time and there are no context switches within a
      // single IRSB, this is not a problem.

      // Find addresses of Hi and Lo part.
      IRExpr* offset; // offset between Hi and Lo part of addr
      IRExpr* addr_Lo; // one of addr_Lo, addr_Hi is det->addr,
      IRExpr* addr_Hi; // the other one is det->addr + offset
      IROp add; // operation to add addresses
      switch(typeOfIRExpr(diffenv.sb_out->tyenv,det->addr)){
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
          addr_Lo = det->addr;
          addr_Hi = IRExpr_Binop(add,det->addr,offset);
        } else {
          addr_Lo = det->addr;
          addr_Hi = NULL;
        }
      } else { // Iend_BE
        if(double_element){
          addr_Lo = IRExpr_Binop(add,det->addr,offset);
          addr_Hi = det->addr;
        } else {
          addr_Lo = det->addr;
          addr_Hi = NULL;
        }
      }

      // Load derivatives of oldLo.
      addStmtToIRSB(diffenv.sb_out, IRStmt_WrTmp(
        det->oldLo + diffenv.t_offset, loadShadowMemory(diffenv.sb_out,addr_Lo,type)));
      // Possibly load derivatives of oldHi.
      if(double_element){
          addStmtToIRSB(diffenv.sb_out, IRStmt_WrTmp(
            det->oldHi + diffenv.t_offset, loadShadowMemory(diffenv.sb_out,addr_Hi,type)));
      }
      // Find out if CAS succeeded.
      IROp cmp;
      switch(type){
        case Ity_I8: cmp = Iop_CmpEQ8; break;
        case Ity_I16: cmp = Iop_CmpEQ16; break;
        case Ity_I32: cmp = Iop_CmpEQ32; break;
        case Ity_I64: cmp = Iop_CmpEQ64; break;
        default: VG_(printf)("Unhandled type in translation of Ist_CAS.\n"); tl_assert(False); break;
      }
      IRTemp cas_succeeded = newIRTemp(sb_out->tyenv, Ity_I1);
      addStmtToIRSB(sb_out, IRStmt_WrTmp(cas_succeeded,
        IRExpr_Unop(Iop_32to1, IRExpr_Unop(Iop_1Uto32,
          IRExpr_Binop(cmp,IRExpr_RdTmp(det->oldLo), det->expdLo)
      ))));
      // Guarded write of Lo part to shadow memory.
      IRExpr* differentiated_expdLo = differentiate_or_zero(det->expdLo,diffenv,False,"");
      storeShadowMemory(sb_out,addr_Lo,differentiated_expdLo,IRExpr_RdTmp(cas_succeeded));
      // Possibly guarded write of Hi part to shadow memory.
      if(double_element){
        IRExpr* differentiated_expdHi =  differentiate_or_zero(det->expdHi,diffenv,False,"");
        storeShadowMemory(sb_out,addr_Hi,differentiated_expdHi,IRExpr_RdTmp(cas_succeeded));
      }
    } else if(st->tag==Ist_LLSC) {
      addStmtToIRSB(sb_out, st_orig);
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
        IRExpr* differentiated_expr = differentiate_or_zero(expr,diffenv,False,"");
        IRDirty* dd = unsafeIRDirty_0_N(
              0,
              "x86g_amd64g_diff_dirtyhelper_storeF80le",
              &x86g_amd64g_diff_dirtyhelper_storeF80le,
              mkIRExprVec_2(addr, differentiated_expr));
        addStmtToIRSB(sb_out, IRStmt_Dirty(dd));
        addStmtToIRSB(sb_out, st_orig);
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
              t + diffenv.t_offset,
              0,
              "x86g_amd64g_diff_dirtyhelper_loadF80le",
              &x86g_amd64g_diff_dirtyhelper_loadF80le,
              mkIRExprVec_1(addr));
        addStmtToIRSB(sb_out, IRStmt_Dirty(dd));
        addStmtToIRSB(sb_out, st_orig);
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
          IRTemp shadow_tmp = det->tmp+diffenv.t_offset;
          IRType type = typeOfIRTemp(diffenv.sb_out->tyenv,det->tmp);
          IRExpr* zero = mkIRConst_zero(type);
          addStmtToIRSB(sb_out,IRStmt_WrTmp(shadow_tmp,zero));
        }
        addStmtToIRSB(sb_out, st_orig);

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
      addStmtToIRSB(sb_out, st_orig);
      // no relevance for any tool, do nothing
    } else if(st->tag==Ist_Exit || st->tag==Ist_MBE) {
      addStmtToIRSB(sb_out, st_orig);
      // no relevance for AD, do nothing
    } else {
      tl_assert(False);
    }


  }

   return sb_out;
}

static void dg_fini(Int exitcode)
{
  shadow_destroy_map(my_sm);
  VG_(free)(my_sm);
}

static void dg_pre_clo_init(void)
{


   VG_(details_name)            ("DerivGrind");
   VG_(details_version)         (NULL);
   VG_(details_description)     ("a tool for forward-mode AD of compiled programs.");
   VG_(details_copyright_author)(
      "Copyright (C) 2022, and GNU GPL'd, by Max Aehle.");
   VG_(details_bug_reports_to)  ("max.aehle@scicomp.uni-kl.de");

   VG_(details_avg_translation_sizeB) ( 275 );

   VG_(basic_tool_funcs)        (dg_post_clo_init,
                                 dg_instrument,
                                 dg_fini);


   /* No needs, no core events to track */
   VG_(printf)("Allocate shadow memory... ");
   my_sm = (ShadowMap*) VG_(malloc)("allocate_shadow_memory", sizeof(ShadowMap));
   #ifdef BUILD_32BIT
   if(my_sm==NULL) VG_(printf)("Error\n");
   my_sm->shadow_bits = 1;
   my_sm->application_bits = 1;
   my_sm->num_distinguished = 1;
   shadow_initialize_map(my_sm);
   #else
   shadow_initialize_with_mmap(my_sm);
   #endif
   VG_(printf)("done\n");

   VG_(needs_client_requests)     (dg_handle_client_request);

}

VG_DETERMINE_INTERFACE_VERSION(dg_pre_clo_init)

/*--------------------------------------------------------------------*/
/*--- end                                                          ---*/
/*--------------------------------------------------------------------*/