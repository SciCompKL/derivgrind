#include <pybind11/pybind11.h>
#include <valgrind/derivgrind.h>

namespace py = pybind11;

PYBIND11_MODULE(derivgrind, m){
  m.doc() = "Wrapper for DerivGrind client requests.";

  m.def( "set_derivative", [](double val, double grad)->double { 
    double ret = val; 
    VALGRIND_SET_DERIVATIVE(&ret, &grad, 8);
    return ret; 
  });
  m.def( "get_derivative", [](double val)->double { 
    double grad; 
    VALGRIND_GET_DERIVATIVE(&val, &grad, 8);
    return grad; 
  });
  m.def( "set_derivative", [](float val, float grad)->float { 
    float ret = val; 
    VALGRIND_SET_DERIVATIVE(&ret, &grad, 4);
    return ret; 
  });
  m.def( "get_derivative", [](float val)->float { 
    float grad; 
    VALGRIND_GET_DERIVATIVE(&val, &grad, 4);
    return grad; 
  });
}
