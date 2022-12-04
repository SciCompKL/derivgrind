#include "sm2/flexible-shadow.hpp"
#include "sm2/flexible-shadow-valgrindstdlib.hpp"
#include <pub_tool_libcbase.h>
#include "dg_utils.h"

#ifdef BUILD_32BIT
#define NUM_LOW_BITS 14
#else
#define NUM_LOW_BITS 18
#endif
struct ShadowLeaf {
  UChar data[1ul<<NUM_LOW_BITS];
  static ShadowLeaf distinguished;
};
ShadowLeaf ShadowLeaf::distinguished;
void initialize_distinguished(){
  for(Addr i=0; i<(1ul<<NUM_LOW_BITS); i++){
    ShadowLeaf::distinguished.data[i] = 0;
  }
}
#ifdef BUILD_32BIT
using ShadowMapType = ShadowMap<Addr,ShadowLeaf,ValgrindStandardLibraryInterface,18,14>;
#else
using ShadowMapType = ShadowMap<Addr,ShadowLeaf,ValgrindStandardLibraryInterface,29,17,18>;
#endif
ShadowMapType* sm_dot2;

extern "C" void dg_dot_shadowGet(void* sm_address, void* real_address, int size){
  ShadowLeaf* leaf = sm_dot2->leaf_for_read((Addr)sm_address);
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
  ShadowLeaf* leaf = sm_dot2->leaf_for_write((Addr)sm_address);
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
  sm_dot2 = (ShadowMapType*)VG_(malloc)("Space for primary map",sizeof(ShadowMapType));
  ShadowMapType::constructAt(sm_dot2);
}
extern "C" void dg_dot_shadowFini(){
  ShadowMapType::destructAt(sm_dot2);
  VG_(free)(sm_dot2);
}


