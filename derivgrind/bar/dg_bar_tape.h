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

#ifndef DG_BAR_TAPE_H
#define DG_BAR_TAPE_H

#include "pub_tool_basics.h"

/*! Add one elementary operation to the tape if an active variable is involved.
 *  \param index1 - Index of first operand.
 *  \param index2 - Index of second operand.
 *  \param diff1 - Partial derivative of result w.r.t. first operand.
 *  \param diff2 - Partial derivative of result w.r.t. second operand.
 *  \returns Index of result of operation. May be 0 if no active variable is involved.
 */
ULong tapeAddStatement(ULong index1,ULong index2,double diff1,double diff2);

/*! Add one elementary operation to the tape, without activity analysis.
 *  \param index1 - Index of first operand.
 *  \param index2 - Index of second operand.
 *  \param diff1 - Partial derivative of result w.r.t. first operand.
 *  \param diff2 - Partial derivative of result w.r.t. second operand.
 *  \returns Index of result of operation, newly assigned and in particular non-zero.
 */
ULong tapeAddStatement_noActivityAnalysis(ULong index1,ULong index2,double diff1,double diff2);

/*! Write index to input-index file.
 */
void dg_bar_tape_write_input_index(ULong index);

/*! Write index to output-index file.
 */
void dg_bar_tape_write_output_index(ULong index);

/*! Add one recorded value to the list of operation results.
 *
 *  Call this function after the corresponding tapeAddStatement that returned a non-zero index,
 *  and only if bar_record_values==True.
 *
 *  \param value - Value to be recorded.
 */
void valuesAddStatement(double value);
// Note: We did not merge valuesAddStatement into tapeAddStatement because the dirty call would
// need seven parameters (both halves of two indices, two partial derivatives, plus the value),
// which is currently not possible in Valgrind/VEX. So one must have two dirty calls, and
// correspondingly two separate functions that they call. Actually, we need to emit the
// dirty call for valuesAddStatement only if bar_record_values==True.

/*! Initialize tape.
 */
void dg_bar_tape_initialize(const HChar* filename);

/*! Finalize tape.
 */
void dg_bar_tape_finalize(void);

#endif // DG_BAR_TAPE_H
