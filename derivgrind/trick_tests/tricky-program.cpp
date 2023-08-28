
#include <type_traits>
#include <iostream>
#include <cmath>

/*! \struct typeinfo
 * \brief Store information about floating-point types
 * \tparam fp Floating-point type
 */
template<typename fp>
struct typeinfo {
  using uint = void; //!< Unsigned integer type of the same size
};
template<> struct typeinfo<double>{ using type = unsigned long long; };
template<> struct typeinfo<float>{ using type = unsigned int; };

int main(){
  typeinfo<double>::type i = 4;


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
    DG_OUTPUTF(a);
  } else if(mode=='t'){
  }
}

using ul8 = unsigned long long;
using ul4 = unsigned int;

template<typename fp>
std::conditional<std::is_same<fp,double>::value, ul8, ul4> as_int(fp val){
  return 

//! 8-byte unsigned integer type
typedef unsigned long long volatile ul;

//! convert floating-point to unsigned integer data
ul as_ul(double a){
  return *(ul*)&a;
}

//! convert unsigned integer to floating-point data
double as_fp(ul a){
  return *(double*)&a;
}

//! Further processing of a number contaminated with bit-tricks
void use(double a){
  double volatile b = a;
  b += 1.0;
}


bool integer_addition_to_exponent(){
  double a_d = 2.7;
  BITTRICK_INPUT(a_d);
  ul a_i = as_ul(a_d);
  ul n = 2;
  a_i += (n<<52);
  double res = as_fp(a_i);
  BITTRICK_OUTPUT(res, 
}

int main(){
  integer_addition_to_exponent();
}



