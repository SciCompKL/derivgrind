#include <math.h>
#include "valgrind/derivgrind.h"

/*! \file math.c
 * Compile to a replacement libm.so, whose functions 
 * - do their original job, and additionally 
 * - update the gradient information.
 * 
 * Access to the gradient information is given through
 * the DerivGrind client requests.
 * 
 * It does not suffice to just wrap machine instructions.
 * For instance, some versions of the glibc libm do not use
 * the x86 fsin instruction to implement sin. Rather,
 * they use several numerical approximations depending on
 * the magnitude of the angle. The derivatives of these 
 * approximations seem to be bad approximations of the
 * derivative sometimes. A replacement libm allows us to
 * use the accurate analytic derivatives.
 *
 * We often need to call math.h functions for the 
 * derivatives, e.g. the derivative of sin is cos. 
 * Fortunately, the glibc libm also exports __cos etc.,
 * which we can use for that. If differentiated code uses
 * these, it circumvents the replacement libm.
 *
 * Warning: This file is scanned by the libm-replacement
 * compile script to obtain all names of wrapped functions.
 * Therefore the wrapping must be done by the macros defined 
 * in this file.
 */

/*! Wrap a math.h function to also handle
 * the derivative information.
 */
#define DERIVGRIND_MATH_FUNCTION(name, deriv)\
double name (double x){ \
  double x_d; \
  VALGRIND_GET_DERIVATIVE(&x,&x_d,8); \
  double ret = __##name(x); \
  double ret_d = (deriv) * x_d; \
  VALGRIND_SET_DERIVATIVE(&ret,&ret_d,8); \
  return ret; \
}
/*! Wrap a math.h function with two arguments
 * to also handle the derivative information.
 */
#define DERIVGRIND_MATH_FUNCTION2(name, derivX, derivY)\
double name (double x, double y){ \
  double x_d, y_d; \
  VALGRIND_GET_DERIVATIVE(&x,&x_d,8); \
  VALGRIND_GET_DERIVATIVE(&y,&y_d,8); \
  double ret = __##name(x,y); \
  double ret_d = (derivX) * x_d + (derivY) * y_d; \
  VALGRIND_SET_DERIVATIVE(&ret,&ret_d,8); \
  return ret; \
}
/*! Wrap math.h function on (double, int).
 * Used for ldexp.
 */
#define DERIVGRIND_MATH_FUNCTION2i(name, deriv)\
double name (double x, int i){ \
  double x_d; \
  VALGRIND_GET_DERIVATIVE(&x,&x_d,8); \
  double ret = __##name(x,i); \
  double ret_d = (deriv) * x_d; \
  VALGRIND_SET_DERIVATIVE(&ret,&ret_d,8); \
  return ret; \
}
/*! Wrap math.h function on (double,int*).
 * Used for frexp.
 */
#define DERIVGRIND_MATH_FUNCTION2ip(name, deriv)\
double name (double x, int* p){ \
  double x_d; \
  VALGRIND_GET_DERIVATIVE(&x,&x_d,8); \
  double ret = __##name(x,p); \
  double ret_d = (deriv) * x_d; \
  VALGRIND_SET_DERIVATIVE(&ret,&ret_d,8); \
  return ret; \
}

// missing: modf,fmod
DERIVGRIND_MATH_FUNCTION(acos,-1/__sqrt(1-x*x))
DERIVGRIND_MATH_FUNCTION(asin,1/__sqrt(1-x*x))
DERIVGRIND_MATH_FUNCTION(atan,1/(1+x*x))
DERIVGRIND_MATH_FUNCTION2(atan2,-y/(x*x+y*y),x/(x*x+y*y))
DERIVGRIND_MATH_FUNCTION(ceil,0)
DERIVGRIND_MATH_FUNCTION(cos, -__sin(x))
DERIVGRIND_MATH_FUNCTION(cosh, __sinh(x))
DERIVGRIND_MATH_FUNCTION(exp, __exp(x))
DERIVGRIND_MATH_FUNCTION(fabs, (x>0?1:-1))
DERIVGRIND_MATH_FUNCTION(floor, 0)
DERIVGRIND_MATH_FUNCTION2ip(frexp,__ldexp(1,-*p))
DERIVGRIND_MATH_FUNCTION2i(ldexp,__ldexp(1,i))
DERIVGRIND_MATH_FUNCTION(log,1/x)
DERIVGRIND_MATH_FUNCTION(log10, 1/(__log(10)*x))
DERIVGRIND_MATH_FUNCTION2(pow, (y==0||y==-0)?0:(y*__pow(x,y-1)), __pow(x,y)*__log(x))
DERIVGRIND_MATH_FUNCTION(sin, __cos(x))
DERIVGRIND_MATH_FUNCTION(sinh, __cosh(x))
DERIVGRIND_MATH_FUNCTION(sqrt,1/(2*__sqrt(x)))
DERIVGRIND_MATH_FUNCTION(tan, 1/(__cos(x)*__cos(x)))
DERIVGRIND_MATH_FUNCTION(tanh, 1-__tanh(x)*__tanh(x))








