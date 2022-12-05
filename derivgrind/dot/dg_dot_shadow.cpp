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


