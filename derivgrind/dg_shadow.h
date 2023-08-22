/*--------------------------------------------------------------------*/
/*--- Shadow memory stuff.                             dg_shadow.h ---*/
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

#ifndef DG_SHADOW_H
#define DG_SHADOW_H

#include "pub_tool_basics.h"
#include "pub_tool_tooliface.h"
#include "pub_tool_mallocfree.h"
#include "pub_tool_libcassert.h"
#include "pub_tool_gdbserver.h"
#include "pub_tool_libcbase.h"

#include "dg_utils.h"

/*! Debugging help. Add a dirty statement to IRSB that prints the value of expr whenever it is run.
 *  \param[in] tag - Tag of your choice, will be printed alongside.
 *  \param[in] sb_out - IRSB to which the dirty statement is added.
 *  \param[in] expr - Expression.
 */
void dg_add_print_stmt(ULong tag, IRSB* sb_out, IRExpr* expr);

/*! Debugging help. Add a dirty statement to IRSB that prints two expressions whenever it is run.
 *  \param[in] sb_out - IRSB to which the dirty statement is added.
 *  \param[in] value - Expression.
 *  \param[in] dotvalue - Expression.
 */
void dg_add_diffquotdebug(IRSB* sb_out, IRExpr* value, IRExpr* dotvalue);

void dg_add_diffquotdebug_fini(void);

#endif // DG_SHADOW_H
