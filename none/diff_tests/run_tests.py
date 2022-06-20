import numpy as np
import copy
from TestCase import InteractiveTestCase, ClientRequestTestCase, TYPE_DOUBLE, TYPE_FLOAT, TYPE_LONG_DOUBLE
import sys
import os

selected_testcase = None
if len(sys.argv)>2:
  printf("Usage: "+sys.argv[0]+"                      - Run all testcases.")
  printf("       "+sys.argv[0]+" [name of testcase]   - Run single testcase.")
  exit(1)
elif len(sys.argv)==2:
  selected_testcase = sys.argv[1]


# We first define a list of "basic" tests.
# The actual testlist is derived from it by additionally
# configuring
# - the instruction set (x86 / amd64)
# - the floating-point type (double / float / long double)
# - whether gcc or g++ is used for compilation
#
# Then all these tests are run in the end of the script,
# or, if a command-line argument is specified, only
# that single test.
basiclist = []

### Basic arithmetic operations ###

addition = ClientRequestTestCase("addition")
addition.stmtd = "double c = a+b;"
addition.stmtf = "float c = a+b;"
addition.stmtl = "long double c = a+b;"
addition.vals = {'a':1.0,'b':2.0}
addition.grads = {'a':3.0,'b':4.0}
addition.test_vals = {'c':3.0}
addition.test_grads = {'c':7.0}
basiclist.append(addition)

addition_const_l = ClientRequestTestCase("addition_const_l")
addition_const_l.stmtd = "double c = 0.3 + a;"
addition_const_l.stmtf = "float c = 0.3 + a;"
addition_const_l.stmtl = "long double c = 0.3 + a;"
addition_const_l.vals = {'a':1.0}
addition_const_l.grads = {'a':2.0}
addition_const_l.test_vals = {'c':1.3}
addition_const_l.test_grads = {'c':2.0}
basiclist.append(addition_const_l)

addition_const_r = ClientRequestTestCase("addition_const_r")
addition_const_r.stmtd = "double c = a + 0.3;"
addition_const_r.stmtf = "float c = a + 0.3;"
addition_const_r.stmtl = "long double c = a + 0.3;"
addition_const_r.vals = {'a':1.0}
addition_const_r.grads = {'a':2.0}
addition_const_r.test_vals = {'c':1.3}
addition_const_r.test_grads = {'c':2.0}
basiclist.append(addition_const_r)

subtraction = ClientRequestTestCase("subtraction")
subtraction.stmtd = "double c = a-b;"
subtraction.stmtf = "float c = a-b;"
subtraction.stmtl = "long double c = a-b;"
subtraction.vals = {'a':1.0,'b':2.0}
subtraction.grads = {'a':3.0,'b':4.0}
subtraction.test_vals = {'c':-1.0}
subtraction.test_grads = {'c':-1.0}
basiclist.append(subtraction)

subtraction_const_l = ClientRequestTestCase("subtraction_const_l")
subtraction_const_l.stmtd = "double c = 0.3 - a;"
subtraction_const_l.stmtf = "float c = 0.3 - a;"
subtraction_const_l.stmtl = "long double c = 0.3 - a;"
subtraction_const_l.vals = {'a':1.0}
subtraction_const_l.grads = {'a':2.0}
subtraction_const_l.test_vals = {'c':-0.7}
subtraction_const_l.test_grads = {'c':-2.0}
basiclist.append(subtraction_const_l)

subtraction_const_r = ClientRequestTestCase("subtraction_const_r")
subtraction_const_r.stmtd = "double c = a - 0.5;"
subtraction_const_r.stmtf = "float c = a - 0.5;"
subtraction_const_r.stmtl = "long double c = a - 0.5;"
subtraction_const_r.vals = {'a':1.0}
subtraction_const_r.grads = {'a':2.0}
subtraction_const_r.test_vals = {'c':0.5}
subtraction_const_r.test_grads = {'c':2.0}
basiclist.append(subtraction_const_r)

