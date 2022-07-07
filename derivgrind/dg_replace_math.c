#include <valgrind.h>
double I_WRAP_SONAME_FNNAME_ZZ(Za,testfun)(double x) 
{
  double res; unsigned long res_i;
  OrigFn fn;
  VALGRIND_GET_ORIG_FN(fn);
  unsigned long x_i = *(unsigned long*)&x;
  CALL_FN_W_W(res_i, fn, x_i);
  res = *(double*)&res_i;
  return res+2;
}
