import numpy as np
from TestCase import InteractiveTestCase, ClientRequestTestCase

testlist = []

### Basic arithmetic operations ###

addition = ClientRequestTestCase("addition")
addition.stmt = "double c = a+b;"
addition.vals = {'a':1.0,'b':2.0}
addition.grads = {'a':3.0,'b':4.0}
addition.test_vals = {'c':3.0}
addition.test_grads = {'c':7.0}
testlist.append(addition)

addition_const_l = ClientRequestTestCase("addition_const_l")
addition_const_l.stmt = "double c = 0.3 + a;"
addition_const_l.vals = {'a':1.0}
addition_const_l.grads = {'a':2.0}
addition_const_l.test_vals = {'c':1.3}
addition_const_l.test_grads = {'c':2.0}
testlist.append(addition_const_l)

addition_const_r = ClientRequestTestCase("addition_const_r")
addition_const_r.stmt = "double c = a + 0.3;"
addition_const_r.vals = {'a':1.0}
addition_const_r.grads = {'a':2.0}
addition_const_r.test_vals = {'c':1.3}
addition_const_r.test_grads = {'c':2.0}
testlist.append(addition_const_r)

subtraction = ClientRequestTestCase("subtraction")
subtraction.stmt = "double c = a-b;"
subtraction.vals = {'a':1.0,'b':2.0}
subtraction.grads = {'a':3.0,'b':4.0}
subtraction.test_vals = {'c':-1.0}
subtraction.test_grads = {'c':-1.0}
testlist.append(subtraction)

subtraction_const_l = ClientRequestTestCase("subtraction_const_l")
subtraction_const_l.stmt = "double c = 0.3 - a;"
subtraction_const_l.vals = {'a':1.0}
subtraction_const_l.grads = {'a':2.0}
subtraction_const_l.test_vals = {'c':-0.7}
subtraction_const_l.test_grads = {'c':-2.0}
testlist.append(subtraction_const_l)

subtraction_const_r = ClientRequestTestCase("subtraction_const_r")
subtraction_const_r.stmt = "double c = a - 0.5;"
subtraction_const_r.vals = {'a':1.0}
subtraction_const_r.grads = {'a':2.0}
subtraction_const_r.test_vals = {'c':0.5}
subtraction_const_r.test_grads = {'c':2.0}
testlist.append(subtraction_const_r)

multiplication = ClientRequestTestCase("multiplication")
multiplication.stmt = "double c = a*b;"
multiplication.vals = {'a':1.0,'b':2.0}
multiplication.grads = {'a':3.0,'b':4.0}
multiplication.test_vals = {'c':2.0}
multiplication.test_grads = {'c':10.0}
testlist.append(multiplication)

multiplication_const_l = ClientRequestTestCase("multiplication_const_l")
multiplication_const_l.stmt = "double c = 0.3 * a;"
multiplication_const_l.vals = {'a':2.0}
multiplication_const_l.grads = {'a':3.0}
multiplication_const_l.test_vals = {'c':0.6}
multiplication_const_l.test_grads = {'c':0.9}
testlist.append(multiplication_const_l)

multiplication_const_r = ClientRequestTestCase("multiplication_const_r")
multiplication_const_r.stmt = "double c = a * 0.5;"
multiplication_const_r.vals = {'a':2.0}
multiplication_const_r.grads = {'a':3.0}
multiplication_const_r.test_vals = {'c':1.0}
multiplication_const_r.test_grads = {'c':1.5}
testlist.append(multiplication_const_r)

division = ClientRequestTestCase("division")
division.stmt = "double c = a/b;"
division.vals = {'a':1.0,'b':2.0}
division.grads = {'a':5.0,'b':4.0}
division.test_vals = {'c':0.5}
division.test_grads = {'c':1.5}
testlist.append(division)

division_const_l = ClientRequestTestCase("division_const_l")
division_const_l.stmt = "double c = 0.3 / a;"
division_const_l.vals = {'a':2.0}
division_const_l.grads = {'a':3.0}
division_const_l.test_vals = {'c':0.15}
division_const_l.test_grads = {'c':-9/40}
testlist.append(division_const_l)

