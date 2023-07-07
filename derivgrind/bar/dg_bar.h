/*--------------------------------------------------------------------*/
/*--- Recording-mode expression handling.                 dg_bar.h ---*/
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

#ifndef DG_BAR_H
#define DG_BAR_H

#include "pub_tool_basics.h"
#include "../dg_utils.h"

extern Bool typegrind;

extern Bool bar_record_values;

/*! Add reverse-mode instrumentation to output IRSB.
 *  \param[in,out] diffenv - General data.
 *  \param[in] st_orig - Original statement.
 */
void dg_bar_handle_statement(DiffEnv* diffenv, IRStmt* st_orig);

/*! Initialize recording-pass data structures.
 */
void dg_bar_initialize(void);

/*! Destroy recording-pass data structures.
 */
void dg_bar_finalize(void);

// Declarations of the tool functions are required by the bit-trick-finding instrumentation.
#ifdef DG_BAR_H_INCLUDE_TOOL_FUNCTIONS
  static void dg_bar_wrtmp(DiffEnv* diffenv, IRTemp temp, void* expr);
  static void* dg_bar_rdtmp(DiffEnv* diffenv, IRTemp temp)
  static void dg_bar_puti(DiffEnv* diffenv, Int offset, void* expr, IRRegArray* descr, IRExpr* ix);
  static void* dg_bar_geti(DiffEnv* diffenv, Int offset, IRType type, IRRegArray* descr, IRExpr* ix);
  static void dg_bar_store(DiffEnv* diffenv, IRExpr* addr, void* expr, IRExpr* guard);
  static void* dg_bar_load(DiffEnv* diffenv, IRExpr* addr, IRType type);
  static void* dg_bar_constant(DiffEnv* diffenv, IRConstTag type);
  static void* dg_bar_default_(DiffEnv* diffenv, IRType type);
  static IRExpr* dg_bar_compare(DiffEnv* diffenv, void* arg1, void* arg2);
  static void* dg_bar_ite(DiffEnv* diffenv, IRExpr* cond, void* dtrue, void* dfalse);
  extern V256* dg_bar_shadow_mem_buffer;
#endif


#endif // DG_BAR_H
