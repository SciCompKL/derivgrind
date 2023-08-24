#ifndef DG_DOT_DIFFQUOTDEBUG_H
#define DG_DOT_DIFFQUOTDEBUG_H

#include "dot/dg_dot_shadow.h"
#include "bar/dg_bar_shadow.h"

#include "pub_tool_basics.h"
#include "pub_tool_tooliface.h"
#include "pub_tool_mallocfree.h"
#include "pub_tool_libcassert.h"
#include "pub_tool_gdbserver.h"
#include "pub_tool_libcbase.h"

#include "dg_utils.h"

#include "pub_tool_gdbserver.h"
#include "pub_tool_threadstate.h"
#include "pub_tool_libcfile.h"
#include "pub_tool_vki.h"

void dg_dot_diffquotdebug_initialize(const HChar* path);

void dg_dot_diffquotdebug_finalize(void);

void dg_add_diffquotdebug(IRSB* sb_out, IRExpr* value, IRExpr* dotvalue);

#endif // DG_DOT_DIFFQUOTDEBUG_H
