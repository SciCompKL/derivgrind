/*--------------------------------------------------------------------*/
/*--- Handling of min/max operations.              dg_dot_minmax.h ---*/
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

#ifndef DG_DOT_MINMAX_H
#define DG_DOT_MINMAX_H

#include "pub_tool_basics.h"

VG_REGPARM(0) ULong dg_dot_arithmetic_min32(ULong x, ULong xd, ULong y, ULong yd);
VG_REGPARM(0) ULong dg_dot_arithmetic_min64(ULong x, ULong xd, ULong y, ULong yd);
VG_REGPARM(0) ULong dg_dot_arithmetic_max32(ULong x, ULong xd, ULong y, ULong yd);
VG_REGPARM(0) ULong dg_dot_arithmetic_max64(ULong x, ULong xd, ULong y, ULong yd);


#endif // DG_DOT_MINMAX_H
