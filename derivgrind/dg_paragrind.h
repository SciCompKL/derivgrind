/*--------------------------------------------------------------------*/
/*--- ParaGrind: Computing results for a shifted    dg_paragrind.c ---*/
/*--- input alongside the original input.                          ---*/
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

#ifndef DG_PARAGRIND_H
#define DG_PARAGRIND_H


#include "pub_tool_basics.h"
#include "pub_tool_tooliface.h"
#include "pub_tool_libcassert.h"
#include "dg_shadow.h"

extern void *sm_pgdata;
extern void *sm_pginit;

void dg_paragrind_pre_clo_init(void);

IRExpr* parallel_expr(IRExpr* ex, DiffEnv* diffenv);

IRExpr* parallel_or_zero(IRExpr* ex, DiffEnv* diffenv, Bool warn, const char* operation);


#endif // DG_PARAGRIND_H
