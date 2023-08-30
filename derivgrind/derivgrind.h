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
   Contact: Prof. Nicolas R. Gauger (derivgrind@projects.rptu.de)

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
      VG_USERREQ__GET_MODE,
      VG_USERREQ__GET_FLAGS,
      VG_USERREQ__SET_FLAGS,
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

/* Disable certain Derivgrind actions on specific sections of user code
 * by putting the section into a DG_DISABLE(1,0) ... DG_DISABLE(0,1) bracket.
 *
 * The macro DG_DISABLE(plus,minus) will add plus and subtract minus from
 * a Derivgrind-internal counter dg_disable. When the counter is non-zero,
 * the following Derivgrind actions are disabled.
 *
 * In forward mode, this will enable/disable outputting of values and
 * dot values for difference quotient debugging, but dot values will still
 * be propagated.
 *
 * In recording mode, whenever a block would be written on the tape, this
 * will assign the index 0 or 0xff..f (if --typegrind=yes) instead of the
 * block index and not write to the tape. Also, typgrind warnings about an
 * index 0xff..f will be suppressed.
 *
 * In bit-trick-finding mode, warning messages will be suppressed.
 *
 * We use this client request in the math replacement wrappers, because we
 * do not want to see Derivgrind messages for, and recordings of, the
 * original math library code and the derivative computation.
 */
#define DG_DISABLE(_qzz_plus,_qzz_minus) \
    VALGRIND_DO_CLIENT_REQUEST_EXPR(0 /* default return */,      \
                            VG_USERREQ__DISABLE,    \
                            (_qzz_plus), (_qzz_minus), 0, 0, 0)
#define DERIVGRIND_DISABLE(_qzz_plus,_qzz_minus) DG_DISABLE(_qzz_plus,_qzz_minus)

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

typedef struct {
   void const* index1addr; //!< Address where index of first operand is read from, of type unsigned long long.
   void const* index2addr; //!< Address where index of second operand is read from, of type unsigned long long.
   void const* diff1addr;  //!< Address where the partial derivative w.r.t. first operand is read from, of type double.
   void const* diff2addr; //!< Address where the partial derivative w.r.t. second operand is read from, of type double.
   void* newindexaddr; //!< Address where the index of the result is written to, of type unsigned long long.
   void const* valueaddr; //!< Address where the value of the result can be read from for debugging purposes, of type double.
} TapeBlockInfo;

static TapeBlockInfo tbi;
/* Push new operation to the tape, with activity analysis.
* _qzz_index1addr, _qzz_index2addr point to 8-byte indices,
* _qzz_diff1addr, _qzz_diff2addr point to 8-byte (double) partial derivatives,
* _qzz_newindexaddr points to 8 byte for new index, which can be zero if input indices are zero,
* _qzz_valueaddr points to double for value.
*/
#define DG_NEW_INDEX(_qzz_index1addr,_qzz_index2addr,_qzz_diff1addr,_qzz_diff2addr,_qzz_newindexaddr,_qzz_valueaddr)  \
   ( \
     tbi.index1addr = _qzz_index1addr, \
     tbi.index2addr = _qzz_index2addr, \
     tbi.diff1addr = _qzz_diff1addr, \
     tbi.diff2addr = _qzz_diff2addr, \
     tbi.newindexaddr = _qzz_newindexaddr, \
     tbi.valueaddr = _qzz_valueaddr, \
     VALGRIND_DO_CLIENT_REQUEST_EXPR(0 /* default return */,      \
                            VG_USERREQ__NEW_INDEX,          \
                            &tbi, 0, 0, 0, 0) \
   )
#define DERIVGRIND_NEW_INDEX(_qzz_index1addr,_qzz_index2addr,_qzz_diff1addr,_qzz_diff2addr,_qzz_newindexaddr,_qzz_valueaddr) DG_NEW_INDEX(_qzz_index1addr,_qzz_index2addr,_qzz_diff1addr,_qzz_diff2addr,_qzz_newindexaddr,_qzz_valueaddr)


/* Push new operation to the tape, without activity analysis.
* _qzz_index1addr, _qzz_index2addr point to 8-byte indices,
* _qzz_diff1addr, _qzz_diff2addr point to 8-byte (double) partial derivatives,
* _qzz_newindexaddr points to 8 byte for new index, which is non-zero,
* _qzz_valueaddr points to double for value.
*/
#define DG_NEW_INDEX_NOACTIVITYANALYSIS(_qzz_index1addr,_qzz_index2addr,_qzz_diff1addr,_qzz_diff2addr,_qzz_newindexaddr,_qzz_valueaddr)  \
   ( \
     tbi.index1addr = _qzz_index1addr, \
     tbi.index2addr = _qzz_index2addr, \
     tbi.diff1addr = _qzz_diff1addr, \
     tbi.diff2addr = _qzz_diff2addr, \
     tbi.newindexaddr = _qzz_newindexaddr, \
     tbi.valueaddr = _qzz_valueaddr, \
     VALGRIND_DO_CLIENT_REQUEST_EXPR(0 /* default return */,      \
                            VG_USERREQ__NEW_INDEX_NOACTIVITYANALYSIS,          \
                            &tbi, 0, 0, 0, 0) \
   )
#define DERIVGRIND_NEW_INDEX_NOACTIVITYANALYSIS(_qzz_index1addr,_qzz_index2addr,_qzz_diff1addr,_qzz_diff2addr,_qzz_newindexaddr,_qzz_valueaddr) DG_NEW_INDEX_NOACTIVITYANALYSIS(_qzz_index1addr,_qzz_index2addr,_qzz_diff1addr,_qzz_diff2addr,_qzz_newindexaddr,_qzz_valueaddr)

/* Write index to an index file.
 */
#define DG_INDEX_TO_FILE(_qzz_outputfile,_qzz_indexaddr)  \
    VALGRIND_DO_CLIENT_REQUEST_EXPR(0 /* default return */,      \
                            VG_USERREQ__INDEX_TO_FILE,          \
                            (_qzz_outputfile), (_qzz_indexaddr), 0, 0, 0)
#define DERIVGRIND_INDEX_TO_FILE(_qzz_outputfile,_qzz_addrindex) DG_INDEX_TO_FILE(_qzz_outputfile,_qzz_addrindex)

/* Get flags of the bit-trick finder.
 */
#define DG_GET_FLAGS(_qzz_addr,_qzz_Aaddr, _qzz_Daddr, _qzz_size)  \
    VALGRIND_DO_CLIENT_REQUEST_EXPR(0, \
                            VG_USERREQ__GET_FLAGS,\
                            (_qzz_addr), (_qzz_Aaddr), (_qzz_Daddr), (_qzz_size), 0)

/* Set flags of the bit-trick finder.
 */
#define DG_SET_FLAGS(_qzz_addr,_qzz_Aaddr, _qzz_Daddr, _qzz_size)  \
    VALGRIND_DO_CLIENT_REQUEST_EXPR(0, \
                            VG_USERREQ__SET_FLAGS,\
                            (_qzz_addr), (_qzz_Aaddr), (_qzz_Daddr), (_qzz_size), 0)

/* Get AD mode.
 */
#define DG_GET_MODE  \
    VALGRIND_DO_CLIENT_REQUEST_EXPR(0 /* default return */,      \
                            VG_USERREQ__GET_MODE,          \
                            0, 0, 0, 0, 0)
#define DERIVGRIND_GET_MODE DG_GET_MODE


#endif


