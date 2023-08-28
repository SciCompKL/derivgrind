
#include <iostream>
#include <cmath>
#include "valgrind/derivgrind.h"

template<typename fp>
void bittrick_input(fp& a){
  char mode = DG_GET_MODE;
  if(mode=='d'){
    fp const one=1.0;
    DG_SET_DOTVALUE(&a,&one,sizeof(fp));
  } else if(mode=='b'){
    DG_INPUTF(a);
  } else if(mode=='t'){
    DG_MARK_FLOAT(a);
  }
}

template<typename fp>
void bittrick_output(fp& var, fp expected_derivative){
  char mode = DG_GET_MODE;
  if(mode=='d'){
    fp derivative = 0.0;
    DG_GET_DOTVALUE(&var,&derivative,sizeof(fp));
    if( std::fabs((derivative-expected_derivative)/expected_derivative) > 1e-6 ){
      std::cout << "WRONG FORWARD-MODE DERIVATIVE: computed=" << derivative << " expected=" << expected_derivative << std::endl;
    }
  } else if(mode=='b'){
    DG_OUTPUTF(var);
  } else if(mode=='t'){
    // just use the output somehow
    fp volatile var2 = var;
    var2 += (fp)1.0;
  }
}

using ul = unsigned long long;
using ui = unsigned int;

ul as_ul(double val){ return *(ul*)&val; }
ui as_ui(float val){ return *(ui*)&val; }
double as_double(ul val){ return *(double*)&val; }
float as_float(ui val){ return *(float*)&val;} 

void integer_addition_to_exponent_double(){
  double a = 2.7;
  bittrick_input(a);
  ul exponent = 2;
  double b = as_double( as_ul(a) + (exponent<<52) );
  bittrick_output(b,4.0);
}

int main(){
  integer_addition_to_exponent_double();
}