multiplication = ClientRequestTestCase("multiplication")
multiplication.stmtd = "double c = a*b;"
multiplication.stmtf = "float c = a*b;"
multiplication.stmtl = "long double c = a*b;"
multiplication.vals = {'a':1.0,'b':2.0}
multiplication.grads = {'a':3.0,'b':4.0}
multiplication.test_vals = {'c':2.0}
multiplication.test_grads = {'c':10.0}
basiclist.append(multiplication)

multiplication_const_l = ClientRequestTestCase("multiplication_const_l")
multiplication_const_l.stmtd = "double c = 0.3 * a;"
multiplication_const_l.stmtf = "float c = 0.3 * a;"
multiplication_const_l.stmtl = "long double c = 0.3 * a;"
multiplication_const_l.vals = {'a':2.0}
multiplication_const_l.grads = {'a':3.0}
multiplication_const_l.test_vals = {'c':0.6}
multiplication_const_l.test_grads = {'c':0.9}
basiclist.append(multiplication_const_l)

multiplication_const_r = ClientRequestTestCase("multiplication_const_r")
multiplication_const_r.stmtd = "double c = a * 0.5;"
multiplication_const_r.stmtf = "float c = a * 0.5;"
multiplication_const_r.stmtl = "long double c = a * 0.5;"
multiplication_const_r.vals = {'a':2.0}
multiplication_const_r.grads = {'a':3.0}
multiplication_const_r.test_vals = {'c':1.0}
multiplication_const_r.test_grads = {'c':1.5}
basiclist.append(multiplication_const_r)

division = ClientRequestTestCase("division")
division.stmtd = "double c = a/b;"
division.stmtf = "float c = a/b;"
division.stmtl = "long double c = a/b;"
division.vals = {'a':1.0,'b':2.0}
division.grads = {'a':5.0,'b':4.0}
division.test_vals = {'c':0.5}
division.test_grads = {'c':1.5}
basiclist.append(division)

division_const_l = ClientRequestTestCase("division_const_l")
division_const_l.stmtd = "double c = 0.3 / a;"
division_const_l.stmtf = "float c = 0.3 / a;"
division_const_l.stmtl = "long double c = 0.3 / a;"
division_const_l.vals = {'a':2.0}
division_const_l.grads = {'a':3.0}
division_const_l.test_vals = {'c':0.15}
division_const_l.test_grads = {'c':-9/40}
basiclist.append(division_const_l)

division_const_r = ClientRequestTestCase("division_const_r")
division_const_r.stmtd = "double c = a / 0.5;"
division_const_r.stmtf = "float c = a / 0.5;"
division_const_r.stmtl = "long double c = a / 0.5;"
division_const_r.vals = {'a':2.0}
division_const_r.grads = {'a':3.0}
division_const_r.test_vals = {'c':4.0}
division_const_r.test_grads = {'c':6.0}
basiclist.append(division_const_r)

### Advances arithmetic and trigonometric operations ###

abs_plus = ClientRequestTestCase("abs_plus")
abs_plus.include = "#include <math.h>"
abs_plus.ldflags = '-lm'
#abs_minus.cflags = '-fno-builtin' # might be advisable because the builtin fabs is difficult to differentiate
abs_plus.stmtd = "double c = fabs(a);"
abs_plus.stmtf = "float c = fabsf(a);"
abs_plus.stmtl = "long double c = fabsl(a);"
abs_plus.vals = {'a':1.0}
abs_plus.grads = {'a':2.0}
abs_plus.test_vals = {'c':1.0}
abs_plus.test_grads = {'c':2.0}
basiclist.append(abs_plus)

abs_minus = ClientRequestTestCase("abs_minus")
abs_minus.include = "#include <math.h>"
abs_minus.ldflags = '-lm'
#abs_minus.cflags = '-fno-builtin' # might be advisable because the builtin fabs is difficult to differentiate
abs_minus.stmtd = "double c = fabs(a);"
abs_minus.stmtf = "float c = fabsf(a);"
abs_minus.stmtl = "long double c = fabsl(a);"
abs_minus.vals = {'a':-1.0}
abs_minus.grads = {'a':2.0}
abs_minus.test_vals = {'c':1.0}
abs_minus.test_grads = {'c':-2.0}
basiclist.append(abs_minus)

