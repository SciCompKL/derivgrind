#include <math.h>
#include "valgrind/derivgrind.h"

/*! Wrap a math.h function to also handle
 * the derivative information.
 */
#define DERIVGRIND_MATH_FUNCTION(name, deriv)\
double name (double x){ \
  double x_d; \
  VALGRIND_GET_DERIVATIVE(&x,&x_d,8); \
  double ret = __##name(x); \
  double ret_d = deriv * x_d; \
  VALGRIND_SET_DERIVATIVE(&ret,&ret_d,8); \
  return ret; \
}

DERIVGRIND_MATH_FUNCTION(sin, __cos(x))
DERIVGRIND_MATH_FUNCTION(cos, -__sin(x))
DERIVGRIND_MATH_FUNCTION(sqrt,0.5/__sqrt(x))

