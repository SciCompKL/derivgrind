/*
   ----------------------------------------------------------------

   Notice that the following BSD-style license applies to this one
   file (derivgrind.h) only.  The rest of Valgrind is licensed under the
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
   (derivgrind.h) only.  The entire rest of Valgrind is licensed under
   the terms of the GNU General Public License, version 2.  See the
   COPYING file in the source distribution for details.

   ---------------------------------------------------------------- 
*/


#ifndef __DERIVGRIND_H
#define __DERIVGRIND_H


/* This file is for inclusion into client (your!) code.

   You can use these macros to manipulate and query memory permissions
   inside your own programs.

   See comment near the top of valgrind.h on how to use them.
*/

#include "valgrind.h"
#include "derivgrind-recording.h" // utility macros around the client requests

/* !! ABIWARNING !! ABIWARNING !! ABIWARNING !! ABIWARNING !! 
   This enum comprises an ABI exported by Valgrind to programs
   which use client requests.  DO NOT CHANGE THE ORDER OF THESE
   ENTRIES, NOR DELETE ANY -- add new ones at the end. */
typedef
   enum { 
      VG_USERREQ__GET_DOTVALUE = VG_USERREQ_TOOL_BASE('D','G'),
      VG_USERREQ__SET_DOTVALUE,
      VG_USERREQ__DISABLE,
      VG_USERREQ__GET_INDEX,
      VG_USERREQ__SET_INDEX,
      VG_USERREQ__NEW_INDEX,
      VG_USERREQ__NEW_INDEX_NOACTIVITYANALYSIS,
      VG_USERREQ__INDEX_TO_FILE,
      VG_USERREQ__GET_MODE
   } Vg_DerivgrindClientRequest;

typedef enum {
     DG_INDEXFILE_INPUT,
     DG_INDEXFILE_OUTPUT
   } Dg_Indexfile;

/* === Client-code macros to manipulate the state of memory. === */
// We added synonymes that write out "DG_" as "DERIVGRNID_" for better
// readability, and the VALGRIND_[S/G]ET_DERIVATIVE from the first preprint.

/* --- Forward mode. ---*/

/* Get dot value of variable at _qzz_addr into variable at _qzz_daddr of the same type of size _qzz_size. */
#define DG_GET_DOTVALUE(_qzz_addr,_qzz_daddr,_qzz_size)  \
    VALGRIND_DO_CLIENT_REQUEST_EXPR(0 /* default return */,      \
                            VG_USERREQ__GET_DOTVALUE,          \
                            (_qzz_addr), (_qzz_daddr), (_qzz_size), 0, 0)
#define DERIVGRIND_GET_DOTVALUE(_qzz_addr,_qzz_daddr,_qzz_size) DG_GET_DOTVALUE(_qzz_addr,_qzz_daddr,_qzz_size)
#define VALGRIND_GET_DERIVATIVE(_qzz_addr,_qzz_daddr,_qzz_size) DG_GET_DOTVALUE(_qzz_addr,_qzz_daddr,_qzz_size)
      
/* Set dot value of variable at _qzz_addr from variable at _qzz_daddr of the same type of size _qzz_size. */
#define DG_SET_DOTVALUE(_qzz_addr,_qzz_daddr,_qzz_size)  \
    VALGRIND_DO_CLIENT_REQUEST_EXPR(0 /* default return */,      \
                            VG_USERREQ__SET_DOTVALUE,          \
                            (_qzz_addr), (_qzz_daddr), (_qzz_size), 0, 0)
#define DERIVGRIND_SET_DOTVALUE(_qzz_addr,_qzz_daddr,_qzz_size) DG_SET_DOTVALUE(_qzz_addr,_qzz_daddr,_qzz_size)
#define VALGRIND_SET_DERIVATIVE(_qzz_addr,_qzz_daddr,_qzz_size) DG_SET_DOTVALUE(_qzz_addr,_qzz_daddr,_qzz_size)

/* Disable Derivgrind on specific code sections by putting the section
 * into a DG_DISABLE(1) ... DG_DISABLE(-1) bracket. Used by the math function
 * wrappers.
 *
 * In forward mode, this will enable/disable outputting of values and
 * dot values for difference quotient debugging, but dot values will still
 * be propagated.
 *
 * In recording mode, whenever a block would be written on the tape, this
 * will assign the index 0 or 0xff..f (if --typegrind=yes) instead of the
 * block index and not write to the tape. Also, typgrind warnings about an
 * index 0xff..f will be suppressed.
 */
#define DG_DISABLE(_qzz_delta) \
    VALGRIND_DO_CLIENT_REQUEST_EXPR(0 /* default return */,      \
                            VG_USERREQ__DISABLE,    \
                            (_qzz_delta), 0, 0, 0, 0)
