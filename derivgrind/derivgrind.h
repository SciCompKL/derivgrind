
/*
   ----------------------------------------------------------------

   Notice that the following BSD-style license applies to this one
   file (derivgrind.h) only.  The rest of Valgrind is licensed under the
   terms of the GNU General Public License, version 2, unless
   otherwise indicated.  See the COPYING file in the source
   distribution for details.

   ----------------------------------------------------------------

   This file is part of DerivGrind, a Valgrind tool for automatic 
   differentiation in forward mode.

   Copyright (C) 2022 Max Aehle

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

/* !! ABIWARNING !! ABIWARNING !! ABIWARNING !! ABIWARNING !! 
   This enum comprises an ABI exported by Valgrind to programs
   which use client requests.  DO NOT CHANGE THE ORDER OF THESE
   ENTRIES, NOR DELETE ANY -- add new ones at the end. */
typedef
   enum { 
      VG_USERREQ__GET_DERIVATIVE = VG_USERREQ_TOOL_BASE('D','G'),
      VG_USERREQ__SET_DERIVATIVE
   } Vg_DerivGrindClientRequest;



/* Client-code macros to manipulate the state of memory. */

/* Get derivative of variable at _qzz_addr into variable at _qzz_daddr of the same type of size _qzz_size. */
#define VALGRIND_GET_DERIVATIVE(_qzz_addr,_qzz_daddr,_qzz_size)  \
    VALGRIND_DO_CLIENT_REQUEST_EXPR(0 /* default return */,      \
                            VG_USERREQ__GET_DERIVATIVE,          \
                            (_qzz_addr), (_qzz_daddr), (_qzz_size), 0, 0)
      
/* Set derivative of variable at _qzz_addr from variable at _qzz_daddr of the same type of size _qzz_size. */
#define VALGRIND_SET_DERIVATIVE(_qzz_addr,_qzz_daddr,_qzz_size)  \
    VALGRIND_DO_CLIENT_REQUEST_EXPR(0 /* default return */,      \
                            VG_USERREQ__SET_DERIVATIVE,          \
                            (_qzz_addr), (_qzz_daddr), (_qzz_size), 0, 0)

#endif