division_const_r = ClientRequestTestCase("division_const_r")
division_const_r.stmt = "double c = a / 0.5;"
division_const_r.vals = {'a':2.0}
division_const_r.grads = {'a':3.0}
division_const_r.test_vals = {'c':4.0}
division_const_r.test_grads = {'c':6.0}
testlist.append(division_const_r)

### Advances arithmetic and trigonometric operations ###

sqrt = ClientRequestTestCase("sqrt")
sqrt.include = "#include <math.h>"
sqrt.ldflags = '-lm'
sqrt.stmt = "double c = sqrt(a);"
sqrt.vals = {'a':4.0}
sqrt.grads = {'a':1.0}
sqrt.test_vals = {'c':2.0}
sqrt.test_grads = {'c':0.25}
testlist.append(sqrt)

abs_plus = ClientRequestTestCase("abs_plus")
abs_plus.include = "#include <math.h>"
abs_plus.ldflags = '-lm'
abs_plus.stmt = "double c = fabs(a);"
abs_plus.vals = {'a':1.0}
abs_plus.grads = {'a':2.0}
abs_plus.test_vals = {'c':1.0}
abs_plus.test_grads = {'c':2.0}
testlist.append(abs_plus)

abs_minus = ClientRequestTestCase("abs_minus")
abs_minus.include = "#include <math.h>"
abs_minus.ldflags = '-lm'
abs_minus.stmt = "double c = fabs(a);"
abs_minus.vals = {'a':-1.0}
abs_minus.grads = {'a':2.0}
abs_minus.test_vals = {'c':1.0}
abs_minus.test_grads = {'c':-2.0}
testlist.append(abs_minus)

for angle,angletext in [(0,"0"), (1e-3,"1m"), (1e-2,"10m"), (1e-1,"100m"), (1.,"1"), (-10.,"neg10"), (100.,"100")]:
  sin = ClientRequestTestCase("sin_"+angletext)
  sin.include = "#include <math.h>"
  sin.ldflags = '-lm'
  sin.stmt = "double c = sin(a);"
  sin.vals = {'a':angle}
  sin.grads = {'a':3.1}
  sin.test_vals = {'c':np.sin(angle)}
  sin.test_grads = {'c':np.cos(angle)*3.1}
  testlist.append(sin)

  cos = ClientRequestTestCase("cos_"+angletext)
  cos.include = "#include <math.h>"
  cos.ldflags = '-lm'
  cos.stmt = "double c = cos(a);"
  cos.vals = {'a':angle}
  cos.grads = {'a':2.7}
  cos.test_vals = {'c':np.cos(angle)}
  cos.test_grads = {'c':-np.sin(angle)*2.7}
  testlist.append(cos)

  tan = ClientRequestTestCase("tan_"+angletext)
  tan.include = "#include <math.h>"
  tan.ldflags = '-lm'
  tan.stmt = "double c = tan(a);"
  tan.vals = {'a':angle}
  tan.grads = {'a':1.0}
  tan.test_vals = {'c':np.tan(angle)}
  tan.test_grads = {'c':1./np.cos(angle)**2}
  testlist.append(tan)

exp = ClientRequestTestCase("exp")
exp.include = "#include <math.h>"
exp.ldflags = '-lm'
exp.stmt = "double c = exp(a);"
exp.vals = {'a':4}
exp.grads = {'a':5.0}
exp.test_vals = {'c':np.exp(4)}
exp.test_grads = {'c':np.exp(4)*5.0}
testlist.append(exp)



### Memory operations from string.h ###

memcpy = ClientRequestTestCase("memcpy")
memcpy.include = "#include <string.h>"
memcpy.stmt = "double aa[3],ac[3],c; aa[1] = a; memcpy(ac,aa,3*sizeof(double)); c=ac[1];"
memcpy.vals = {'a':-12.34}
memcpy.grads = {'a':-56.78}
memcpy.test_vals = {'c':-12.34}
memcpy.test_grads = {'c':-56.78}
testlist.append(memcpy)

