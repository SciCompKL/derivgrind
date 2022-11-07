#ifndef DG_EXPRESSIONHANDLING_H
#define DG_EXPRESSIONHANDLING_H

#include "pub_tool_basics.h"
#include "pub_tool_tooliface.h"
#include "pub_tool_mallocfree.h"
#include "pub_tool_libcassert.h"
#include "pub_tool_gdbserver.h"
#include "pub_tool_libcbase.h"
#include "pub_tool_options.h"
#include "valgrind.h"
#include "derivgrind.h"

#include "dg_utils.h"

/*! \file dg_expressionhandling.h
 * AD-mode independent handling of VEX statements and expressions.
 */

/*! Tuple of functions defining how to modify expressions and to instrument statements.
 * 
 * We define two instances:
 * - dg_dot_expressionhandling in dot/dg_dot.c for forward-mode instrumentation.
 * - dg_bar_expressionhandling in bar/dg_bar.c for recording-mode instrumentation.
 */
typedef struct {

  /*! How to store data in shadow temporary.
   *  \param diffenv - General setup.
   *  \param temp - Index of the temporary.
   *  \param expr - Data to be stored.
   */
  void (*wrtmp)(DiffEnv* diffenv, IRTemp temp, void* expr);
  /*! How to load data from shadow temporary.
   *  \param diffenv - General setup.
   *  \param temp - Index of the temporary.
   *  \returns Data from shadow temporary.
   */
  void* (*rdtmp)(DiffEnv* diffenv, IRTemp temp);

  /*! How to store data in shadow register.
   *  \param diffenv - General setup.
   *  \param offset - Offset into shadow register (Put) or bias (PutI).
   *  \param expr - Data to be stored.
   *  \param descr - NULL (Put) or description of circular structure (PutI).
   *  \param ix - NULL (Put) or variable component of register offset (PutI).
   */
  void (*puti)(DiffEnv* diffenv,Int offset,void* expr,IRRegArray* descr,IRExpr* ix);
  /*! How to load data from shadow register.
   *  \param diffenv - General setup.
   *  \param offset - Offset into shadow register (Get) or bias (GetI).
   *  \param type - Primal data type.
   *  \param descr - NULL (Put) or description of circular structure (PutI).
   *  \param ix - NULL (Put) or variable component of register offset (PutI).
   *  \returns Data from shadow register.
   */
  void* (*geti)(DiffEnv* diffenv, Int offset, IRType type, IRRegArray* descr, IRExpr* ix);

  /*! How to store data in shadow memory.
   *  \param diffenv - General setup.
   *  \param addr - Address into memory.
   *  \param expr - Data to be stored.
   *  \param guard - Guard.
   */
  void (*store)(DiffEnv* diffenv, IRExpr* addr, void* expr, IRExpr* guard);
  /*! How to load data from shadow memory.
   *  \param diffenv - General setup.
   *  \param addr - Address into memory.
   *  \param type - Primal data type.
   *  \returns Data from shadow memory.
   */
  void* (*load)(DiffEnv* diffenv, IRExpr* addr, IRType type);

  /*! Handling of storeF80le dirty call.
   *  \param diffenv - General setup.
   *  \param addr - Address into memory.
   *  \param expr - Data.
   */
  void (*dirty_storeF80le)(DiffEnv* diffenv,IRExpr* addr,void* expr);
  /*! Handling of loadF80le dirty call.
   *  \param diffenv - General setup.
   *  \param addr - Address into memory.
   *  \param temp - Temporary to be written to.
   */
  void (*dirty_loadF80le)(DiffEnv* diffenv,IRExpr* addr,IRTemp temp);

  /*! Handling of constant expressions.
   *  \param diffenv - General setup.
   *  \param type - Primal data type.
   *  \returns Data for constant expression.
   */
  void* (*constant)(DiffEnv* diffenv, IRConstTag type);
  /*! Default data if modification fails.
   *  \param diffenv - General setup.
   *  \param type - Primal data type.
   */
  void* (*default_)(DiffEnv* diffenv, IRType type);
  /*! Comparison of data.
   *  \param diffenv - General setup.
   *  \param arg1 - Data.
   *  \param arg2 - Data.
   *  \returns Expression comparing data.
   */
  IRExpr* (*compare)(DiffEnv* diffenv, void* arg1, void* arg2);
  /*! Handling of if-then-else expression.
   *  \param diffenv - General setup.
   *  \param cond - Condition.
   *  \param dtrue - True branch.
   *  \param dfalse - False branch.
   */
  void* (*ite)(DiffEnv* diffenv, IRExpr* cond, void* dtrue, void* dfalse);
  /*! Handling of operations.
   *  \param diffenv - General setup.
   *  \param op - Operation.
   *  \param arg1 - Primal argument.
   *  \param arg2 - Primal argument.
   *  \param arg3 - Primal argument.
   *  \param arg4 - Primal argument.
   *  \param mod1 - Modified argument.
   *  \param mod2 - Modified argument.
   *  \param mod3 - Modified argument.
   *  \param mod4 - Modified argument.
   */
  void* (*operation)(DiffEnv* diffenv,
                     IROp op,
                     IRExpr* arg1, IRExpr* arg2, IRExpr* arg3, IRExpr* arg4,
                     void* mod1, void* mod2, void* mod3, void* mod4);

} ExpressionHandling;

/*! Return expression modified for use in the instrumented statement.
 *
 *  Replace reads from temporaries, registers or memory by reads from
 *  the respective shadow locations as specified by the ExpressionHandling.
 *  Replace operations and constant expressions as specified by the
 *  ExpressionHandling.
 *
 *  \param diffenv - General setup.
 *  \param eh - Mode-dependent details of the modification.
 *  \param ex - Expression to be modified.
 *  \returns Modified expression.
 */
void* dg_modify_expression(DiffEnv* diffenv, ExpressionHandling eh, IRExpr* ex);

/*! Handle expressions with dg_modify_expressions, but return default data 
 *  in unhandled cases.
 */
void* dg_modify_expression_or_default(DiffEnv* diffenv, ExpressionHandling eh, IRExpr* expr, Bool warn, const char* operation);

/*! Add instrumented statement to output IRSB.
 *  \param diffenv - General setup.
 *  \param eh - Mode-dependent details of the instrumentation.
 *  \param st_orig - Original statement to be instrumented.
 */
void add_statement_modified(DiffEnv* diffenv, ExpressionHandling eh, IRStmt* st_orig);



#endif // DG_EXPRESSIONHANDLING_H
