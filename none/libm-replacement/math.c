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
#define DERIVGRIND_MATH_FUNCTION(name, deriv, type)\
type name (type x){ \
  type x_d; \
  VALGRIND_GET_DERIVATIVE(&x,&x_d,8); \
  type ret = __##name(x); \
  type ret_d = (deriv) * x_d; \
  VALGRIND_SET_DERIVATIVE(&ret,&ret_d,8); \
  return ret; \
}
/*! Wrap a math.h function with two arguments
 * to also handle the derivative information.
 */
#define DERIVGRIND_MATH_FUNCTION2(name, derivX, derivY,type)\
type name (type x, type y){ \
  type x_d, y_d; \
  VALGRIND_GET_DERIVATIVE(&x,&x_d,8); \
  VALGRIND_GET_DERIVATIVE(&y,&y_d,8); \
  type ret = __##name(x,y); \
  type ret_d = (derivX) * x_d + (derivY) * y_d; \
  VALGRIND_SET_DERIVATIVE(&ret,&ret_d,8); \
  return ret; \
}
/*! Wrap math.h function on (double, int).
 * Used for ldexp.
 */
#define DERIVGRIND_MATH_FUNCTION2i(name, deriv,type)\
type name (type x, int i){ \
  type x_d; \
  VALGRIND_GET_DERIVATIVE(&x,&x_d,8); \
  type ret = __##name(x,i); \
  type ret_d = (deriv) * x_d; \
  VALGRIND_SET_DERIVATIVE(&ret,&ret_d,8); \
  return ret; \
}
/*! Wrap math.h function on (double,int*).
 * Used for frexp.
 */
#define DERIVGRIND_MATH_FUNCTION2ip(name, deriv,type)\
type name (type x, int* p){ \
  type x_d; \
  VALGRIND_GET_DERIVATIVE(&x,&x_d,8); \
  type ret = __##name(x,p); \
  type ret_d = (deriv) * x_d; \
  VALGRIND_SET_DERIVATIVE(&ret,&ret_d,8); \
  return ret; \
}

// missing: modf,fmod
DERIVGRIND_MATH_FUNCTION(acos,-1/__sqrt(1-x*x),double)
DERIVGRIND_MATH_FUNCTION(asin,1/__sqrt(1-x*x),double)
DERIVGRIND_MATH_FUNCTION(atan,1/(1+x*x),double)
DERIVGRIND_MATH_FUNCTION2(atan2,-y/(x*x+y*y),x/(x*x+y*y),double)
DERIVGRIND_MATH_FUNCTION(ceil,0,double)
DERIVGRIND_MATH_FUNCTION(cos, -__sin(x),double)
DERIVGRIND_MATH_FUNCTION(cosh, __sinh(x),double)
DERIVGRIND_MATH_FUNCTION(exp, __exp(x),double)
DERIVGRIND_MATH_FUNCTION(fabs, (x>0?1:-1),double)
DERIVGRIND_MATH_FUNCTION(floor, 0,double)
DERIVGRIND_MATH_FUNCTION2ip(frexp,__ldexp(1,-*p),double)
DERIVGRIND_MATH_FUNCTION2i(ldexp,__ldexp(1,i),double)
DERIVGRIND_MATH_FUNCTION(log,1/x,double)
DERIVGRIND_MATH_FUNCTION(log10, 1/(__log(10)*x),double)
DERIVGRIND_MATH_FUNCTION2(pow, (y==0||y==-0)?0:(y*__pow(x,y-1)), __pow(x,y)*__log(x),double)
DERIVGRIND_MATH_FUNCTION(sin, __cos(x),double)
DERIVGRIND_MATH_FUNCTION(sinh, __cosh(x),double)
DERIVGRIND_MATH_FUNCTION(sqrt,1/(2*__sqrt(x)),double)
DERIVGRIND_MATH_FUNCTION(tan, 1/(__cos(x)*__cos(x)),double)
DERIVGRIND_MATH_FUNCTION(tanh, 1-__tanh(x)*__tanh(x),double)

// float versions
DERIVGRIND_MATH_FUNCTION(acosf,-1/__sqrtf(1-x*x),float)
DERIVGRIND_MATH_FUNCTION(asinf,1/__sqrtf(1-x*x),float)
DERIVGRIND_MATH_FUNCTION(atanf,1/(1+x*x),float)
DERIVGRIND_MATH_FUNCTION2(atan2f,-y/(x*x+y*y),x/(x*x+y*y),float)
DERIVGRIND_MATH_FUNCTION(ceilf,0,float)
DERIVGRIND_MATH_FUNCTION(cosf, -__sinf(x),float)
DERIVGRIND_MATH_FUNCTION(coshf, __sinhf(x),float)
DERIVGRIND_MATH_FUNCTION(expf, __expf(x),float)
DERIVGRIND_MATH_FUNCTION(fabsf, (x>0?1:-1),float)
DERIVGRIND_MATH_FUNCTION(floorf, 0,float)
DERIVGRIND_MATH_FUNCTION2ip(frexpf,__ldexpf(1,-*p),float)
DERIVGRIND_MATH_FUNCTION2i(ldexpf,__ldexpf(1,i),float)
DERIVGRIND_MATH_FUNCTION(logf,1/x,float)
DERIVGRIND_MATH_FUNCTION(log10f, 1/(__logf(10)*x),float)
DERIVGRIND_MATH_FUNCTION2(powf, (y==0||y==-0)?0:(y*__powf(x,y-1)), __powf(x,y)*__logf(x),float)
DERIVGRIND_MATH_FUNCTION(sinf, __cosf(x),float)
DERIVGRIND_MATH_FUNCTION(sinhf, __coshf(x),float)
DERIVGRIND_MATH_FUNCTION(sqrtf,1/(2*__sqrtf(x)),float)
DERIVGRIND_MATH_FUNCTION(tanf, 1/(__cosf(x)*__cosf(x)),float)
DERIVGRIND_MATH_FUNCTION(tanhf, 1-__tanhf(x)*__tanhf(x),float)







