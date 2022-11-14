/*! \file performanceTestMacros.hpp
 * Include into the performance test client code.
 * 
 * Then declare inputs as
 *
 *     DOUBLE input = 3;
 *     HANDLE_INPUT(input);
 *
 */

static const double one = 1.;

#if defined(DG_DOT) 
  #include <valgrind/derivgrind.h> 
  #define DOUBLE double
  #define HANDLE_INPUT(var) DG_SET_DOTVALUE(&var, &one, 8);
#elif defined(DG_BAR) 
  #include <valgrind/derivgrind.h>
  #define DOUBLE double
  #define HANDLE_INPUT(var) DG_INPUTF(var)
#elif defined(CODI_DOT)
  #include "codi.hpp"
  #define DOUBLE codi::RealForward
  #define HANDLE_INPUT(var) var.setGradient(one);
#elif defined(CODI_BAR) 
  #include "codi.hpp"
  #define DOUBLE codi::RealReverse
  #define HANDLE_INPUT(var) tape.registerInput(var);
#else // no AD
  #define DOUBLE double
  #define HANDLE_INPUT(var) 
#endif 