memmove = ClientRequestTestCase("memmove")
memmove.include = "#include <string.h>"
memmove.stmt = "double aa[3],c; aa[0] = 3.14*a; aa[1] = a; memmove(aa+1,aa,2*sizeof(double)); c=aa[1];"
memmove.vals = {'a':-12.34}
memmove.grads = {'a':-56.78}
memmove.test_vals = {'c':-12.34*3.14}
memmove.test_grads = {'c':-56.78*3.14}
testlist.append(memmove)

memset = ClientRequestTestCase("memset")
memset.include = "#include <string.h>"
memset.stmt = "memset(&a,0,sizeof(double));"
memset.vals = {'a':-12.34}
memset.grads = {'a':-56.78}
memset.test_vals = {'a':0.0}
memset.test_grads = {'a':0.0}
testlist.append(memset)


### Control structures ###

ifbranch = ClientRequestTestCase("ifbranch")
ifbranch.stmt = "double c; if(a<1) c = 2+a; else c = 2*a; "
ifbranch.vals = {'a':0.0}
ifbranch.grads = {'a':1.0}
ifbranch.test_vals = {'c':2.0}
ifbranch.test_grads = {'c':1.0}
testlist.append(ifbranch)

elsebranch = ClientRequestTestCase("elsebranch")
elsebranch.stmt = "double c; if(a<-1) c = 2+a; else c = 2*a; "
elsebranch.vals = {'a':0.0}
elsebranch.grads = {'a':1.0}
elsebranch.test_vals = {'c':0.0}
elsebranch.test_grads = {'c':2.0}
testlist.append(elsebranch)

ternary_true = ClientRequestTestCase("ternary_true")
ternary_true.stmt = "double c = (a>-1) ? (3*a) : (a*a);"
ternary_true.vals = {'a':10.0}
ternary_true.grads = {'a':1.0}
ternary_true.test_vals = {'c':30.0}
ternary_true.test_grads = {'c':3.0}
testlist.append(ternary_true)

ternary_false = ClientRequestTestCase("ternary_false")
ternary_false.stmt = "double c = (a>-1) ? (3*a) : (a*a);"
ternary_false.vals = {'a':-10.0}
ternary_false.grads = {'a':1.0}
ternary_false.test_vals = {'c':100.0}
ternary_false.test_grads = {'c':-20.0}
testlist.append(ternary_false)


addition_forloop = ClientRequestTestCase("addition_forloop")
addition_forloop.stmt = "double c = 0; for(int i=0; i<10; i++) c+=a;"
addition_forloop.vals = {'a':2.0}
addition_forloop.grads = {'a':1.0}
addition_forloop.test_vals = {'c':20.0}
addition_forloop.test_grads = {'c':10.0}
testlist.append(addition_forloop)

multiplication_forloop = ClientRequestTestCase("multiplication_forloop")
multiplication_forloop.stmt = "double c = 1; for(int i=0; i<10; i++) c*=a;"
multiplication_forloop.vals = {'a':2.0}
multiplication_forloop.grads = {'a':1.0}
multiplication_forloop.test_vals = {'c':1024.0}
multiplication_forloop.test_grads = {'c':5120.0}
testlist.append(multiplication_forloop)

addition_whileloop = ClientRequestTestCase("addition_whileloop")
addition_whileloop.stmt = "double c = 0; while(c<19) c+=a;"
addition_whileloop.vals = {'a':2.0}
addition_whileloop.grads = {'a':1.0}
addition_whileloop.test_vals = {'c':20.0}
addition_whileloop.test_grads = {'c':10.0}
testlist.append(addition_whileloop)

multiplication_whileloop = ClientRequestTestCase("multiplication_whileloop")
multiplication_whileloop.stmt = "double c = 1; while(c<1023) c*=a;"
multiplication_whileloop.vals = {'a':2.0}
multiplication_whileloop.grads = {'a':1.0}
multiplication_whileloop.test_vals = {'c':1024.0}
multiplication_whileloop.test_grads = {'c':5120.0}
testlist.append(multiplication_whileloop)