sqrt = ClientRequestTestCase("sqrt")
sqrt.include = "#include <math.h>"
sqrt.ldflags = '-lm'
sqrt.stmtd = "double c = sqrt(a);"
sqrt.stmtf = "float c = sqrtf(a);"
sqrt.stmtl = "long double c = sqrtl(a);"
sqrt.vals = {'a':4.0}
sqrt.grads = {'a':1.0}
sqrt.test_vals = {'c':2.0}
sqrt.test_grads = {'c':0.25}
basiclist.append(sqrt)

# if pow(a,b) is implemented as a*a for b==2., 
# the gradient of b would be discarded
pow_2 = ClientRequestTestCase("pow_2") 
pow_2.include = "#include <math.h>"
pow_2.ldflags = '-lm'
pow_2.stmtd = "double c = pow(a,b);"
pow_2.stmtf = "float c = powf(a,b);"
pow_2.stmtl = "long double c = powl(a,b);"
pow_2.vals = {'a':4.0,'b':2.0}
pow_2.grads = {'a':1.6,'b':1.9}
pow_2.test_vals = {'c':4.0**2}
pow_2.test_grads = {'c':1.6*2*4.0 + 1.9*4.0**2*np.log(4)}
basiclist.append(pow_2)

pow_both = ClientRequestTestCase("pow_both")
pow_both.include = "#include <math.h>"
pow_both.ldflags = '-lm'
pow_both.stmtd = "double c = pow(a,b);"
pow_both.stmtf = "float c = powf(a,b);"
pow_both.stmtl = "long double c = powl(a,b);"
pow_both.vals = {'a':4.0,'b':3.0}
pow_both.grads = {'a':1.6,'b':1.9}
pow_both.test_vals = {'c':4.0**3.0}
pow_both.test_grads = {'c':1.6*3*4.0**2 + 1.9*4.0**3.0*np.log(4)}
basiclist.append(pow_both)

for angle,angletext in [(0,"0"), (1e-3,"1m"), (1e-2,"10m"), (1e-1,"100m"), (1.,"1"), (-10.,"neg10"), (100.,"100")]:
  sin = ClientRequestTestCase("sin_"+angletext)
  sin.include = "#include <math.h>"
  sin.ldflags = '-lm'
  sin.stmtd = "double c = sin(a);"
  sin.stmtf = "float c = sinf(a);"
  sin.stmtl = "long double c = sinl(a);"
  sin.vals = {'a':angle}
  sin.grads = {'a':3.1}
  sin.test_vals = {'c':np.sin(angle)}
  sin.test_grads = {'c':np.cos(angle)*3.1}
  basiclist.append(sin)

  cos = ClientRequestTestCase("cos_"+angletext)
  cos.include = "#include <math.h>"
  cos.ldflags = '-lm'
  cos.stmtd = "double c = cos(a);"
  cos.stmtf = "float c = cosf(a);"
  cos.stmtl = "long double c = cosl(a);"
  cos.vals = {'a':angle}
  cos.grads = {'a':2.7}
  cos.test_vals = {'c':np.cos(angle)}
  cos.test_grads = {'c':-np.sin(angle)*2.7}
  basiclist.append(cos)

  tan = ClientRequestTestCase("tan_"+angletext)
  tan.include = "#include <math.h>"
  tan.ldflags = '-lm'
  tan.stmtd = "double c = tan(a);"
  tan.stmtf = "float c = tanf(a);"
  tan.stmtl = "long double c = tanl(a);"
  tan.vals = {'a':angle}
  tan.grads = {'a':1.0}
  tan.test_vals = {'c':np.tan(angle)}
  tan.test_grads = {'c':1./np.cos(angle)**2}
  basiclist.append(tan)

exp = ClientRequestTestCase("exp")
exp.include = "#include <math.h>"
exp.ldflags = '-lm'
exp.stmtd = "double c = exp(a);"
exp.stmtf = "float c = expf(a);"
exp.stmtl = "long double c = expl(a);"
exp.vals = {'a':4}
exp.grads = {'a':5.0}
exp.test_vals = {'c':np.exp(4)}
exp.test_grads = {'c':np.exp(4)*5.0}
basiclist.append(exp)

