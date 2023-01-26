/*--------------------------------------------------------------------*/
/*--- Recording-mode shadow memory interface.    dg_bar_shadow.cpp ---*/
/*--------------------------------------------------------------------*/

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

struct ShadowLeafBar {
  UChar data_Lo[1ul<<(SHADOW_LAYERS)];
  UChar data_Hi[1ul<<(SHADOW_LAYERS)];
  static ShadowLeafBar distinguished;
};
ShadowLeafBar ShadowLeafBar::distinguished;

using ShadowMapTypeBar = ShadowMap<Addr,ShadowLeafBar,ValgrindStandardLibraryInterface,SHADOW_LAYERS>;

ShadowMapTypeBar* sm_bar2;

extern "C" void dg_bar_shadowGet(void* sm_address, void* real_address_Lo, void* real_address_Hi, int size){
  ShadowLeafBar* leaf = sm_bar2->leaf_for_read((Addr)sm_address);
  Addr contiguousSize = sm_bar2->contiguousElements((Addr)sm_address);
  ULong index = sm_bar2->index((Addr)sm_address);
  if(contiguousSize >= size){
    if(real_address_Lo) VG_(memcpy)(real_address_Lo, &leaf->data_Lo[index], size);
    if(real_address_Hi) VG_(memcpy)(real_address_Hi, &leaf->data_Hi[index], size);
  } else {
    if(real_address_Lo){
      VG_(memcpy)(real_address_Lo, &leaf->data_Lo[index], contiguousSize);
      real_address_Lo = (void*)((Addr)real_address_Lo+contiguousSize);
    }
    if(real_address_Hi){
      VG_(memcpy)(real_address_Hi, &leaf->data_Hi[index], contiguousSize);
      real_address_Hi = (void*)((Addr)real_address_Hi+contiguousSize);
    }
    sm_address = (void*)((Addr)sm_address+contiguousSize);
    size = size-contiguousSize;
    dg_bar_shadowGet(sm_address,real_address_Lo,real_address_Hi,size);
  }
}

extern "C" void dg_bar_shadowSet(void* sm_address, void* real_address_Lo, void* real_address_Hi, int size){
  ShadowLeafBar* leaf = sm_bar2->leaf_for_write((Addr)sm_address);
  Addr contiguousSize = sm_bar2->contiguousElements((Addr)sm_address);
  ULong index = sm_bar2->index((Addr)sm_address);
  if(contiguousSize >= size){
    if(real_address_Lo) VG_(memcpy)(&leaf->data_Lo[index], real_address_Lo, size);
    if(real_address_Hi) VG_(memcpy)(&leaf->data_Hi[index], real_address_Hi, size);
  } else {
    if(real_address_Lo){
      VG_(memcpy)(&leaf->data_Lo[index], real_address_Lo, contiguousSize);
      real_address_Lo = (void*)((Addr)real_address_Lo+contiguousSize);
    }
    if(real_address_Hi){
      VG_(memcpy)(&leaf->data_Hi[index], real_address_Hi, contiguousSize);
      real_address_Hi = (void*)((Addr)real_address_Hi+contiguousSize);
    }
    sm_address = (void*)((Addr)sm_address+contiguousSize);
    size = size-contiguousSize;
    dg_bar_shadowSet(sm_address,real_address_Lo,real_address_Hi,size);
  }
}

extern "C" void dg_bar_shadowInit(){
  for(Addr i=0; i<(1ul<<(SHADOW_LAYERS)); i++){
    ShadowLeafBar::distinguished.data_Lo[i] = 0;
    ShadowLeafBar::distinguished.data_Hi[i] = 0;
  }
  sm_bar2 = (ShadowMapTypeBar*)VG_(malloc)("Space for primary map",sizeof(ShadowMapTypeBar));
  ShadowMapTypeBar::constructAt(sm_bar2);
}
extern "C" void dg_bar_shadowFini(){
  ShadowMapTypeBar::destructAt(sm_bar2);
  VG_(free)(sm_bar2);
}


