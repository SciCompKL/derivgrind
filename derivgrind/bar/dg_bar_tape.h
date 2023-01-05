#ifndef DG_BAR_TAPE_H
#define DG_BAR_TAPE_H

#include "pub_tool_basics.h"

/*! Add one elementary operation to the tape if an active variable is involved.
 *  \param index1 - Index of first operand.
 *  \param index2 - Index of second operand.
 *  \param diff1 - Partial derivative of result w.r.t. first operand.
 *  \param diff2 - Partial derivative of result w.r.t. second operand.
 *  \returns Index of result of operation. May be 0 if no active variable is involved.
 */
ULong tapeAddStatement(ULong index1,ULong index2,double diff1,double diff2);

/*! Add one elementary operation to the tape, without activity analysis.
 *  \param index1 - Index of first operand.
 *  \param index2 - Index of second operand.
 *  \param diff1 - Partial derivative of result w.r.t. first operand.
 *  \param diff2 - Partial derivative of result w.r.t. second operand.
 *  \returns Index of result of operation, newly assigned and in particular non-zero.
 */
ULong tapeAddStatement_noActivityAnalysis(ULong index1,ULong index2,double diff1,double diff2);

/*! Write index to input-index file.
 */
void dg_bar_tape_write_input_index(ULong index);

/*! Write index to output-index file.
 */
void dg_bar_tape_write_output_index(ULong index);

/*! Add one recorded value to the list of operation results.
 *
 *  Call this function after the corresponding tapeAddStatement that returned a non-zero index,
 *  and only if bar_record_values==True.
 *
 *  \param value - Value to be recorded.
 */
void valuesAddStatement(double value);
// Note: We did not merge valuesAddStatement into tapeAddStatement because the dirty call would
// need seven parameters (both halves of two indices, two partial derivatives, plus the value),
// which is currently not possible in Valgrind/VEX. So one must have two dirty calls, and
// correspondingly two separate functions that they call. Actually, we need to emit the
// dirty call for valuesAddStatement only if bar_record_values==True.

/*! Initialize tape.
 */
void dg_bar_tape_initialize(const HChar* filename);

/*! Finalize tape.
 */
void dg_bar_tape_finalize(void);

#endif // DG_BAR_TAPE_H
