/*--------------------------------------------------------------------*/
/*--- Derivgrind utilities.                             dg_utils.h ---*/
/*--------------------------------------------------------------------*/

/*
   This file is part of Derivgrind, an automatic differentiation
   tool applicable to compiled programs.

   Copyright (C) 2022, Chair for Scientific Computing, TU Kaiserslautern
   Copyright (C) since 2023, Chair for Scientific Computing, University of Kaiserslautern-Landau
   Homepage: https://www.scicomp.uni-kl.de
   Contact: Prof. Nicolas R. Gauger

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

#ifndef DG_UTILS_H
#define DG_UTILS_H

#include "pub_tool_basics.h"
#include "pub_tool_tooliface.h"

#ifndef __GNUC__
  #error "Only tested with GCC."
#endif

#if __x86_64__
#else
#define BUILD_32BIT
#endif

#define DEFAULT_ROUNDING IRExpr_Const(IRConst_U32(Irrm_NEAREST))

/*! Make constant of certain type with bitwise representation 0x00..00.
 *
 *  This corresponds to a floating-point 0.0.
 */
IRExpr* mkIRConst_zero(IRType type);

/*! Make constant of certain type with bitwise representation 0xff..ff.
 */
IRExpr* mkIRConst_ones(IRType type);

/*! Make SIMD vector filled by components representing the floating-point number 2.0.
 *
 * Required for the derivative of the square root.
 * The type of the return expression is F32 or F64 for scalars,
 * and I64, V128 or V256 if there is more than one component.
 * \param[in] fpsize - Size of a component in bytes, 4 or 8.
 * \param[in] simdsize - Number of components, 1, 2, 4 or 8.
 */
IRExpr* mkIRConst_fptwo(int fpsize, int simdsize);

/*! \struct DiffEnv
 *  Data required for differentiation, is passed to differentiate_expr.
 */
typedef struct {
  /*! Offset for indices of shadow temporaries.
   */
  IRTemp tmp_offset;
  /*! Offset for byte offsets into the shadow guest state (registers).
   */
  Int gs_offset;
  /*! Add helper statements to this IRSB.
   */
  IRSB* sb_out;
  /*! If success of a CAS operation is tested in
   *  one instrumentation step, use the result in
   *  subsequent instrumentation steps.
   */
  IRTemp cas_succeeded;
} DiffEnv;

// Some valid pieces of VEX IR cannot be translated back to machine code by
// Valgrind, but end up with an "ISEL" error. Therefore we sometimes need
// workarounds using convertToF64 and convertFromF64.
/*! Convert F32 and F64 expressions to F64.
 *  \param[in] expr - F32 or F64 expression.
 *  \param[in] diffenv - Differentiation environment.
 *  \param[out] originaltype - Original type is stored here so convertFromF64 can go back.
 *  \return F64 expression.
 */
IRExpr* convertToF64(IRExpr* expr, DiffEnv* diffenv, IRType* originaltype);

/*! Convert F64 expressions to F32 or F64.
 *  \param[in] expr - F64 expression.
 *  \param[in] originaltype - Original type is stored here from convertToF64.
 *  \return F32 or F64 expression.
 */
IRExpr* convertFromF64(IRExpr* expr, IRType originaltype);

/*! Check whether expression evaluates to zero.
 *
 *  Used by dg_bar_operation with typegrind==True to decide
 *  whether to emit an index 0 or 0xff..f. If type==Ity_INVALID,
 *  expr may be NULL and True is returned.
 *
 *  \param expr Expression to be compared with zero.
 *  \param type Type of evaluated expression.
 */
IRExpr* isZero(IRExpr* expr, IRType type);

/*! Extract one component of a SIMD vector expression, as I32 or I64 expression.
 *
 * \param[in] expression - SIMD vector.
 * \param[in] fpsize - Size of a component in bytes, 4 or 8.
 * \param[in] simdsize - Number of components, 1, 2, 4 or 8.
 * \param[in] component - Index within SIMD vector, 0 <= component < simdsize.
 * \param[in] diffenv - Additional data.
 */
IRExpr* getSIMDComponent(IRExpr* expression, int fpsize, int simdsize, int component, DiffEnv* diffenv);

/*! Assemble a SIMD vector from expressions for its components.
 *  \param[in] expressions - Array of expressions for components.
 *  \param[in] fpsize - Size of a component in bytes, 4 or 8.
 *  \param[in] simdsize - Number of components.
 *  \param[in] diffenv - Additional data.
 */
IRExpr* assembleSIMDVector(IRExpr** expressions, int fpsize, int simdsize, DiffEnv* diffenv);

/*! Convert between types of same size.
 *  \param diffenv - General setup.
 *  \param expr - Expression to be converted.
 *  \param type - Type to convert expression into.
 *  \returns Converted expression.
 */
IRExpr* reinterpretType(DiffEnv* diffenv, IRExpr* expr,IRType type);

/*! Helper to extract high/low addresses of CAS statement.
 *
 *  One of addr_Lo, addr_Hi is det->addr,
 *  the other one is det->addr + offset.
 *  \param[in] det - CAS statement details.
 *  \param[in] diffenv - Differentiation environment.
 *  \param[out] addr_Lo - Low address.
 *  \param[out] addr_Hi - High address.
 */
void addressesOfCAS(IRCAS const* det, IRSB* sb_out, IRExpr** addr_Lo, IRExpr** addr_Hi);


#endif // DG_UTILS_H
