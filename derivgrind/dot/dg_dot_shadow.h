/*--------------------------------------------------------------------*/
/*--- Forward-mode shadow memory interface.        dg_dot_shadow.h ---*/
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

#ifndef DG_DOT_SHADOW_H
#define DG_DOT_SHADOW_H

#ifdef __cplusplus
extern "C" {
#endif

/*! */
void dg_dot_shadowGet(void* sm_address, void* real_address, int size);
void dg_dot_shadowSet(void* sm_address, void* real_address, int size);
void dg_dot_shadowInit(void);
void dg_dot_shadowFini(void);

#ifdef __cplusplus
}
#endif

#endif // DG_DOT_SHADOW_H
