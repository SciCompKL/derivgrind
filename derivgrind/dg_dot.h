#ifndef DG_DOT_H
#define DG_DOT_H

#include "pub_tool_basics.h"
#include "dg_utils.h"

/*! Add forward-mode instrumentation to output IRSB.
 *  \param[in,out] diffenv - General data.
 *  \param[in] st_orig - Original statement.
 */
void dg_dot_handle_statement(DiffEnv* diffenv, IRStmt* st_orig);

/*! Initialize forward-mode data structures.
 */
void dg_dot_initialize(void);

/*! Destroy forward-mode data structures.
 */
void dg_dot_finalize(void);

#endif // DG_DOT_H
