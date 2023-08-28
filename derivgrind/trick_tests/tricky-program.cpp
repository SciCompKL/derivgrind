
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
void bittrick_output(fp& var, fp expected_value, fp expected_derivative){
  DG_DISABLE(1,0); // keep the bit-trick finder from emitting warnings at this place
  if( std::fabs((var-expected_value)/expected_value) > 1e-6 ){
    std::cout << "WRONG VALUE: computed=" << var << " expected=" << expected_value << std::endl;
  }
  DG_DISABLE(0,1);
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

/*! Bit-trick: Multiply with a power of two by making an integer addition 
 * to the exponent bits.
 */
void integer_addition_to_exponent_double(){
  double a = 2.7;
  bittrick_input(a);
  ul exponent = 5;
  double b = as_double( as_ul(a) + (exponent<<52) );
  bittrick_output(b,2.7*32.0,32.0);
}

/*! Bit-trick: See integer_addition_to_exponent_double.
 */
void integer_addition_to_exponent_float(){
  float a = -2.7;
  bittrick_input(a);
  ui exponent = 3;
  float b = as_float( as_ui(a) + (exponent<<23) );
  bittrick_output(b,-2.7f*8.0f,8.0f);
}

/*! Bit-trick: Perform frexp by overwriting exponent bytes with 0b01111111110.
 * 
 * These eleven bits are the exponent bits of all numbers between 0.5 (inclusive)
 * and 1.0 (exclusive). Setting them this way multiplied the value with a power of two,
 * chosen such that the result ends up between 0.5 and 1.0.
 */
void incomplete_masking_to_perform_frexp_double(){
  double a = 38.1;
  bittrick_input(a);
  double b = as_double( (as_ul(a) & 0x800ffffffffffffful) | 0x3fe0000000000000ul );
  bittrick_output(b, 38.1/64.0, 1/64.0);
}

/*! Bit-trick: Perform frexpf by overwriting exponent bytes with 0b01111110.
 *
 * See incomplete_masking_to_perform_frexp_float.
 */
void incomplete_masking_to_perform_frexp_float(){
  float a = -38.1;
  bittrick_input(a);
  float b = as_float( (as_ui(a) & 0x807fffff) | 0x3f000000 );
  bittrick_output(b, -38.1f/64.0f, 1/64.0f);
}

int main(int argc, char* argv[]){
  integer_addition_to_exponent_double();
  integer_addition_to_exponent_float();
  incomplete_masking_to_perform_frexp_double();
  incomplete_masking_to_perform_frexp_float();
}



