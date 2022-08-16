#ifndef DG_DOT_EXPRESSIONS_H
#define DG_DOT_EXPRESSIONS_H

#include "pub_tool_basics.h"
#include "pub_tool_libcassert.h"
#include "pub_tool_tooliface.h"
#include "dg_utils.h"



/*! Differentiate an expression, i.e. produce an
 *  expression computing the dot value.
 *
 *  - For arithmetic expressions involving float or double variables, we
 *    use the respective differentiation rules. Not all kinds of operations
 *    have been implemented already.
 *  - For expressions that just copy bytes, we copy the respective shadow
 *    bytes.
 *  - Otherwise, we return NULL. The function differentiate_or_zero has
 *    been created to return zero bytes and possibly output a warning.
 *
 *  The function might add helper statements to diffenv.sb_out.
 *
 *  \param[in] ex - Expression.
 *  \param[in,out] diffenv - Additional data necessary for differentiation.
 *  \returns Differentiated expression or NULL.
 */
IRExpr* differentiate_expr(IRExpr const* ex, DiffEnv* diffenv );

/*! Compute derivative. If this fails, return a zero expression.
 *  \param[in] expr - expression to be differentiated
 *  \param[in] diffenv - Additional data necessary for differentiation.
 *  \param[in] warn - If true, a warning message will be printed if differentiation fails.
 *  \param[in] operation - Word for how the derivative is used, e.g. 'WrTmp' or 'Store'.
 * \returns Differentiated expression or zero expression.
 */
IRExpr* differentiate_or_zero(IRExpr* expr, DiffEnv* diffenv, Bool warn, const char* operation);

#endif // DG_DOT_EXPRESSIONS_H
