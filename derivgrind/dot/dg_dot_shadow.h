#ifndef DG_DOT_SHADOW_H
#define DG_DOT_SHADOW_H

#ifdef __cplusplus
extern "C" {
#endif

/*! */
void dg_dot_shadowGet(void* sm_address, void* real_address, int size);
void dg_dot_shadowSet(void* sm_address, void* real_address, int size);
void dg_dot_shadowInit();
void dg_dot_shadowFini();

#ifdef __cplusplus
}
#endif

#endif // DG_DOT_SHADOW_H
