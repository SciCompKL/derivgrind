/*--------------------------------------------------------------------*/
/*--- Utilities.                                        dg_utils.h ---*/
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

/*! Make zero constant of certain type.
 */
IRExpr* mkIRConst_zero(IRType type);

/*! Make constant of certain type with bitwise representation 0xff..ff.
 */
IRExpr* mkIRConst_ones(IRType type);


/*! \struct DiffEnv
 *  Data required for differentiation, is passed to differentiate_expr.
 */
typedef struct {
  /*! Offsets for indices of shadow temporaries.
   *
   *  Use index 1 for foward-mode AD and index 2 for paragrind.
   */
  IRTemp tmp_offset[3];
  /*! Offsets for byte offsets into the shadow guest state (registers).
   *
   *  Use index 1 for foward-mode AD and index 2 for paragrind.
   */
  Int gs_offset[3];
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



#endif // DG_UTILS_H
