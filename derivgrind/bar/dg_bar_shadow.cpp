#include "sm2/flexible-shadow.hpp"
#include "sm2/flexible-shadow-valgrindstdlib.hpp"
#include <pub_tool_libcbase.h>
#include "dg_utils.h"

#ifdef BUILD_32BIT
#define NUM_LOW_BITS 14
#else
#define NUM_LOW_BITS 18
#endif
struct ShadowLeafBar {
  UChar data_Lo[1ul<<NUM_LOW_BITS];
  UChar data_Hi[1ul<<NUM_LOW_BITS];
  static ShadowLeafBar distinguished;
};
ShadowLeafBar ShadowLeafBar::distinguished;
#ifdef BUILD_32BIT
using ShadowMapTypeBar = ShadowMap<Addr,ShadowLeafBar,ValgrindStandardLibraryInterface,18,14>;
#else
using ShadowMapTypeBar = ShadowMap<Addr,ShadowLeafBar,ValgrindStandardLibraryInterface,29,17,18>;
#endif
ShadowMapTypeBar* sm_bar2;

extern "C" void dg_bar_shadowGet(void* sm_address, void* real_address_Lo, void* real_address_Hi, int size){
  ShadowLeafBar* leaf = sm_bar2->leaf_for_read((Addr)sm_address);
  Addr contiguousSize = sm_bar2->contiguousElements((Addr)sm_address);
  ULong index = sm_bar2->index((Addr)sm_address);
  if(contiguousSize >= size){
    VG_(memcpy)(real_address_Lo, &leaf->data_Lo[index], size);
    VG_(memcpy)(real_address_Hi, &leaf->data_Hi[index], size);
  } else {
    VG_(memcpy)(real_address_Lo, &leaf->data_Lo[index], contiguousSize);
    VG_(memcpy)(real_address_Hi, &leaf->data_Hi[index], contiguousSize);
    dg_bar_shadowGet((void*)((Addr)sm_address+contiguousSize),(void*)((Addr)real_address_Lo+contiguousSize),(void*)((Addr)real_address_Hi+contiguousSize),size-contiguousSize);
  }
}

extern "C" void dg_bar_shadowSet(void* sm_address, void* real_address_Lo, void* real_address_Hi, int size){
  ShadowLeafBar* leaf = sm_bar2->leaf_for_write((Addr)sm_address);
  Addr contiguousSize = sm_bar2->contiguousElements((Addr)sm_address);
  ULong index = sm_bar2->index((Addr)sm_address);
  if(contiguousSize >= size){
    VG_(memcpy)(&leaf->data_Lo[index], real_address_Lo, size);
    VG_(memcpy)(&leaf->data_Hi[index], real_address_Hi, size);
  } else {
    VG_(memcpy)(&leaf->data_Lo[index], real_address_Lo, contiguousSize);
    VG_(memcpy)(&leaf->data_Hi[index], real_address_Hi, contiguousSize);
    dg_bar_shadowSet((void*)((Addr)sm_address+contiguousSize),(void*)((Addr)real_address_Lo+contiguousSize),(void*)((Addr)real_address_Hi+contiguousSize),size-contiguousSize);
  }
}

extern "C" void dg_bar_shadowInit(){
  for(Addr i=0; i<(1ul<<NUM_LOW_BITS); i++){
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


