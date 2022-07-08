#include <valgrind.h>
#include <derivgrind.h>
#include <stdbool.h>
#include <stdio.h>

static bool called_from_within_wrapper = false;

double I_WRAP_SONAME_FNNAME_ZU(Za,sin)(double x) 
{
  OrigFn fn;
  VALGRIND_GET_ORIG_FN(fn);
  double res;
  printf("x=%lf\n",x);
  volatile double x2=x;
  CALL_FN_D_D(res, fn, x2);
  /*if(!called_from_within_wrapper){
    printf("Enter in sin\n");
    //VALGRIND_GET_DERIVATIVE(&x, &x_d, 8);
    called_from_within_wrapper = true;
      double grad = cos(x) * x_d;
    called_from_within_wrapper = false;
    //VALGRIND_SET_DERIVATIVE(&res, &grad, 8);
  }
  */
  printf("sin x=%lf res=%lf\n", x,res);
  return res;
}
/*
double I_WRAP_SONAME_FNNAME_ZZ(NONE,cosf)(double x) 
{
  unsigned long res_i;
  OrigFn fn;
  VALGRIND_GET_ORIG_FN(fn);
  unsigned long x_i = *(unsigned long*)&x;
  CALL_FN_W_W(res_i, fn, x_i);
  double res = *(double*)&res_i;
  if(!called_from_within_wrapper){
    printf("Enter in cos\n");
    double x_d;
    VALGRIND_GET_DERIVATIVE(&x, &x_d, 8);
    called_from_within_wrapper = true;
      double grad = -sin(x) * x_d;
    called_from_within_wrapper = false;
    VALGRIND_SET_DERIVATIVE(&res, &grad, 8);
  }
  printf("cos x=%lf res=%lf\n", x,res);
  return res;
}

*/