log = ClientRequestTestCase("log")
log.include = "#include <math.h>"
log.ldflags = '-lm'
log.stmtd = "double c = log(a);"
log.stmtf = "float c = logf(a);"
log.stmtl = "long double c = logl(a);"
log.vals = {'a':20}
log.grads = {'a':1.0}
log.test_vals = {'c':np.log(20)}
log.test_grads = {'c':0.05}
basiclist.append(log)

log10 = ClientRequestTestCase("log10")
log10.include = "#include <math.h>"
log10.ldflags = '-lm'
log10.stmtd = "double c = log10(a);"
log10.stmtf = "float c = log10f(a);"
log10.stmtl = "long double c = log10l(a);"
log10.vals = {'a':0.01}
log10.grads = {'a':1.0}
log10.test_vals = {'c':-2}
log10.test_grads = {'c':100/np.log(10)}
basiclist.append(log10)

sinh = ClientRequestTestCase("sinh")
sinh.include = "#include <math.h>"
sinh.ldflags = '-lm'
sinh.stmtd = "double c = sinh(a);"
sinh.stmtf = "float c = sinhf(a);"
sinh.stmtl = "long double c = sinhl(a);"
sinh.vals = {'a':2.0}
sinh.grads = {'a':1.0}
sinh.test_vals = {'c':np.sinh(2.0)}
sinh.test_grads = {'c':np.cosh(2.0)}
basiclist.append(sinh)

cosh = ClientRequestTestCase("cosh")
cosh.include = "#include <math.h>"
cosh.ldflags = '-lm'
cosh.stmtd = "double c = cosh(a);"
cosh.stmtf = "float c = coshf(a);"
cosh.stmtl = "long double c = coshl(a);"
cosh.vals = {'a':-2.0}
cosh.grads = {'a':1.0}
cosh.test_vals = {'c':np.cosh(-2.0)}
cosh.test_grads = {'c':np.sinh(-2.0)}
basiclist.append(cosh)

tanh = ClientRequestTestCase("tanh")
tanh.include = "#include <math.h>"
tanh.ldflags = '-lm'
tanh.stmtd = "double c = tanh(a);"
tanh.stmtf = "float c = tanhf(a);"
tanh.stmtl = "long double c = tanhl(a);"
tanh.vals = {'a':-0.5}
tanh.grads = {'a':1.0}
tanh.test_vals = {'c':np.tanh(-0.5)}
tanh.test_grads = {'c':1-np.tanh(-0.5)**2}
basiclist.append(tanh)

asin = ClientRequestTestCase("asin")
asin.include = "#include <math.h>"
asin.ldflags = '-lm'
asin.stmtd = "double c = asin(a);"
asin.stmtf = "float c = asinf(a);"
asin.stmtl = "long double c = asinl(a);"
asin.vals = {'a':0.9}
asin.grads = {'a':1.0}
asin.test_vals = {'c':np.arcsin(0.9)}
asin.test_grads = {'c':1/np.sqrt(1-0.9**2)}
basiclist.append(asin)

acos = ClientRequestTestCase("acos")
acos.include = "#include <math.h>"
acos.ldflags = '-lm'
acos.stmtd = "double c = acos(a);"
acos.stmtf = "float c = acosf(a);"
acos.stmtl = "long double c = acosl(a);"
acos.vals = {'a':-0.4}
acos.grads = {'a':1.0}
acos.test_vals = {'c':np.arccos(-0.4)}
acos.test_grads = {'c':-1/np.sqrt(1-(-0.4)**2)}
basiclist.append(acos)

atan = ClientRequestTestCase("atan")
atan.include = "#include <math.h>"
atan.ldflags = '-lm'
atan.stmtd = "double c = atan(a);"
atan.stmtf = "float c = atanf(a);"
atan.stmtl = "long double c = atanl(a);"
atan.vals = {'a':100}
atan.grads = {'a':1.0}
atan.test_vals = {'c':np.arctan(100)}
atan.test_grads = {'c':1/(1+100**2)}
basiclist.append(atan)

