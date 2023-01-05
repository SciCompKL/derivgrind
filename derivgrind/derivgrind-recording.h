/*
   ----------------------------------------------------------------

   Notice that the following BSD-style license applies to this one
   file (derivgrind-recording.h) only.  The rest of Valgrind is licensed under the
   terms of the GNU General Public License, version 2, unless
   otherwise indicated.  See the COPYING file in the source
   distribution for details.

   ----------------------------------------------------------------

   This file is part of Derivgrind, a Valgrind tool for automatic
   differentiation in forward mode.

   Copyright (C) 2022 Chair for Scientific Computing (SciComp), TU Kaiserslautern
   Homepage: https://www.scicomp.uni-kl.de
   Contact: Prof. Nicolas R. Gauger (derivgrind@scicomp.uni-kl.de)

   Lead developer: Max Aehle (SciComp, TU Kaiserslautern)

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions
   are met:

   1. Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.

   2. The origin of this software must not be misrepresented; you must
      not claim that you wrote the original software.  If you use this
      software in a product, an acknowledgment in the product
      documentation would be appreciated but is not required.

   3. Altered source versions must be plainly marked as such, and must
      not be misrepresented as being the original software.

   4. The name of the author may not be used to endorse or promote
      products derived from this software without specific prior written
      permission.

   THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
   OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
   WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
   ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
   DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
   DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
   GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
   INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
   WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
   NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
   SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

   ----------------------------------------------------------------

   Notice that the above BSD-style license applies to this one file
   (derivgrind-recording.h) only.  The entire rest of Valgrind is licensed under
   the terms of the GNU General Public License, version 2.  See the
   COPYING file in the source distribution for details.

   ----------------------------------------------------------------
*/

#ifndef DERIVGRIND_RECORDING_H
#define DERIVGRIND_RECORDING_H

/*! \file derivgrind-recording.h
 * Helper macros for simple marking of 
 * input and output variables in the client code.
 */

static unsigned long long dg_indextmp, dg_indextmp2;
static double dg_valtmp;
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
#define DG_INPUT(var) (dg_valtmp=(double)(var),DG_NEW_INDEX_NOACTIVITYANALYSIS(&dg_zero,&dg_zero,&dg_zero,&dg_zero,&dg_indextmp,&dg_valtmp), DG_SET_INDEX(&var,&dg_indextmp), dg_indextmp)

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
#define DG_OUTPUT(var) (dg_valtmp=(double)(var),DG_GET_INDEX(&var,&dg_indextmp2), DG_NEW_INDEX_NOACTIVITYANALYSIS(&dg_indextmp2,&dg_zero,&dg_one,&dg_zero,&dg_indextmp,&dg_valtmp), dg_indextmp)

/*! Mark variable as AD output, retrieve its 8-byte index, and dump the index into a file.
 */
#define DG_OUTPUTF(var) { dg_indextmp2 = DG_OUTPUT(var); DG_INDEX_TO_FILE(DG_INDEXFILE_OUTPUT, &dg_indextmp2); }



#endif
