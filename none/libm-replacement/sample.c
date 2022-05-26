// Sample C program using math.h.
// Used to determine location and version of libm.so,
// and GLIBC version.

#include <math.h>
#include <stdio.h>

int main(){
  double a;
  scanf("%lf", &a);
  printf("%lf", sin(a));
}


