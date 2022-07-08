#include <valgrind.h>
#include <derivgrind.h>
#include <stdbool.h>
#include <stdio.h>

static bool initialization = false;

void I_WRAP_SONAME_FNNAME_ZZ(Za,initmod)(){
  initialization = !initialization;
}

static OrigFn fn_sin, fn_cos;

double I_WRAP_SONAME_FNNAME_ZZ(Za,sin)(double x) 
{
  unsigned long res_i;
  printf("Ent sin\n");
  VALGRIND_GET_ORIG_FN(fn_sin);
  if(initialization) return 0.;
  unsigned long x_i = *(unsigned long*)&x;
  CALL_FN_D_D(res_i, fn_sin, x_i);
  double res = *(double*)&res_i;
    double x_d; unsigned long res2_i;
    CALL_FN_D_D(res2_i, fn_cos, x_i);
    double res2 = *(double*)&res2_i;
    VALGRIND_GET_DERIVATIVE(&x, &x_d, 8);
      double grad = res2 * x_d;
    VALGRIND_SET_DERIVATIVE(&res, &grad, 8);
  printf("sin x=%lf res=%lf\n", x,res);
  return res;
}
double I_WRAP_SONAME_FNNAME_ZZ(Za,cos)(double x) 
{
  printf("Ent cos\n");
  unsigned long res_i;
  VALGRIND_GET_ORIG_FN(fn_cos);
  if(initialization) return 0.;
  unsigned long x_i = *(unsigned long*)&x;
  CALL_FN_D_D(res_i, fn_cos, x_i);
  double res = *(double*)&res_i;
    double x_d; unsigned long res2_i;
    CALL_FN_D_D(res2_i, fn_sin, x_i);
    double res2 = *(double*)&res2_i;
    VALGRIND_GET_DERIVATIVE(&x, &x_d, 8);
      double grad = -res2 * x_d;
    VALGRIND_SET_DERIVATIVE(&res, &grad, 8);
  printf("sin x=%lf res=%lf\n", x,res);
  return res;
}