#define DERIVGRIND_DISABLE(_qzz_delta) DG_DISABLE(_qzz_delta)

/* --- Recording mode. ---*/

/* Get index of variable at _qzz_addr into 8 byte at _qzz_iaddr. */
#define DG_GET_INDEX(_qzz_addr,_qzz_iaddr)  \
    VALGRIND_DO_CLIENT_REQUEST_EXPR(0 /* default return */,      \
                            VG_USERREQ__GET_INDEX,          \
                            (_qzz_addr), (_qzz_iaddr), 0, 0, 0)
#define DERIVGRIND_GET_INDEX(_qzz_addr,_qzz_iaddr) DG_GET_INDEX(_qzz_addr,_qzz_iaddr)


/* Set index of variable at _qzz_addr from 8 byte at _qzz_iaddr.*/
#define DG_SET_INDEX(_qzz_addr,_qzz_iaddr)  \
    VALGRIND_DO_CLIENT_REQUEST_EXPR(0 /* default return */,      \
                            VG_USERREQ__SET_INDEX,          \
                            (_qzz_addr), (_qzz_iaddr), 0, 0, 0)
#define DERIVGRIND_SET_INDEX(_qzz_addr,_qzz_iaddr) DG_SET_INDEX(_qzz_addr,_qzz_iaddr)

/* Push new operation to the tape, with activity analysis.
 * _qzz_index1addr, _qzz_index2addr point to 8-byte indices,
 * _qzz_diff1addr, _qzz_diff2addr point to 8-byte (double) partial derivatives,
 * _qzz_newindexaddr points to 8 byte for new index, which can be zero if input indices are zero.
 */
#define DG_NEW_INDEX(_qzz_index1addr,_qzz_index2addr,_qzz_diff1addr,_qzz_diff2addr,_qzz_newindexaddr)  \
    VALGRIND_DO_CLIENT_REQUEST_EXPR(0 /* default return */,      \
                            VG_USERREQ__NEW_INDEX,          \
                            (_qzz_index1addr), (_qzz_index2addr), (_qzz_diff1addr), (_qzz_diff2addr), (_qzz_newindexaddr))
#define DERIVGRIND_NEW_INDEX(_qzz_index1addr,_qzz_index2addr,_qzz_diff1addr,_qzz_diff2addr,_qzz_newindexaddr) DG_NEW_INDEX(_qzz_index1addr,_qzz_index2addr,_qzz_diff1addr,_qzz_diff2addr,_qzz_newindexaddr)

/* Push new operation to the tape, without activity analysis.
* _qzz_index1addr, _qzz_index2addr point to 8-byte indices,
* _qzz_diff1addr, _qzz_diff2addr point to 8-byte (double) partial derivatives,
* _qzz_newindexaddr points to 8 byte for new index, which is non-zero.
*/
#define DG_NEW_INDEX_NOACTIVITYANALYSIS(_qzz_index1addr,_qzz_index2addr,_qzz_diff1addr,_qzz_diff2addr,_qzz_newindexaddr)  \
    VALGRIND_DO_CLIENT_REQUEST_EXPR(0 /* default return */,      \
                            VG_USERREQ__NEW_INDEX_NOACTIVITYANALYSIS,          \
                            (_qzz_index1addr), (_qzz_index2addr), (_qzz_diff1addr), (_qzz_diff2addr), (_qzz_newindexaddr))
#define DERIVGRIND_NEW_INDEX_NOACTIVITYANALYSIS(_qzz_addr,_qzz_iaddr) DG_NEW_INDEX_NOACTIVITYANALYSIS(_qzz_addr,_qzz_iaddr)

/* Write index to an index file.
 */
#define DG_INDEX_TO_FILE(_qzz_outputfile,_qzz_indexaddr)  \
    VALGRIND_DO_CLIENT_REQUEST_EXPR(0 /* default return */,      \
                            VG_USERREQ__INDEX_TO_FILE,          \
                            (_qzz_outputfile), (_qzz_indexaddr), 0, 0, 0)
#define DERIVGRIND_INDEX_TO_FILE(_qzz_outputfile,_qzz_addrindex) DG_INDEX_TO_FILE(_qzz_outputfile,_qzz_addrindex)

/* Get AD mode.
 */
#define DG_GET_MODE  \
    VALGRIND_DO_CLIENT_REQUEST_EXPR(0 /* default return */,      \
                            VG_USERREQ__GET_MODE,          \
                            0, 0, 0, 0, 0)
#define DERIVGRIND_GET_MODE DG_GET_MODE


#endif


