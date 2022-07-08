#include <valgrind.h>
#include <derivgrind.h>
#include <stdbool.h>
#include <stdio.h>
#include <math.h>

static bool called_from_within_wrapper = false;


__attribute__((optimize("O0")))
double I_WRAP_SONAME_FNNAME_ZU(Za,sin)(double x) 
{
  OrigFn fn;
  VALGRIND_GET_ORIG_FN(fn);
  double ret;
  CALL_FN_D_D(ret, fn, x);
  if(!called_from_within_wrapper){
    double x_d;
    VALGRIND_GET_DERIVATIVE(&x, &x_d, 8);
    called_from_within_wrapper = true;
      double ret_d = cos(x) * x_d;
    called_from_within_wrapper = false;
    VALGRIND_SET_DERIVATIVE(&ret, &ret_d, 8);
  }
  return ret;
}

__attribute__((optimize("O0")))
double I_WRAP_SONAME_FNNAME_ZU(Za,cos)(double x) 
{
  OrigFn fn;
  VALGRIND_GET_ORIG_FN(fn);
  double ret;
  CALL_FN_D_D(ret, fn, x);
  if(!called_from_within_wrapper){
    double x_d;
    VALGRIND_GET_DERIVATIVE(&x, &x_d, 8);
    called_from_within_wrapper = true;
      double ret_d = -sin(x) * x_d;
    called_from_within_wrapper = false;
    VALGRIND_SET_DERIVATIVE(&ret, &ret_d, 8);
  }
  return ret;
}
