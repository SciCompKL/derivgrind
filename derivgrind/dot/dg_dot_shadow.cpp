#include "sm2/flexible-shadow.hpp"
#include "sm2/flexible-shadow-valgrindstdlib.hpp"
#include <pub_tool_libcbase.h>
#include "dg_utils.h"

#ifdef BUILD_32BIT
#define NUM_LOW_BITS 14
#else
#define NUM_LOW_BITS 18
#endif
struct ShadowLeafDot {
  UChar data[1ul<<NUM_LOW_BITS];
  static ShadowLeafDot distinguished;
};
ShadowLeafDot ShadowLeafDot::distinguished;
#ifdef BUILD_32BIT
using ShadowMapTypeDot = ShadowMap<Addr,ShadowLeafDot,ValgrindStandardLibraryInterface,18,14>;
#else
using ShadowMapTypeDot = ShadowMap<Addr,ShadowLeafDot,ValgrindStandardLibraryInterface,29,17,18>;
#endif
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
  for(Addr i=0; i<(1ul<<NUM_LOW_BITS); i++){
    ShadowLeafDot::distinguished.data[i] = 0;
  }
  sm_dot2 = (ShadowMapTypeDot*)VG_(malloc)("Space for primary map",sizeof(ShadowMapTypeDot));
  ShadowMapTypeDot::constructAt(sm_dot2);
}
extern "C" void dg_dot_shadowFini(){
  ShadowMapTypeDot::destructAt(sm_dot2);
  VG_(free)(sm_dot2);
}


