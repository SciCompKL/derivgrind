#include <valgrind/derivgrind.h>
#include <stdio.h>

void valgrind_set_derivative(void** val, void** grad, int* size){
  VALGRIND_SET_DERIVATIVE(*val,*grad,*size);
}
void valgrind_get_derivative(void** val, void** grad, int* size){
  VALGRIND_GET_DERIVATIVE(*val,*grad,*size);
}
