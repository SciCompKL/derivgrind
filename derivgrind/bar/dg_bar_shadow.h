#ifndef DG_BAR_SHADOW_H
#define DG_BAR_SHADOW_H

#ifdef __cplusplus
extern "C" {
#endif

/*! */
void dg_bar_shadowGet(void* sm_address, void* real_address_Lo, void* real_address_Hi, int size);
void dg_bar_shadowSet(void* sm_address, void* real_address, void* real_address_Hi, int size);
void dg_bar_shadowInit();
void dg_bar_shadowFini();

#ifdef __cplusplus
}
#endif

#endif // DG_BAR_SHADOW_H
