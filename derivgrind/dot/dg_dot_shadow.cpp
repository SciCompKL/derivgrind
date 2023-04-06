/*--------------------------------------------------------------------*/
/*--- Forward-mode shadow memory interface.      dg_dot_shadow.cpp ---*/
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

#include "externals/flexible-shadow/flexible-shadow.hpp"
#include "externals/flexible-shadow/flexible-shadow-valgrindstdlib.hpp"
#include <pub_tool_libcbase.h>
#include "dg_utils.h"

#ifndef SHADOW_LAYERS_32
  #define SHADOW_LAYERS_32 18,14
#endif
#ifndef SHADOW_LAYERS_64
  #define SHADOW_LAYERS_64 29,17,18
#endif

#ifdef BUILD_32BIT
  #define SHADOW_LAYERS SHADOW_LAYERS_32
#else
  #define SHADOW_LAYERS SHADOW_LAYERS_64
#endif

struct ShadowLeafDot {
  UChar data[1ul<<(SHADOW_LAYERS)];
  static ShadowLeafDot distinguished;
};
ShadowLeafDot ShadowLeafDot::distinguished;

using ShadowMapTypeDot = ShadowMap<Addr,ShadowLeafDot,ValgrindStandardLibraryInterface,SHADOW_LAYERS>;

ShadowMapTypeDot* sm_dot2;

extern "C" void dg_dot_shadowGet(void* sm_address, void* real_address, int size){
  ShadowLeafDot* leaf = sm_dot2->leaf_for_read((Addr)sm_address);
  Addr contiguousSize = sm_dot2->contiguousElements((Addr)sm_address);
  ULong index = sm_dot2->index((Addr)sm_address);
  if(contiguousSize >= size){
    VG_(memcpy)(real_address, &leaf->data[index], size);
  } else {
    VG_(memcpy)(real_address, &leaf->data[index], contiguousSize);
    dg_dot_shadowGet((void*)((Addr)sm_address+contiguousSize),(void*)((Addr)real_address+contiguousSize),size-contiguousSize);
  }
}

extern "C" void dg_dot_shadowSet(void* sm_address, void* real_address, int size){
  ShadowLeafDot* leaf = sm_dot2->leaf_for_write((Addr)sm_address);
  Addr contiguousSize = sm_dot2->contiguousElements((Addr)sm_address);
  ULong index = sm_dot2->index((Addr)sm_address);
  if(contiguousSize >= size){
    VG_(memcpy)(&leaf->data[index], real_address, size);
  } else {
    VG_(memcpy)(&leaf->data[index], real_address, contiguousSize);
    dg_dot_shadowSet((void*)((Addr)sm_address+contiguousSize),(void*)((Addr)real_address+contiguousSize),size-contiguousSize);
  }
}

extern "C" void dg_dot_shadowInit(){
  for(Addr i=0; i<(1ul<<(SHADOW_LAYERS)); i++){
    ShadowLeafDot::distinguished.data[i] = 0;
  }
  sm_dot2 = (ShadowMapTypeDot*)VG_(malloc)("Space for primary map",sizeof(ShadowMapTypeDot));
  ShadowMapTypeDot::constructAt(sm_dot2);
}
extern "C" void dg_dot_shadowFini(){
  ShadowMapTypeDot::destructAt(sm_dot2);
  VG_(free)(sm_dot2);
}


