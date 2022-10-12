/*--------------------------------------------------------------------*/
/*--- Shadow memory stuff.                             dg_shadow.h ---*/
/*--------------------------------------------------------------------*/

/*
   This file is part of Derivgrind, a tool performing forward-mode
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

#ifndef DG_SHADOW_H
#define DG_SHADOW_H

/*! \file dg_shadow.h
 *  Interface to the shadow memory tool, and
 *  accompanying VEX utilities.
 */

#include "pub_tool_basics.h"
#include "pub_tool_tooliface.h"
#include "pub_tool_mallocfree.h"
#include "pub_tool_libcassert.h"
#include "pub_tool_gdbserver.h"
#include "pub_tool_libcbase.h"

#include "dg_utils.h"

// Unfortunately, for the currently employed shadow
// memory tool we cannot simply `#include shadow.h`
// because this header file contains definitions.
// Therefore, we just include it in dg_shadow.c,
// add some typedef's here, and use void* instead
// of ShadowMap*.

#ifdef BUILD_32BIT
typedef unsigned long int SM_Addr;
typedef unsigned char U8;
#else
typedef unsigned long long int SM_Addr;
typedef unsigned char U8;
#endif

/*! Initialize a shadow map.
 *  \returns Initialized shadow map pointer.
 */

void* initializeShadowMap(void);

/*! Destroy a shadow map.
 *  \param[in] sm - Shadow map.
 */
void destroyShadowMap(void* sm);

/*! Read from shadow memory.
 *  \param[in] sm - Shadow map.
 *  \param[in] sm_address - Pointer into shadow memory, indicating where to read from.
 *  \param[in] read_address - Pointer into real memory, indicating where to write to.
 *  \param[in] size - Number of bytes to be read.
 */
void shadowGet(void* sm, void* sm_address, void* real_address, int size);

/*! Write to shadow memory.
 *  \param[in] sm - Shadow map.
 *  \param[in] sm_address - Pointer into shadow memory, indicating where to write to.
 *  \param[in] read_address - Pointer into real memory, indicating where to read from.
 *  \param[in] size - Number of bytes to be written.
 */
void shadowSet(void* sm, void* sm_address, void* real_address, int size);

/*! Add VEX instructions to read the shadow memory at a given address.
 *  \param[in] sm - Shadow map.
 *  \param[in,out] sb_out - IRSB where VEX instruction can be added.
 *  \param[in] addr - Address to be read from.
 *  \param[in] type - Type of the variable stored at addr.
 *  \returns IRExpr that evaluates to the content of the shadow memory.
 */
IRExpr* loadShadowMemory(void* sm, IRSB* sb_out, IRExpr* addr, IRType type);

/*! Like loadShadowMemory, but default to read from original memory if the
 *  shadow memory has not been initialized yet.
 *
 *  \param[in] sm_data - Shadow map for data in shadow memory.
 *  \param[in] sm_init - Shadow map to store initialization bits.
 *  \param[in,out] sb_out - IRSB where VEX instruction can be added.
 *  \param[in] addr - Address to be read from.
 *  \param[in] type - Type of the variable stored at addr.
 *  \returns IRExpr that evaluates to the content of the shadow memory.
 */
IRExpr* loadLayeredShadowMemory(void* sm_data, void* sm_init, IRSB* sb_out, IRExpr* addr, IRType type);

/*! Add VEX instructions to store the shadow memory at a given address.
 *  \param[in] sm - Shadow map.
 *  \param[in,out] sb_out - IRSB where VEX instruction can be added.
 *  \param[in] addr - Address to store to.
 *  \param[in] expr - Expression whose value is to be stored.
 *  \param[in] guard - Store guard, can be NULL.
 */
void storeShadowMemory(void* sm, IRSB* sb_out, IRExpr* addr, IRExpr* expr, IRExpr* guard);

/*! Like storeShadowMemory, but also set initialization bits.
 *  \param[in] sm_data - Shadow map for data in shadow memory.
 *  \param[in] sm_init - Shadow map to store initialization bits.
 *  \param[in,out] sb_out - IRSB where VEX instruction can be added.
 *  \param[in] addr - Address to store to.
 *  \param[in] expr - Expression whose value is to be stored.
 *  \param[in] guard - Store guard, can be NULL.
 */
void storeLayeredShadowMemory(void* sm_data, void* sm_init, IRSB* sb_out, IRExpr* addr, IRExpr* expr, IRExpr* guard);


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

#endif // DG_SHADOW_H
