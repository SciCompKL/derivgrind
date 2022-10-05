#ifndef DG_BAR_TAPE_H
#define DG_BAR_TAPE_H

#include "pub_tool_basics.h"

/*! Add one elementary operation to the tape.
 *  \param index1 - Index of first operand.
 *  \param index2 - Index of second operand.
 *  \param diff1 - Partial derivative of result w.r.t. first operand.
 *  \param diff2 - Partial derivative of result w.r.t. second operand.
 *  \returns Index of result of operation.
 */
ULong tapeAddStatement(ULong index1,ULong index2,double diff1,double diff2);

/*! Initialize tape.
 */
void dg_bar_tape_initialize(const HChar* filename);

/*! Finalize tape.
 */
void dg_bar_tape_finalize(void);

#endif // DG_BAR_TAPE_H