atan2 = ClientRequestTestCase("atan2")
atan2.include = "#include <math.h>"
atan2.ldflags = '-lm'
atan2.stmtd = "double c = atan2(a,b);"
atan2.stmtf = "float c = atan2f(a,b);"
atan2.stmtl = "long double c = atan2l(a,b);"
atan2.vals = {'a':3,'b':4}
atan2.grads = {'a':1.3, 'b':1.5}
atan2.test_vals = {'c':np.arctan2(3,4)}
atan2.test_grads = {'c':1.3*(-4)/(3**2+4**2) + 1.5*3/(3**2+4**2)}
basiclist.append(atan2)

floor = ClientRequestTestCase("floor")
floor.include = "#include <math.h>"
floor.ldflags = '-lm'
floor.stmtd = "double c = floor(a);"
floor.stmtf = "float c = floorf(a);"
floor.stmtl = "long double c = floorl(a);"
floor.vals = {'a':2.0}
floor.grads = {'a':1.0}
floor.test_vals = {'c':2.0}
floor.test_grads = {'c':0.0}
basiclist.append(floor)

ceil = ClientRequestTestCase("ceil")
ceil.include = "#include <math.h>"
ceil.ldflags = '-lm'
ceil.stmtd = "double c = ceil(a);"
ceil.stmtf = "float c = ceilf(a);"
ceil.stmtl = "long double c = ceill(a);"
ceil.vals = {'a':2.1}
ceil.grads = {'a':1.0}
ceil.test_vals = {'c':3.0}
ceil.test_grads = {'c':0.0}
basiclist.append(ceil)


### Memory operations from string.h ###

memcpy = ClientRequestTestCase("memcpy")
memcpy.include = "#include <string.h>"
memcpy.stmtd = "double aa[3],ac[3],c; aa[1] = a; memcpy(ac,aa,3*sizeof(double)); c=ac[1];"
memcpy.stmtf = "float aa[3],ac[3],c; aa[1] = a; memcpy(ac,aa,3*sizeof(float)); c=ac[1];"
memcpy.stmtl = "long double aa[3],ac[3],c; aa[1] = a; memcpy(ac,aa,3*sizeof(long double)); c=ac[1];"
memcpy.vals = {'a':-12.34}
memcpy.grads = {'a':-56.78}
memcpy.test_vals = {'c':-12.34}
memcpy.test_grads = {'c':-56.78}
basiclist.append(memcpy)

memmove = ClientRequestTestCase("memmove")
memmove.include = "#include <string.h>"
memmove.stmtd = "double aa[3],c; aa[0] = 3.14*a; aa[1] = a; memmove(aa+1,aa,2*sizeof(double)); c=aa[1];"
memmove.stmtf = "float aa[3],c; aa[0] = 3.14*a; aa[1] = a; memmove(aa+1,aa,2*sizeof(float)); c=aa[1];"
memmove.stmtl = "long double aa[3],c; aa[0] = 3.14*a; aa[1] = a; memmove(aa+1,aa,2*sizeof(long double)); c=aa[1];"
memmove.vals = {'a':-12.34}
memmove.grads = {'a':-56.78}
memmove.test_vals = {'c':-12.34*3.14}
memmove.test_grads = {'c':-56.78*3.14}
basiclist.append(memmove)

memset = ClientRequestTestCase("memset")
memset.include = "#include <string.h>"
memset.stmtd = "memset(&a,0,sizeof(double));"
memset.stmtf = "memset(&a,0,sizeof(float));"
memset.stmtl = "memset(&a,0,sizeof(long double));"
memset.vals = {'a':-12.34}
memset.grads = {'a':-56.78}
memset.test_vals = {'a':0.0}
memset.test_grads = {'a':0.0}
basiclist.append(memset)


### Control structures ###

