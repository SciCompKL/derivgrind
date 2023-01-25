/*
   ----------------------------------------------------------------
   Notice that the following MIT license applies to this one file
   (tape-evaluation.cpp) only.  The rest of Valgrind is licensed under the
   terms of the GNU General Public License, version 2, unless
   otherwise indicated.  See the COPYING file in the source
   distribution for details.
   ----------------------------------------------------------------

   This file is part of Derivgrind, an automatic differentiation
   tool applicable to compiled programs.

   Copyright (C) 2022, Chair for Scientific Computing, TU Kaiserslautern
   Copyright (C) since 2023, Chair for Scientific Computing, University of Kaiserslautern-Landau
   Homepage: https://www.scicomp.uni-kl.de
   Contact: Prof. Nicolas R. Gauger

   Lead developer: Max Aehle

   Permission is hereby granted, free of charge, to any person obtaining a copy
   of this software and associated documentation files (the "Software"), to deal
   in the Software without restriction, including without limitation the rights
   to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
   copies of the Software, and to permit persons to whom the Software is
   furnished to do so, subject to the following conditions:
   
   The above copyright notice and this permission notice shall be included in all
   copies or substantial portions of the Software.
   
   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
   OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
   SOFTWARE.

   ----------------------------------------------------------------
   Notice that the above MIT license applies to this one file
   (tape-evaluation.cpp) only.  The rest of Valgrind is licensed under the
   terms of the GNU General Public License, version 2, unless
   otherwise indicated.  See the COPYING file in the source
   distribution for details.
   ----------------------------------------------------------------
*/

#ifndef DERIVGRIND_RECORDING_H
#define DERIVGRIND_RECORDING_H

/*! \file derivgrind-recording.h
 * Helper macros for simple marking of 
 * input and output variables in the client code.
 */

static unsigned long long dg_indextmp, dg_indextmp2;
static unsigned long long const dg_zero = 0;
static double const dg_one = 1.;

/*! Mark variable as AD input and assign new 8-byte index to it.
 * 
 * You may e.g. write
 *
 *     printf("a index = %llu\n", DG_INPUT(a));
 *
 * to print the index from your client code. Or use the DG_INPUTF macro to
 * write it directly into a file.
 */
#define DG_INPUT(var) (DG_NEW_INDEX_NOACTIVITYANALYSIS(&dg_zero,&dg_zero,&dg_zero,&dg_zero,&dg_indextmp), DG_SET_INDEX(&var,&dg_indextmp), dg_indextmp)

/*! Mark variable as AD input, assign new 8-byte index, and dump the index into a file.
 */
#define DG_INPUTF(var) { dg_indextmp2 = DG_INPUT(var); DG_INDEX_TO_FILE(DG_INDEXFILE_INPUT, &dg_indextmp2); }

/*! Mark variable as AD output and retrieve its 8-byte index.
 * 
 * You may e.g. write
 *
 *     printf("a index = %llu\n", DG_OUTPUT(a));
 *
 * to print the index from your client code. Or use the DG_OUTPUTF macro to
 * write it directly into a file.
 */
#define DG_OUTPUT(var) (DG_GET_INDEX(&var,&dg_indextmp2), DG_NEW_INDEX_NOACTIVITYANALYSIS(&dg_indextmp2,&dg_zero,&dg_one,&dg_zero,&dg_indextmp), dg_indextmp)

/*! Mark variable as AD output, retrieve its 8-byte index, and dump the index into a file.
 */
#define DG_OUTPUTF(var) { dg_indextmp2 = DG_OUTPUT(var); DG_INDEX_TO_FILE(DG_INDEXFILE_OUTPUT, &dg_indextmp2); }



#endif
