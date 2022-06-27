
# \file overloaded_functions.py
#
# Define how to wrap math.h functions in the
# DerivGrind replacement libm.so.
# 
# Basically, each wrapped function is specified by its
# name, parameter/return types, and derivative.
# In order to use original math.h functions for the 
# calculation of the derivative, use the macro
# LIBM(functionname). 
#
# The actual source code is generated in compile.py.
#

class DERIVGRIND_MATH_FUNCTION_BASE:
  def __init__(self,name,type_):
    self.name = name
    self.type = type_
    if self.type=="double":
      self.size = 8
    elif self.type=="float":
      self.size = 4
    elif self.type=="long double":
      self.size = 10
    else:
      print("Unknown type '"+self.type+"'")
      exit(1)
    self.glibc_version = None

class DERIVGRIND_MATH_FUNCTION(DERIVGRIND_MATH_FUNCTION_BASE):
  """Wrap a math.h function to also handle
    the derivative information."""
  def __init__(self,name,deriv,type_):
    super().__init__(name,type_)
    self.deriv = deriv
  def c_code(self):
    return \
f"""
{self.type} {self.name} ({self.type} x){{
  {self.type} x_d;
  VALGRIND_GET_DERIVATIVE(&x,&x_d,{self.size});
  {self.type} ret = LIBM({self.name})(x);
  {self.type} ret_d = ({self.deriv}) * x_d;
  VALGRIND_SET_DERIVATIVE(&ret,&ret_d,{self.size});
  return ret;
}}
"""
  def declaration_original_pointer(self):
    return f"""{self.type} (*LIBM({self.name})) ({self.type}) = NULL;\n"""

class DERIVGRIND_MATH_FUNCTION2(DERIVGRIND_MATH_FUNCTION_BASE):
  """Wrap a math.h function with two arguments
    to also handle the derivative information."""
  def __init__(self,name,derivX,derivY,type_):
    super().__init__(name,type_)
    self.derivX = derivX
    self.derivY = derivY
  def c_code(self):
    return \
f"""
{self.type} {self.name} ({self.type} x, {self.type} y){{
  {self.type} x_d, y_d;
  VALGRIND_GET_DERIVATIVE(&x,&x_d,{self.size});
  VALGRIND_GET_DERIVATIVE(&y,&y_d,{self.size});
  {self.type} ret = LIBM({self.name})(x,y);
  {self.type} ret_d = ({self.derivX}) * x_d + ({self.derivY}) * y_d;
  VALGRIND_SET_DERIVATIVE(&ret,&ret_d,{self.size});
  return ret;
}}
"""
  def declaration_original_pointer(self):
    return f"""{self.type} (*LIBM({self.name})) ({self.type}, {self.type}) = NULL;\n"""

class DERIVGRIND_MATH_FUNCTION2e(DERIVGRIND_MATH_FUNCTION_BASE):
  """Wrap a math.h function on (double,othertype)
    Used for ldexp and frexp."""
  def __init__(self,name,deriv,type_,othertype):
    super().__init__(name,type_)
    self.deriv = deriv
    self.othertype = othertype
  def c_code(self):
    return \
f"""
{self.type} {self.name} ({self.type} x, {self.othertype} e){{
  {self.type} x_d;
  VALGRIND_GET_DERIVATIVE(&x,&x_d,{self.size});
  {self.type} ret = LIBM({self.name})(x,e);
  {self.type} ret_d = ({self.deriv}) * x_d;
  VALGRIND_SET_DERIVATIVE(&ret,&ret_d,{self.size});
  return ret;
}}
"""
  def declaration_original_pointer(self):
    return f"""{self.type} (*LIBM({self.name})) ({self.type}, {self.othertype}) = NULL;\n"""

