#include <math.h>
#include "valgrind/derivgrind.h"

double sin(double x){
  double x_d;
  VALGRIND_GET_DERIVATIVE(&x,&x_d,8);
  double ret = __sin(x);
  double ret_d = __cos(x) * x_d;
  VALGRIND_SET_DERIVATIVE(&ret,&ret_d,8);
  return ret;
}

double cos(double x){
  double x_d;
  VALGRIND_GET_DERIVATIVE(&x,&x_d,8);
  double ret = __cos(x);
  double ret_d =- __sin(x) * x_d;
  VALGRIND_SET_DERIVATIVE(&ret,&ret_d,8);
  return ret;
}

double sqrt(double x){
  double x_d;
  VALGRIND_GET_DERIVATIVE(&x,&x_d,8);
  double ret = __sqrt(x);
  double ret_d = 0.5/__sqrt(x) * x_d;
  VALGRIND_SET_DERIVATIVE(&ret,&ret_d,8);
  return ret;
}
