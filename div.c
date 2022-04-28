#include <stdio.h>
int main(){
  double a;
  scanf("%lf",&a);
  printf("%lf\n", (1.0+a/0.1)/(a+0.1));
}