functions = [

  # missing: modf
  DERIVGRIND_MATH_FUNCTION("acos","-1/LIBM(sqrt)(1-x*x)","double"),
  DERIVGRIND_MATH_FUNCTION("asin","1/LIBM(sqrt)(1-x*x)","double"),
  DERIVGRIND_MATH_FUNCTION("atan","1/(1+x*x)","double"),
  DERIVGRIND_MATH_FUNCTION2("atan2","-y/(x*x+y*y)","x/(x*x+y*y)","double"),
  DERIVGRIND_MATH_FUNCTION("ceil","0","double"),
  DERIVGRIND_MATH_FUNCTION("cos", "-LIBM(sin)(x)","double"),
  DERIVGRIND_MATH_FUNCTION("cosh", "LIBM(sinh)(x)","double"),
  DERIVGRIND_MATH_FUNCTION("exp", "LIBM(exp)(x)","double"),
  DERIVGRIND_MATH_FUNCTION("fabs", "(x>0?1:-1)","double"),
  DERIVGRIND_MATH_FUNCTION("floor", "0","double"),
  DERIVGRIND_MATH_FUNCTION2("fmod", "1", "- LIBM(floor)(LIBM(fabs)(x/y)) * (x>0?1.:-1.) * (y>0?1.:-1.)","double"),
  DERIVGRIND_MATH_FUNCTION2e("frexp","LIBM(ldexp)(1,-*e)","double","int*"),
  DERIVGRIND_MATH_FUNCTION2e("ldexp","LIBM(ldexp)(1,e)","double","int"),
  DERIVGRIND_MATH_FUNCTION("log","1/x","double"),
  DERIVGRIND_MATH_FUNCTION("log10", "1/(LIBM(log)(10)*x)","double"),
  DERIVGRIND_MATH_FUNCTION2("pow"," (y==0||y==-0)?0:(y*LIBM(pow)(x,y-1))", "(x<=0) ? 0 : (LIBM(pow)(x,y)*LIBM(log)(x))","double"), # maybe assert that yd is integer if x<=0 ?
  DERIVGRIND_MATH_FUNCTION("sin", "LIBM(cos)(x)","double"),
  DERIVGRIND_MATH_FUNCTION("sinh", "LIBM(cosh)(x)","double"),
  DERIVGRIND_MATH_FUNCTION("sqrt", "1/(2*LIBM(sqrt)(x))","double"),
  DERIVGRIND_MATH_FUNCTION("tan", "1/(LIBM(cos)(x)*LIBM(cos)(x))","double"),
  DERIVGRIND_MATH_FUNCTION("tanh", "1-LIBM(tanh)(x)*LIBM(tanh)(x)","double"),

  # float versions
  DERIVGRIND_MATH_FUNCTION("acosf","-1/LIBM(sqrtf)(1-x*x)","float"),
  DERIVGRIND_MATH_FUNCTION("asinf","1/LIBM(sqrtf)(1-x*x)","float"),
  DERIVGRIND_MATH_FUNCTION("atanf","1/(1+x*x)","float"),
  DERIVGRIND_MATH_FUNCTION2("atan2f","-y/(x*x+y*y)","x/(x*x+y*y)","float"),
  DERIVGRIND_MATH_FUNCTION("ceilf","0","float"),
  DERIVGRIND_MATH_FUNCTION("cosf", "-LIBM(sinf)(x)","float"),
  DERIVGRIND_MATH_FUNCTION("coshf", "LIBM(sinhf)(x)","float"),
  DERIVGRIND_MATH_FUNCTION("expf", "LIBM(expf)(x)","float"),
  DERIVGRIND_MATH_FUNCTION("fabsf", "(x>0?1:-1)","float"),
  DERIVGRIND_MATH_FUNCTION("floorf", "0","float"),
  DERIVGRIND_MATH_FUNCTION2("fmodf", "1", "- LIBM(floorf)(LIBM(fabsf)(x/y)) * (x>0?1.f:-1.f) * (y>0?1.f:-1.f)","float"),
  DERIVGRIND_MATH_FUNCTION2e("frexpf","LIBM(ldexpf)(1,-*e)","float","int*"),
  DERIVGRIND_MATH_FUNCTION2e("ldexpf","LIBM(ldexpf)(1,e)","float","int"),
  DERIVGRIND_MATH_FUNCTION("logf","1/x","float"),
  DERIVGRIND_MATH_FUNCTION("log10f", "1/(LIBM(logf)(10)*x)","float"),
  DERIVGRIND_MATH_FUNCTION2("powf"," (y==0||y==-0)?0:(y*LIBM(powf)(x,y-1))", "(x<=0) ? 0 : (LIBM(powf)(x,y)*LIBM(logf)(x))","float"),
  DERIVGRIND_MATH_FUNCTION("sinf", "LIBM(cosf)(x)","float"),
  DERIVGRIND_MATH_FUNCTION("sinhf", "LIBM(coshf)(x)","float"),
  DERIVGRIND_MATH_FUNCTION("sqrtf", "1/(2*LIBM(sqrtf)(x))","float"),
  DERIVGRIND_MATH_FUNCTION("tanf", "1/(LIBM(cosf)(x)*LIBM(cosf)(x))","float"),
  DERIVGRIND_MATH_FUNCTION("tanhf", "1-LIBM(tanhf)(x)*LIBM(tanhf)(x)","float"),

  # long double versions
  DERIVGRIND_MATH_FUNCTION("acosl","-1/LIBM(sqrtl)(1-x*x)","long double"),
  DERIVGRIND_MATH_FUNCTION("asinl","1/LIBM(sqrtl)(1-x*x)","long double"),
  DERIVGRIND_MATH_FUNCTION("atanl","1/(1+x*x)","long double"),
  DERIVGRIND_MATH_FUNCTION2("atan2l","-y/(x*x+y*y)","x/(x*x+y*y)","long double"),
  DERIVGRIND_MATH_FUNCTION("ceill","0","long double"),
  DERIVGRIND_MATH_FUNCTION("cosl", "-LIBM(sinl)(x)","long double"),
  DERIVGRIND_MATH_FUNCTION("coshl", "LIBM(sinhl)(x)","long double"),
  DERIVGRIND_MATH_FUNCTION("expl", "LIBM(expl)(x)","long double"),
  DERIVGRIND_MATH_FUNCTION("fabsl", "(x>0?1:-1)","long double"),
  DERIVGRIND_MATH_FUNCTION("floorl", "0","long double"),
  DERIVGRIND_MATH_FUNCTION2("fmodl", "1", "- LIBM(floorl)(LIBM(fabsl)(x/y)) * (x>0?1.l:-1.l) * (y>0?1.l:-1.l)","long double"),
  DERIVGRIND_MATH_FUNCTION2e("frexpl","LIBM(ldexpl)(1,-*e)","long double","int*"),
  DERIVGRIND_MATH_FUNCTION2e("ldexpl","LIBM(ldexpl)(1,e)","long double","int"),
  DERIVGRIND_MATH_FUNCTION("logl","1/x","long double"),
  DERIVGRIND_MATH_FUNCTION("log10l", "1/(LIBM(logl)(10)*x)","long double"),
  DERIVGRIND_MATH_FUNCTION2("powl"," (y==0||y==-0)?0:(y*LIBM(powl)(x,y-1))", "(x<=0) ? 0 : (LIBM(powl)(x,y)*LIBM(logl)(x))","long double"),
  DERIVGRIND_MATH_FUNCTION("sinl", "LIBM(cosl)(x)","long double"),
  DERIVGRIND_MATH_FUNCTION("sinhl", "LIBM(coshl)(x)","long double"),
  DERIVGRIND_MATH_FUNCTION("sqrtl", "1/(2*LIBM(sqrtl)(x))","long double"),
  DERIVGRIND_MATH_FUNCTION("tanl", "1/(LIBM(cosl)(x)*LIBM(cosl)(x))","long double"),
  DERIVGRIND_MATH_FUNCTION("tanhl", "1-LIBM(tanhl)(x)*LIBM(tanhl)(x)","long double")

]