ifbranch = ClientRequestTestCase("ifbranch")
ifbranch.stmtd = "double c; if(a<1) c = 2+a; else c = 2*a; "
ifbranch.stmtf = "float c; if(a<1) c = 2+a; else c = 2*a; "
ifbranch.stmtl = "long double c; if(a<1) c = 2+a; else c = 2*a; "
ifbranch.vals = {'a':0.0}
ifbranch.grads = {'a':1.0}
ifbranch.test_vals = {'c':2.0}
ifbranch.test_grads = {'c':1.0}
basiclist.append(ifbranch)

elsebranch = ClientRequestTestCase("elsebranch")
elsebranch.stmtd = "double c; if(a<-1) c = 2+a; else c = 2*a; "
elsebranch.stmtf = "float c; if(a<-1) c = 2+a; else c = 2*a; "
elsebranch.stmtl = "long double c; if(a<-1) c = 2+a; else c = 2*a; "
elsebranch.vals = {'a':0.0}
elsebranch.grads = {'a':1.0}
elsebranch.test_vals = {'c':0.0}
elsebranch.test_grads = {'c':2.0}
basiclist.append(elsebranch)

ternary_true = ClientRequestTestCase("ternary_true")
ternary_true.stmtd = "double c = (a>-1) ? (3*a) : (a*a);"
ternary_true.stmtf = "float c = (a>-1) ? (3*a) : (a*a);"
ternary_true.stmtl = "long double c = (a>-1) ? (3*a) : (a*a);"
ternary_true.vals = {'a':10.0}
ternary_true.grads = {'a':1.0}
ternary_true.test_vals = {'c':30.0}
ternary_true.test_grads = {'c':3.0}
basiclist.append(ternary_true)

ternary_false = ClientRequestTestCase("ternary_false")
ternary_false.stmtd = "double c = (a>-1) ? (3*a) : (a*a);"
ternary_false.stmtf = "float c = (a>-1) ? (3*a) : (a*a);"
ternary_false.stmtl = "long double c = (a>-1) ? (3*a) : (a*a);"
ternary_false.vals = {'a':-10.0}
ternary_false.grads = {'a':1.0}
ternary_false.test_vals = {'c':100.0}
ternary_false.test_grads = {'c':-20.0}
basiclist.append(ternary_false)


addition_forloop = ClientRequestTestCase("addition_forloop")
addition_forloop.stmtd = "double c = 0; for(int i=0; i<10; i++) c+=a;"
addition_forloop.stmtf = "float c = 0; for(int i=0; i<10; i++) c+=a;"
addition_forloop.stmtl = "long double c = 0; for(int i=0; i<10; i++) c+=a;"
addition_forloop.vals = {'a':2.0}
addition_forloop.grads = {'a':1.0}
addition_forloop.test_vals = {'c':20.0}
addition_forloop.test_grads = {'c':10.0}
basiclist.append(addition_forloop)

multiplication_forloop = ClientRequestTestCase("multiplication_forloop")
multiplication_forloop.stmtd = "double c = 1; for(int i=0; i<10; i++) c*=a;"
multiplication_forloop.stmtf = "float c = 1; for(int i=0; i<10; i++) c*=a;"
multiplication_forloop.stmtl = "long double c = 1; for(int i=0; i<10; i++) c*=a;"
multiplication_forloop.vals = {'a':2.0}
multiplication_forloop.grads = {'a':1.0}
multiplication_forloop.test_vals = {'c':1024.0}
multiplication_forloop.test_grads = {'c':5120.0}
basiclist.append(multiplication_forloop)

addition_whileloop = ClientRequestTestCase("addition_whileloop")
addition_whileloop.stmtd = "double c = 0; while(c<19) c+=a;"
addition_whileloop.stmtf = "float c = 0; while(c<19) c+=a;"
addition_whileloop.stmtl = "long double c = 0; while(c<19) c+=a;"
addition_whileloop.vals = {'a':2.0}
addition_whileloop.grads = {'a':1.0}
addition_whileloop.test_vals = {'c':20.0}
addition_whileloop.test_grads = {'c':10.0}
basiclist.append(addition_whileloop)

