#ifndef DG_BAR_H
#define DG_BAR_H

#include "pub_tool_basics.h"
#include "../dg_utils.h"

/*! Add reverse-mode instrumentation to output IRSB.
 *  \param[in,out] diffenv - General data.
 *  \param[in] st_orig - Original statement.
 */
void dg_bar_handle_statement(DiffEnv* diffenv, IRStmt* st_orig);

/*! Initialize forward-mode data structures.
 */
void dg_bar_initialize(void);

/*! Destroy forward-mode data structures.
 */
void dg_bar_finalize(void);

#endif // DG_BAR_H