addition_dowhileloop = ClientRequestTestCase("addition_dowhileloop")
addition_dowhileloop.stmt = "double c = 0; do c+=a; while(c<19);"
addition_dowhileloop.vals = {'a':2.0}
addition_dowhileloop.grads = {'a':1.0}
addition_dowhileloop.test_vals = {'c':20.0}
addition_dowhileloop.test_grads = {'c':10.0}
testlist.append(addition_dowhileloop)

multiplication_dowhileloop = ClientRequestTestCase("multiplication_dowhileloop")
multiplication_dowhileloop.stmt = "double c = 1; do c*=a; while(c<1023);"
multiplication_dowhileloop.vals = {'a':2.0}
multiplication_dowhileloop.grads = {'a':1.0}
multiplication_dowhileloop.test_vals = {'c':1024.0}
multiplication_dowhileloop.test_grads = {'c':5120.0}
testlist.append(multiplication_dowhileloop)

addition_recursion = ClientRequestTestCase("addition_recursion")
addition_recursion.include = "double f(int n, double x){ if(n==0) return 0.; else return x+f(n-1,x); }"
addition_recursion.stmt = "double c = f(10,a);"
addition_recursion.vals = {'a':2.0}
addition_recursion.grads = {'a':1.0}
addition_recursion.test_vals = {'c':20.0}
addition_recursion.test_grads = {'c':10.0}
testlist.append(addition_recursion)

multiplication_recursion = ClientRequestTestCase("multiplication_recursion")
multiplication_recursion.include = "double f(int n, double x){ if(n==0) return 1.; else return x*f(n-1,x); }"
multiplication_recursion.stmt = "double c = f(10,a);"
multiplication_recursion.vals = {'a':2.0}
multiplication_recursion.grads = {'a':1.0}
multiplication_recursion.test_vals = {'c':1024.0}
multiplication_recursion.test_grads = {'c':5120.0}
testlist.append(multiplication_recursion)

### OpenMP ###

# The following test succeeds, but variations of
# it fail. E.g. you cannot put a reduction clause or
# an  atomic directive instead of the critical 
# directive.
omp = ClientRequestTestCase("omp")
omp.cflags = "-fopenmp -lm"
omp.include = "#include <omp.h>\n#include <math.h>"
omp.stmt = """
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
omp.vals = {'a':1.0}
omp.grads = {'a':3.0}
omp_test_sum_val=0
omp_test_sum_grad=0
for i in range(100):
  omp_test_sum_val += np.sin(i*1.0)
  omp_test_sum_grad += np.cos(i*1.0)*i*3.0
omp.test_vals = {'sum':omp_test_sum_val}
omp.test_grads = {'sum':omp_test_sum_grad}
testlist.append(omp)


### Interactive tests ###
addition_interactive = InteractiveTestCase("addition_interactive")
addition_interactive.stmt = "double c = a+b;"
addition_interactive.vals = {'a':1.0,'b':2.0}
addition_interactive.grads = {'a':3.0,'b':4.0}
addition_interactive.test_vals = {'c':3.0}
addition_interactive.test_grads = {'c':7.0}
testlist.append(addition_interactive)

multiplication_interactive = InteractiveTestCase("multiplication_interactive")
multiplication_interactive.stmt = "double c = a*b;"
multiplication_interactive.vals = {'a':1.0,'b':2.0}
multiplication_interactive.grads = {'a':3.0,'b':4.0}
multiplication_interactive.test_vals = {'c':2.0}
multiplication_interactive.test_grads = {'c':10.0}
testlist.append(multiplication_interactive)

sin_100_interactive = ClientRequestTestCase("sin_100_interactive")
sin_100_interactive.include = "#include <math.h>"
sin_100_interactive.ldflags = '-lm'
sin_100_interactive.stmt = "double c = sin(a);"
sin_100_interactive.vals = {'a':100}
sin_100_interactive.grads = {'a':3.1}
sin_100_interactive.test_vals = {'c':np.sin(100)}
sin_100_interactive.test_grads = {'c':np.cos(100)*3.1}
testlist.append(sin_100_interactive)

outcomes = []
for test in testlist:
  outcomes.append(test.run())

print("Summary:")
there_are_failed_tests = False
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

  