multiplication_whileloop = ClientRequestTestCase("multiplication_whileloop")
multiplication_whileloop.stmtd = "double c = 1; while(c<1023) c*=a;"
multiplication_whileloop.stmtf = "float c = 1; while(c<1023) c*=a;"
multiplication_whileloop.stmtl = "long double c = 1; while(c<1023) c*=a;"
multiplication_whileloop.vals = {'a':2.0}
multiplication_whileloop.grads = {'a':1.0}
multiplication_whileloop.test_vals = {'c':1024.0}
multiplication_whileloop.test_grads = {'c':5120.0}
basiclist.append(multiplication_whileloop)

addition_dowhileloop = ClientRequestTestCase("addition_dowhileloop")
addition_dowhileloop.stmtd = "double c = 0; do c+=a; while(c<19);"
addition_dowhileloop.stmtf = "float c = 0; do c+=a; while(c<19);"
addition_dowhileloop.stmtl = "long double c = 0; do c+=a; while(c<19);"
addition_dowhileloop.vals = {'a':2.0}
addition_dowhileloop.grads = {'a':1.0}
addition_dowhileloop.test_vals = {'c':20.0}
addition_dowhileloop.test_grads = {'c':10.0}
basiclist.append(addition_dowhileloop)

multiplication_dowhileloop = ClientRequestTestCase("multiplication_dowhileloop")
multiplication_dowhileloop.stmtd = "double c = 1; do c*=a; while(c<1023);"
multiplication_dowhileloop.stmtf = "float c = 1; do c*=a; while(c<1023);"
multiplication_dowhileloop.stmtl = "long double c = 1; do c*=a; while(c<1023);"
multiplication_dowhileloop.vals = {'a':2.0}
multiplication_dowhileloop.grads = {'a':1.0}
multiplication_dowhileloop.test_vals = {'c':1024.0}
multiplication_dowhileloop.test_grads = {'c':5120.0}
basiclist.append(multiplication_dowhileloop)

addition_recursion = ClientRequestTestCase("addition_recursion")
addition_recursion.include = """
  double f(int n, double x){ if(n==0) return 0.; else return x+f(n-1,x); }
  float ff(int n, float x){ if(n==0) return 0.f; else return x+ff(n-1,x); }
  long double fl(int n, long double x){ if(n==0) return 0.l; else return x+fl(n-1,x); }
"""
addition_recursion.stmtd = "double c = f(10,a);"
addition_recursion.stmtf = "float c = ff(10,a);"
addition_recursion.stmtl = "long double c = fl(10,a);"
addition_recursion.vals = {'a':2.0}
addition_recursion.grads = {'a':1.0}
addition_recursion.test_vals = {'c':20.0}
addition_recursion.test_grads = {'c':10.0}
basiclist.append(addition_recursion)

multiplication_recursion = ClientRequestTestCase("multiplication_recursion")
multiplication_recursion.include = """
  double f(int n, double x){ if(n==0) return 1.; else return x*f(n-1,x); }
  float ff(int n, float x){ if(n==0) return 1.f; else return x*ff(n-1,x); }
  long double fl(int n, long double x){ if(n==0) return 1.l; else return x*fl(n-1,x); }
"""
multiplication_recursion.stmtd = "double c = f(10,a);"
multiplication_recursion.stmtf = "float c = ff(10,a);"
multiplication_recursion.stmtl = "long double c = fl(10,a);"
multiplication_recursion.vals = {'a':2.0}
multiplication_recursion.grads = {'a':1.0}
multiplication_recursion.test_vals = {'c':1024.0}
multiplication_recursion.test_grads = {'c':5120.0}
basiclist.append(multiplication_recursion)

### OpenMP ###

# The following test succeeds, but variations of
# it fail. E.g. you cannot put a reduction clause or
# an  atomic directive instead of the critical 
# directive.
omp = ClientRequestTestCase("omp")
omp.cflags = "-fopenmp -lm"
omp.include = "#include <omp.h>\n#include <math.h>"
omp.stmtd = """
double sum=0;
#pragma omp parallel for
for(int i=0; i<100; i++){
  double s = sin(i*a);
  #pragma omp critical
  {
    sum += s;
  }
}
"""
omp.stmtf = None
omp.stmtl = None
omp.vals = {'a':1.0}
omp.grads = {'a':3.0}
omp_test_sum_val=0
omp_test_sum_grad=0
for i in range(100):
  omp_test_sum_val += np.sin(i*1.0)
  omp_test_sum_grad += np.cos(i*1.0)*i*3.0
