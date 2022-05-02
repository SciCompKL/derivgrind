#include <stdio.h>
int main(){
  double volatile a=2.0;
  printf("%lf\n", (a+1)/a);
}
