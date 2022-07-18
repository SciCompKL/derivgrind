#ifndef DG_UTILS_H
#define DG_UTILS_H

#include "pub_tool_basics.h"
#include "pub_tool_tooliface.h"

#ifndef __GNUC__
  #error "Only tested with GCC."
#endif

#if __x86_64__
#else
#define BUILD_32BIT
#endif

#define DEFAULT_ROUNDING IRExpr_Const(IRConst_U32(Irrm_NEAREST))

/*! Make zero constant of certain type.
 */
IRExpr* mkIRConst_zero(IRType type);

#endif // DG_UTILS_H
