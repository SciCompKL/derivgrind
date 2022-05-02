
/*--------------------------------------------------------------------*/
/*--- Nulgrind: The minimal Valgrind tool.               nl_main.c ---*/
/*--------------------------------------------------------------------*/

/*
   This file is part of Nulgrind, the minimal Valgrind tool,
   which does no instrumentation or analysis.

   Copyright (C) 2002-2017 Nicholas Nethercote
      njn@valgrind.org

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

#include "pub_tool_basics.h"
#include "pub_tool_tooliface.h"
#include "pub_tool_mallocfree.h"

#include <assert.h>
#include "shadow-memory/src/shadow.h"
// ----------------------------------------------------------------------------
// The following might end up in its own header file eventually, but for now
// only the application can really know how to set the right types and system
// calls.
/*#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stddef.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>

void  shadow_free(void* addr) { VG_(free)(addr); }
void *shadow_malloc(size_t size) { return VG_(malloc)(size); }
void *shadow_calloc(size_t nmemb, size_t size) { return VG_(calloc)(nmemb, size); }
void  shadow_memcpy(void* dst, void* src, size_t size) { VG_(memcpy)(dst,src,size); }
void  shadow_out_of_memory() {
  VG_(printf)("ERROR: Ran out of memory while allocating shadow memory.\n");
  exit(1);
}*/
// ----------------------------------------------------------------------------
inline void  shadow_free(void* addr) { VG_(free)(addr); }
inline void *shadow_malloc(size_t size) { return VG_(malloc)("Test",size); }
inline void *shadow_calloc(size_t nmemb, size_t size) { return VG_(calloc)("test", nmemb, size); }
inline void  shadow_memcpy(void* dst, void* src, size_t size) { VG_(memcpy)(dst,src,size); }
inline void  shadow_out_of_memory() {
  VG_(printf)("ERROR: Ran out of memory while allocating shadow memory.\n");
	VG_(exit)(1);
}


ShadowMap* my_sm = NULL;

static void nl_post_clo_init(void)
{
}

static
IRSB* nl_instrument ( VgCallbackClosure* closure,
                      IRSB* bb,
                      const VexGuestLayout* layout, 
                      const VexGuestExtents* vge,
                      const VexArchInfo* archinfo_host,
                      IRType gWordTy, IRType hWordTy )
{
  for(int i=0; i<bb->stmts_used; i++){
    if(bb->stmts[i]->tag == Ist_WrTmp){
        if(/*bb->stmts[i]->Ist.Put.data->tag == Iex_Binop*/
           /*bb->stmts[i]->Ist.PutI.details->ix->tag == Iex_Binop*/
           bb->stmts[i]->Ist.WrTmp.data->tag == Iex_Binop
           /* bb->stmts[i]->Ist.Store.data->tag == Iex_Binop */
           /*bb->stmts[i]->Ist.StoreG.details->data->tag == Iex_Binop*/
        ) {
            VG_(printf)("division %d\n", bb->stmts[i]->Ist.Put.data->Iex.Binop.op);
          }
      }
    }
  U8 tmp = 0;
   shadow_get_bits(my_sm, 0xffff1111, &tmp);
  VG_(printf)("shadow bits: %d\n", tmp);
   return bb;
}

static void nl_fini(Int exitcode)
{
  VG_(printf)("%d %d %d %d %d %d %d %d %d %d %d %d \n",
              Iop_DivF64, Iop_DivF32, Iop_DivF64r32,
              Iop_DivF128, Iop_DivD64, Iop_DivD128,
              Iop_Div32Fx4, Iop_Div32F0x4, Iop_Div64Fx2,
              Iop_Div64F0x2, Iop_Div64Fx4, Iop_Div32Fx8   );
  shadow_destroy_map(my_sm);
  VG_(free)(my_sm);

}

static void nl_pre_clo_init(void)
{


   VG_(details_name)            ("Nulgrind");
   VG_(details_version)         (NULL);
   VG_(details_description)     ("the minimal Valgrind tool");
   VG_(details_copyright_author)(
      "Copyright (C) 2002-2017, and GNU GPL'd, by Nicholas Nethercote.");
   VG_(details_bug_reports_to)  (VG_BUGS_TO);

   VG_(details_avg_translation_sizeB) ( 275 );

   VG_(basic_tool_funcs)        (nl_post_clo_init,
                                 nl_instrument,
                                 nl_fini);

   /* No needs, no core events to track */
   VG_(printf)("Allocate SM...");
   my_sm = (ShadowMap*) VG_(malloc)("Some text", sizeof(ShadowMap));
   if(my_sm==NULL) VG_(printf)("Error\n");
   my_sm->shadow_bits = 1;
   my_sm->application_bits = 1;
   my_sm->num_distinguished = 1;
   shadow_initialize_map(my_sm);
   VG_(printf)("done\n");

  shadow_set_bits(my_sm, 0xffff1111, 0xab);

}

VG_DETERMINE_INTERFACE_VERSION(nl_pre_clo_init)

/*--------------------------------------------------------------------*/
/*--- end                                                          ---*/
/*--------------------------------------------------------------------*/