omp.test_vals = {'sum':omp_test_sum_val}
omp.test_grads = {'sum':omp_test_sum_grad}
basiclist.append(omp)


### Interactive tests ###
addition_interactive = InteractiveTestCase("addition_interactive")
addition_interactive.stmtd = "double c = a+b;"
addition_interactive.stmtf = "float c = a+b;"
addition_interactive.stmtl = "long double c = a+b;"
addition_interactive.vals = {'a':1.0,'b':2.0}
addition_interactive.grads = {'a':3.0,'b':4.0}
addition_interactive.test_vals = {'c':3.0}
addition_interactive.test_grads = {'c':7.0}
basiclist.append(addition_interactive)

multiplication_interactive = InteractiveTestCase("multiplication_interactive")
multiplication_interactive.stmtd = "double c = a*b;"
multiplication_interactive.stmtf = "float c = a*b;"
multiplication_interactive.stmtl = "long double c = a*b;"
multiplication_interactive.vals = {'a':1.0,'b':2.0}
multiplication_interactive.grads = {'a':3.0,'b':4.0}
multiplication_interactive.test_vals = {'c':2.0}
multiplication_interactive.test_grads = {'c':10.0}
basiclist.append(multiplication_interactive)

sin_100_interactive = InteractiveTestCase("sin_100_interactive")
sin_100_interactive.include = "#include <math.h>"
sin_100_interactive.ldflags = '-lm'
sin_100_interactive.stmtd = "double c = sin(a);"
sin_100_interactive.stmtf = "float c = sinf(a);"
sin_100_interactive.stmtl = "long double c = sinl(a);"
sin_100_interactive.vals = {'a':100}
sin_100_interactive.grads = {'a':3.1}
sin_100_interactive.test_vals = {'c':np.sin(100)}
sin_100_interactive.test_grads = {'c':np.cos(100)*3.1}
basiclist.append(sin_100_interactive)


### Take "cross product" with other configuation options ###
testlist = []
for test_arch in ["x86", "amd64"]:
  for test_compiler in ["gcc", "gxx"]:
    for test_type in ["double", "float", "longdouble"]:
      for basictest in basiclist:
        test = copy.deepcopy(basictest)
        test.name = test_arch+"_"+test_compiler+"_"+test_type+"_"+basictest.name

        if test_arch == "x86":
          test.arch = 32
        elif test_arch == "amd64":
          test.arch = 64
          # Disable amd64 testcases if hinted so by the environment variable.
          # Apparantly the current CI pipeline does not give us the 4 GB 
          # of RAM that we need to initialize shadow memory on a 64-bit system.
          if "DG_CONSTRAINED_ENVIRONMENT" in os.environ:
            test.disabled = True

        if test_compiler == "gcc":
          test.compiler = "gcc"
        elif test_compiler == "gxx":
          test.compiler = "gxx"

        if test_type == "double":
          test.stmt = test.stmtd
          test.type = TYPE_DOUBLE
        elif test_type == "float":
          test.stmt = test.stmtf
          test.type = TYPE_FLOAT
        elif test_type == "longdouble":
          test.stmt = test.stmtl
          test.type = TYPE_LONG_DOUBLE
        if test.stmt!=None:
          testlist.append(test)

### Run testcases ###
there_are_failed_tests = False
if selected_testcase:
  for test in testlist:
    if test.name==selected_testcase:
      there_are_failed_tests = test.run()
else: 
  outcomes = []
  for test in testlist:
    outcomes.append(test.run())

  print("Summary:")
  for i in range(len(testlist)):
    if outcomes[i]:
      print("  "+testlist[i].name+": PASSED")
    else:
      print("* "+testlist[i].name+": FAILED")
      there_are_failed_tests = True

if there_are_failed_tests:
  exit(1)
else:
  exit(0)

  
