# -------------------------------------------------------------------- #
# --- Define and run unit tests for DerivGrind.      run_tests .py --- #
# -------------------------------------------------------------------- #

#
#  This file is part of DerivGrind, a tool performing forward-mode
#  algorithmic differentiation of compiled programs, implemented
#  in the Valgrind framework.
#
#  Copyright (C) 2022 Chair for Scientific Computing (SciComp), TU Kaiserslautern
#  Homepage: https://www.scicomp.uni-kl.de
#  Contact: Prof. Nicolas R. Gauger (derivgrind@scicomp.uni-kl.de)
#
#  Lead developer: Max Aehle (SciComp, TU Kaiserslautern)
#
#  This program is free software; you can redistribute it and/or
#  modify it under the terms of the GNU General Public License as
#  published by the Free Software Foundation; either version 2 of the
#  License, or (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful, but
#  WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
#  General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, see <http://www.gnu.org/licenses/>.
#
#  The GNU General Public License is contained in the file COPYING.
#

import numpy as np
import copy
from TestCase import InteractiveTestCase, ClientRequestTestCase, TYPE_DOUBLE, TYPE_FLOAT, TYPE_LONG_DOUBLE, TYPE_REAL4, TYPE_REAL8, TYPE_PYTHONFLOAT, TYPE_NUMPYFLOAT64, TYPE_NUMPYFLOAT32
import sys
import os
import fnmatch

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
addition.stmtr4 = "real, target :: c; c= a+b"
addition.stmtr8 = "double precision, target :: c; c= a+b"
addition.stmtp = "c = a+b"
addition.vals = {'a':1.0,'b':2.0}
addition.dots = {'a':3.0,'b':4.0}
addition.bars = {'c':1.0}
addition.test_vals = {'c':3.0}
addition.test_dots = {'c':7.0}
addition.test_bars = {'a':1.0,'b':1.0}
basiclist.append(addition)

addition_const_l = ClientRequestTestCase("addition_const_l")
addition_const_l.stmtd = "double c = 0.3 + a;"
addition_const_l.stmtf = "float c = 0.3f + a;"
addition_const_l.stmtl = "long double c = 0.3l + a;"
addition_const_l.stmtr4 = "real, target :: c; c= 0.3e0+a"
addition_const_l.stmtr8 = "double precision, target :: c; c= 0.3d0+a"
addition_const_l.stmtp = "c = 0.3 + a"
addition_const_l.vals = {'a':1.0}
addition_const_l.dots = {'a':2.0}
addition_const_l.bars = {'c':10.0}
addition_const_l.test_vals = {'c':1.3}
addition_const_l.test_dots = {'c':2.0}
addition_const_l.test_bars = {'a':10.0}
basiclist.append(addition_const_l)

addition_const_r = ClientRequestTestCase("addition_const_r")
addition_const_r.stmtd = "double c = a + 0.3;"
addition_const_r.stmtf = "float c = a + 0.3f;"
addition_const_r.stmtl = "long double c = a + 0.3l;"
addition_const_r.stmtr4 = "real, target :: c; c= a+0.3e0"
addition_const_r.stmtr8 = "double precision, target :: c; c= a+0.3d0"
addition_const_r.stmtp = "c = a + 0.3"
addition_const_r.vals = {'a':1.0}
addition_const_r.dots = {'a':2.0}
addition_const_r.bars = {'c':-20.0}
addition_const_r.test_vals = {'c':1.3}
addition_const_r.test_dots = {'c':2.0}
addition_const_r.test_bars = {'a':-20.0}
basiclist.append(addition_const_r)

subtraction = ClientRequestTestCase("subtraction")
subtraction.stmtd = "double c = a-b;"
subtraction.stmtf = "float c = a-b;"
subtraction.stmtl = "long double c = a-b;"
subtraction.stmtr4 = "real, target :: c; c= a-b"
subtraction.stmtr8 = "double precision, target :: c; c= a-b"
subtraction.stmtp = "c = a - b"
subtraction.vals = {'a':1.0,'b':2.0}
subtraction.dots = {'a':3.0,'b':4.0}
subtraction.bars = {'c':1.0}
subtraction.test_vals = {'c':-1.0}
subtraction.test_dots = {'c':-1.0}
subtraction.test_bars = {'a':1.0,'b':-1.0}
basiclist.append(subtraction)

subtraction_const_l = ClientRequestTestCase("subtraction_const_l")
subtraction_const_l.stmtd = "double c = 0.3 - a;"
subtraction_const_l.stmtf = "float c = 0.3f - a;"
subtraction_const_l.stmtl = "long double c = 0.3l - a;"
subtraction_const_l.stmtr4 = "real, target :: c; c= 0.3e0-a"
subtraction_const_l.stmtr8 = "double precision, target :: c; c= 0.3d0-a"
subtraction_const_l.stmtp = "c = 0.3 - a"
subtraction_const_l.vals = {'a':1.0}
subtraction_const_l.dots = {'a':2.0}
subtraction_const_l.bars = {'c':10.0}
subtraction_const_l.test_vals = {'c':-0.7}
subtraction_const_l.test_dots = {'c':-2.0}
subtraction_const_l.test_bars = {'a':-10.0}
basiclist.append(subtraction_const_l)

subtraction_const_r = ClientRequestTestCase("subtraction_const_r")
subtraction_const_r.stmtd = "double c = a - 0.5;"
subtraction_const_r.stmtf = "float c = a - 0.5f;"
subtraction_const_r.stmtl = "long double c = a - 0.5l;"
subtraction_const_r.stmtr4 = "real, target :: c; c= a-0.5e0"
subtraction_const_r.stmtr8 = "double precision, target :: c; c= a-0.5d0"
subtraction_const_r.stmtp = "c = a - 0.5"
subtraction_const_r.vals = {'a':1.0}
subtraction_const_r.dots = {'a':2.0}
subtraction_const_r.bars = {'c':20.0}
subtraction_const_r.test_vals = {'c':0.5}
subtraction_const_r.test_dots = {'c':2.0}
subtraction_const_r.test_bars = {'a':20.0}
basiclist.append(subtraction_const_r)

multiplication = ClientRequestTestCase("multiplication")
multiplication.stmtd = "double c = a*b;"
multiplication.stmtf = "float c = a*b;"
multiplication.stmtl = "long double c = a*b;"
multiplication.stmtr4 = "real, target :: c; c= a*b"
multiplication.stmtr8 = "double precision, target :: c; c= a*b"
multiplication.stmtp = "c = a * b"
multiplication.vals = {'a':1.0,'b':2.0}
multiplication.dots = {'a':3.0,'b':4.0}
multiplication.bars = {'c':1.0}
multiplication.test_vals = {'c':2.0}
multiplication.test_dots = {'c':10.0}
multiplication.test_bars = {'a':2.0,'b':1.0}
basiclist.append(multiplication)

multiplication_const_l = ClientRequestTestCase("multiplication_const_l")
multiplication_const_l.stmtd = "double c = 0.3 * a;"
multiplication_const_l.stmtf = "float c = 0.3f * a;"
multiplication_const_l.stmtl = "long double c = 0.3l * a;"
multiplication_const_l.stmtr4 = "real, target :: c; c = 0.3e0 * a"
multiplication_const_l.stmtr8 = "double precision, target :: c; c = 0.3d0 * a"
multiplication_const_l.stmtp = "c = 0.3 * a"
multiplication_const_l.vals = {'a':2.0}
multiplication_const_l.dots = {'a':3.0}
multiplication_const_l.bars = {'c':10.0}
multiplication_const_l.test_vals = {'c':0.6}
multiplication_const_l.test_dots = {'c':0.9}
multiplication_const_l.test_bars = {'a':3.0}
basiclist.append(multiplication_const_l)

multiplication_const_r = ClientRequestTestCase("multiplication_const_r")
multiplication_const_r.stmtd = "double c = a * 0.5;"
multiplication_const_r.stmtf = "float c = a * 0.5;"
multiplication_const_r.stmtl = "long double c = a * 0.5;"
multiplication_const_r.stmtr4 = "real, target :: c; c = a * 0.5e0"
multiplication_const_r.stmtr8 = "double precision, target :: c; c = a * 0.5d0"
multiplication_const_r.stmtp = "c = a * 0.5"
multiplication_const_r.vals = {'a':2.0}
multiplication_const_r.dots = {'a':3.0}
multiplication_const_r.bars = {'c':20.0}
multiplication_const_r.test_vals = {'c':1.0}
multiplication_const_r.test_dots = {'c':1.5}
multiplication_const_r.test_bars = {'a':10.0}
basiclist.append(multiplication_const_r)

division = ClientRequestTestCase("division")
division.stmtd = "double c = a/b;"
division.stmtf = "float c = a/b;"
division.stmtl = "long double c = a/b;"
division.stmtr4 = "real, target :: c; c = a/b"
division.stmtr8 = "double precision, target :: c; c = a/b"
division.stmtp = "c = a/b"
division.vals = {'a':1.0,'b':2.0}
division.dots = {'a':5.0,'b':4.0}
division.bars = {'c':1.0}
division.test_vals = {'c':0.5}
division.test_dots = {'c':1.5}
division.test_bars = {'a':0.5,'b':-0.25}
basiclist.append(division)

division_const_l = ClientRequestTestCase("division_const_l")
division_const_l.stmtd = "double c = 0.3 / a;"
division_const_l.stmtf = "float c = 0.3f / a;"
division_const_l.stmtl = "long double c = 0.3l / a;"
division_const_l.stmtr4 = "real, target :: c; c = 0.3e0 / a"
division_const_l.stmtr8 = "double precision, target :: c; c = 0.3d0 / a"
division_const_l.stmtp = "c = 0.3/a"
division_const_l.vals = {'a':2.0}
division_const_l.dots = {'a':3.0}
division_const_l.bars = {'c':10.0}
division_const_l.test_vals = {'c':0.15}
division_const_l.test_dots = {'c':-9/40}
division_const_l.test_bars = {'a':-0.75}
basiclist.append(division_const_l)

division_const_r = ClientRequestTestCase("division_const_r")
division_const_r.stmtd = "double c = a / 0.5;"
division_const_r.stmtf = "float c = a / 0.5f;"
division_const_r.stmtl = "long double c = a / 0.5l;"
division_const_r.stmtr4 = "real, target :: c; c = a / 0.5e0"
division_const_r.stmtr8 = "double precision, target :: c; c = a / 0.5d0"
division_const_r.stmtp = "c = a/0.5"
division_const_r.vals = {'a':2.0}
division_const_r.dots = {'a':3.0}
division_const_r.bars = {'c':20.0}
division_const_r.test_vals = {'c':4.0}
division_const_r.test_dots = {'c':6.0}
division_const_r.test_bars = {'a':40.0}
basiclist.append(division_const_r)

negative = ClientRequestTestCase("negative")
negative.stmtd = "double c = -a;"
negative.stmtf = "float c = -a;"
negative.stmtl = "long double c = -a;"
negative.stmtr4 = "real, target :: c; c= -a"
negative.stmtr8 = "double precision, target :: c; c= -a"
negative.stmtp = "c= -a"
negative.vals = {'a':1.0}
negative.dots = {'a':2.0}
negative.bars = {'c':1.0}
negative.test_vals = {'c':-1.0}
negative.test_dots = {'c':-2.0}
negative.test_bars = {'a':-1.0}
basiclist.append(negative)

negative_0 = ClientRequestTestCase("negative_0")
negative_0.include = "__attribute__((noinline)) double minus(double x){ return -x; }"
negative_0.stmtd = "double c = minus(minus(a));" # the outer minus() computes -(-0.0) i.e. (0x80.. xor 0x80..)
negative_0.vals = {'a':0.0}
negative_0.dots = {'a':1.0}
negative_0.bars = {'c':1.0}
negative_0.test_vals = {'c':0.0}
negative_0.test_dots = {'c':1.0}
negative_0.test_bars = {'a':1.0}
basiclist.append(negative_0)

### Advances arithmetic and trigonometric operations ###

abs_plus = ClientRequestTestCase("abs_plus")
abs_plus.include = "#include <math.h>"
abs_plus.ldflags = '-lm'
#abs_plus.cflags = '-fno-builtin' # might be advisable because the builtin fabs is difficult to differentiate
abs_plus.stmtd = "double c = fabs(a);"
abs_plus.stmtf = "float c = fabsf(a);"
abs_plus.stmtl = "long double c = fabsl(a);"
abs_plus.stmtr4 = "real, target :: c; c = abs(a)"
abs_plus.stmtr8 = "double precision, target :: c; c = abs(a)"
abs_plus.stmtp = "c = abs(a)"
abs_plus.vals = {'a':1.0}
abs_plus.dots = {'a':2.0}
abs_plus.bars = {'c':1.0}
abs_plus.test_vals = {'c':1.0}
abs_plus.test_dots = {'c':2.0}
abs_plus.test_bars = {'a':1.0}
basiclist.append(abs_plus)

abs_minus = ClientRequestTestCase("abs_minus")
abs_minus.include = "#include <math.h>"
abs_minus.ldflags = '-lm'
#abs_minus.cflags = '-fno-builtin' # might be advisable because the builtin fabs is difficult to differentiate
abs_minus.stmtd = "double c = fabs(a);"
abs_minus.stmtf = "float c = fabsf(a);"
abs_minus.stmtl = "long double c = fabsl(a);"
abs_minus.stmtr4 = "real, target :: c; c = abs(a)"
abs_minus.stmtr8 = "double precision, target :: c; c = abs(a)"
abs_minus.stmtp = "c = abs(a)"
abs_minus.vals = {'a':-1.0}
abs_minus.dots = {'a':2.0}
abs_minus.bars = {'c':2.0}
abs_minus.test_vals = {'c':1.0}
abs_minus.test_dots = {'c':-2.0}
abs_minus.test_bars = {'a':-2.0}
basiclist.append(abs_minus)

sqrt = ClientRequestTestCase("sqrt")
sqrt.include = "#include <math.h>"
sqrt.ldflags = '-lm'
sqrt.stmtd = "double c = sqrt(a);"
sqrt.stmtf = "float c = sqrtf(a);"
sqrt.stmtl = "long double c = sqrtl(a);"
sqrt.stmtr4 = "real, target :: c; c = sqrt(a)"
sqrt.stmtr8 = "double precision, target :: c; c = sqrt(a)"
sqrt.stmtp = "c = np.sqrt(a)"
sqrt.vals = {'a':4.0}
sqrt.dots = {'a':1.0}
sqrt.bars = {'c':1.0}
sqrt.test_vals = {'c':2.0}
sqrt.test_dots = {'c':0.25}
sqrt.test_bars = {'a':0.25}
basiclist.append(sqrt)

# if pow(a,b) is implemented as a*a for b==2., 
# the gradient of b would be discarded
pow_2 = ClientRequestTestCase("pow_2") 
pow_2.include = "#include <math.h>"
pow_2.ldflags = '-lm'
pow_2.stmtd = "double c = pow(a,b);"
pow_2.stmtf = "float c = powf(a,b);"
pow_2.stmtl = "long double c = powl(a,b);"
pow_2.stmtr4 = "real, target :: c; c = a**b"
pow_2.stmtr8 = "double precision, target :: c; c = a**b"
pow_2.stmtp = "c = a**b"
pow_2.vals = {'a':4.0,'b':2.0}
pow_2.dots = {'a':1.6,'b':1.9}
pow_2.bars = {'c':1.0}
pow_2.test_vals = {'c':4.0**2}
pow_2.test_dots = {'c':1.6*2*4.0 + 1.9*4.0**2*np.log(4)}
pow_2.test_bars = {'a':2*4.0**1,'b':(np.log(4.0)*4.0**2)}
basiclist.append(pow_2)

pow_both = ClientRequestTestCase("pow_both")
pow_both.include = "#include <math.h>"
pow_both.ldflags = '-lm'
pow_both.stmtd = "double c = pow(a,b);"
pow_both.stmtf = "float c = powf(a,b);"
pow_both.stmtl = "long double c = powl(a,b);"
pow_both.stmtr4 = "real, target :: c; c = a**b"
pow_both.stmtr8 = "double precision, target :: c; c = a**b"
pow_both.stmtp = "c = a**b"
pow_both.vals = {'a':4.0,'b':3.0}
pow_both.dots = {'a':1.6,'b':1.9}
pow_both.bars = {'c':1.0}
pow_both.test_vals = {'c':4.0**3.0}
pow_both.test_dots = {'c':1.6*3*4.0**2 + 1.9*4.0**3.0*np.log(4)}
pow_both.test_bars = {'a':3.0*4.0**2.0, 'b':(np.log(4.0)*4.0**3.0)}
basiclist.append(pow_both)

for angle,angletext in [(0,"0"), (1e-3,"1m"), (1e-2,"10m"), (1e-1,"100m"), (1.,"1"), (-10.,"neg10"), (100.,"100")]:
  sin = ClientRequestTestCase("sin_"+angletext)
  sin.include = "#include <math.h>"
  sin.ldflags = '-lm'
  sin.stmtd = "double c = sin(a);"
  sin.stmtf = "float c = sinf(a);"
  sin.stmtl = "long double c = sinl(a);"
  sin.stmtr4 = "real, target :: c; c = sin(a)"
  sin.stmtr8 = "double precision, target :: c; c = sin(a)"
  sin.stmtp = "c = np.sin(a)"
  sin.vals = {'a':angle}
  sin.dots = {'a':3.1}
  sin.bars = {'c':3.1}
  sin.test_vals = {'c':np.sin(angle)}
  sin.test_dots = {'c':np.cos(angle)*3.1}
  sin.test_bars = {'a':np.cos(angle)*3.1}
  sin.disable = lambda mode, arch, compiler, typename : arch == "amd64" and typename == "np32" # TODO
  basiclist.append(sin)

  cos = ClientRequestTestCase("cos_"+angletext)
  cos.include = "#include <math.h>"
  cos.ldflags = '-lm'
  cos.stmtd = "double c = cos(a);"
  cos.stmtf = "float c = cosf(a);"
  cos.stmtl = "long double c = cosl(a);"
  cos.stmtr4 = "real, target :: c; c = cos(a)"
  cos.stmtr8 = "double precision, target :: c; c = cos(a)"
  cos.stmtp = "c = np.cos(a)"
  cos.vals = {'a':angle}
  cos.dots = {'a':2.7}
  cos.bars = {'c':2.7}
  cos.test_vals = {'c':np.cos(angle)}
  cos.test_dots = {'c':-np.sin(angle)*2.7}
  cos.test_bars = {'a':-np.sin(angle)*2.7}
  cos.disable = lambda mode, arch, compiler, typename : arch == "amd64" and typename == "np32" # TODO
  basiclist.append(cos)

  tan = ClientRequestTestCase("tan_"+angletext)
  tan.include = "#include <math.h>"
  tan.ldflags = '-lm'
  tan.stmtd = "double c = tan(a);"
  tan.stmtf = "float c = tanf(a);"
  tan.stmtl = "long double c = tanl(a);"
  tan.stmtr4 = "real, target :: c; c = tan(a)"
  tan.stmtr8 = "double precision, target :: c; c = tan(a)"
  tan.stmtp = "c = np.tan(a)"
  tan.vals = {'a':angle}
  tan.dots = {'a':1.0}
  tan.bars = {'c':1.0}
  tan.test_vals = {'c':np.tan(angle)}
  tan.test_dots = {'c':1./np.cos(angle)**2}
  tan.test_bars = {'a':1./np.cos(angle)**2}
  basiclist.append(tan)

exp = ClientRequestTestCase("exp")
exp.include = "#include <math.h>"
exp.ldflags = '-lm'
exp.stmtd = "double c = exp(a);"
exp.stmtf = "float c = expf(a);"
exp.stmtl = "long double c = expl(a);"
exp.stmtr4 = "real, target :: c; c = exp(a)"
exp.stmtr8 = "double precision, target :: c; c = exp(a)"
exp.stmtp = "c = np.exp(a)"
exp.vals = {'a':4}
exp.dots = {'a':5.0}
exp.bars = {'c':5.0}
exp.test_vals = {'c':np.exp(4)}
exp.test_dots = {'c':np.exp(4)*5.0}
exp.test_bars = {'a':np.exp(4)*5.0}
exp.disable = lambda mode, arch, compiler, typename : arch == "amd64" and typename == "np32" # TODO
basiclist.append(exp)

log = ClientRequestTestCase("log")
log.include = "#include <math.h>"
log.ldflags = '-lm'
log.stmtd = "double c = log(a);"
log.stmtf = "float c = logf(a);"
log.stmtl = "long double c = logl(a);"
log.stmtr4 = "real, target :: c; c = log(a)"
log.stmtr8 = "double precision, target :: c; c = log(a)"
log.stmtp = "c = np.log(a)"
log.vals = {'a':20}
log.dots = {'a':1.0}
log.bars = {'c':1.0}
log.test_vals = {'c':np.log(20)}
log.test_dots = {'c':0.05}
log.test_bars = {'a':0.05}
log.disable = lambda mode, arch, compiler, typename : arch == "amd64" and typename == "np32" # TODO
basiclist.append(log)

log10 = ClientRequestTestCase("log10")
log10.include = "#include <math.h>"
log10.ldflags = '-lm'
log10.stmtd = "double c = log10(a);"
log10.stmtf = "float c = log10f(a);"
log10.stmtl = "long double c = log10l(a);"
log10.stmtr4 = "real, target :: c; c = log10(a)"
log10.stmtr8 = "double precision, target :: c; c = log10(a)"
log10.stmtp = "c = np.log10(a)"
log10.vals = {'a':0.01}
log10.dots = {'a':1.0}
log10.bars = {'c':1.0}
log10.test_vals = {'c':-2}
log10.test_dots = {'c':100/np.log(10)}
log10.test_bars = {'a':100/np.log(10)}
basiclist.append(log10)

sinh = ClientRequestTestCase("sinh")
sinh.include = "#include <math.h>"
sinh.ldflags = '-lm'
sinh.stmtd = "double c = sinh(a);"
sinh.stmtf = "float c = sinhf(a);"
sinh.stmtl = "long double c = sinhl(a);"
sinh.stmtr4 = "real, target :: c; c = sinh(a)"
sinh.stmtr8 = "double precision, target :: c; c = sinh(a)"
sinh.stmtp = "c = np.sinh(a)"
sinh.vals = {'a':2.0}
sinh.dots = {'a':1.0}
sinh.bars = {'c':1.0}
sinh.test_vals = {'c':np.sinh(2.0)}
sinh.test_dots = {'c':np.cosh(2.0)}
sinh.test_bars = {'a':np.cosh(2.0)}
basiclist.append(sinh)

cosh = ClientRequestTestCase("cosh")
cosh.include = "#include <math.h>"
cosh.ldflags = '-lm'
cosh.stmtd = "double c = cosh(a);"
cosh.stmtf = "float c = coshf(a);"
cosh.stmtl = "long double c = coshl(a);"
cosh.stmtr4 = "real, target :: c; c = cosh(a)"
cosh.stmtr8 = "double precision, target :: c; c = cosh(a)"
cosh.stmtp = "c = np.cosh(a)"
cosh.vals = {'a':-2.0}
cosh.dots = {'a':1.0}
cosh.bars = {'c':1.0}
cosh.test_vals = {'c':np.cosh(-2.0)}
cosh.test_dots = {'c':np.sinh(-2.0)}
cosh.test_bars = {'a':np.sinh(-2.0)}
basiclist.append(cosh)

tanh = ClientRequestTestCase("tanh")
tanh.include = "#include <math.h>"
tanh.ldflags = '-lm'
tanh.stmtd = "double c = tanh(a);"
tanh.stmtf = "float c = tanhf(a);"
tanh.stmtl = "long double c = tanhl(a);"
tanh.stmtr4 = "real, target :: c; c = tanh(a)"
tanh.stmtr8 = "double precision, target :: c; c = tanh(a)"
tanh.stmtp = "c = np.tanh(a)"
tanh.vals = {'a':-0.5}
tanh.dots = {'a':1.0}
tanh.bars = {'c':1.0}
tanh.test_vals = {'c':np.tanh(-0.5)}
tanh.test_dots = {'c':1-np.tanh(-0.5)**2}
tanh.test_bars = {'a':1-np.tanh(-0.5)**2}
basiclist.append(tanh)

asin = ClientRequestTestCase("asin")
asin.include = "#include <math.h>"
asin.ldflags = '-lm'
asin.stmtd = "double c = asin(a);"
asin.stmtf = "float c = asinf(a);"
asin.stmtl = "long double c = asinl(a);"
asin.stmtr4 = "real, target :: c; c = asin(a)"
asin.stmtr8 = "double precision, target :: c; c = asin(a)"
asin.stmtp = "c = np.arcsin(a)"
asin.vals = {'a':0.9}
asin.dots = {'a':1.0}
asin.bars = {'c':1.0}
asin.test_vals = {'c':np.arcsin(0.9)}
asin.test_dots = {'c':1/np.sqrt(1-0.9**2)}
asin.test_bars = {'a':1/np.sqrt(1-0.9**2)}
basiclist.append(asin)

acos = ClientRequestTestCase("acos")
acos.include = "#include <math.h>"
acos.ldflags = '-lm'
acos.stmtd = "double c = acos(a);"
acos.stmtf = "float c = acosf(a);"
acos.stmtl = "long double c = acosl(a);"
acos.stmtr4 = "real, target :: c; c = acos(a)"
acos.stmtr8 = "double precision, target :: c; c = acos(a)"
acos.stmtp = "c = np.arccos(a)"
acos.vals = {'a':-0.4}
acos.dots = {'a':1.0}
acos.bars = {'c':1.0}
acos.test_vals = {'c':np.arccos(-0.4)}
acos.test_dots = {'c':-1/np.sqrt(1-(-0.4)**2)}
acos.test_bars = {'a':-1/np.sqrt(1-(-0.4)**2)}
basiclist.append(acos)

atan = ClientRequestTestCase("atan")
atan.include = "#include <math.h>"
atan.ldflags = '-lm'
atan.stmtd = "double c = atan(a);"
atan.stmtf = "float c = atanf(a);"
atan.stmtl = "long double c = atanl(a);"
atan.stmtr4 = "real, target :: c; c = atan(a)"
atan.stmtr8 = "double precision, target :: c; c = atan(a)"
atan.stmtp = "c = np.arctan(a)"
atan.vals = {'a':100}
atan.dots = {'a':1.0}
atan.bars = {'c':1.0}
atan.test_vals = {'c':np.arctan(100)}
atan.test_dots = {'c':1/(1+100**2)}
atan.test_bars = {'a':1/(1+100**2)}
basiclist.append(atan)

atan2 = ClientRequestTestCase("atan2")
atan2.include = "#include <math.h>"
atan2.ldflags = '-lm'
atan2.stmtd = "double c = atan2(a,b);"
atan2.stmtf = "float c = atan2f(a,b);"
atan2.stmtl = "long double c = atan2l(a,b);"
atan2.stmtr4 = "real, target :: c; c = atan2(a,b)"
atan2.stmtr8 = "double precision, target :: c; c = atan2(a,b)"
atan2.stmtp = "c = np.arctan2(a,b)"
atan2.vals = {'a':3,'b':4}
atan2.dots = {'a':1.3, 'b':1.5}
atan2.bars = {'c':1.0}
atan2.test_vals = {'c':np.arctan2(3,4)}
atan2.test_dots = {'c':1.3*(-4)/(3**2+4**2) + 1.5*3/(3**2+4**2)}
atan2.test_bars = {'a':(-4)/(3**2+4**2), 'b':3/(3**2+4**2)}
basiclist.append(atan2)

floor = ClientRequestTestCase("floor")
floor.include = "#include <math.h>"
floor.ldflags = '-lm'
floor.stmtd = "double c = floor(a);"
floor.stmtf = "float c = floorf(a);"
floor.stmtl = "long double c = floorl(a);"
floor.stmtr4 = "real, target :: c; c = floor(a)"
floor.stmtr8 = "double precision, target :: c; c = floor(a)"
floor.stmtp = "c = np.floor(a)"
floor.vals = {'a':2.0}
floor.dots = {'a':1.0}
floor.bars = {'c':1.0}
floor.test_vals = {'c':2.0}
floor.test_dots = {'c':0.0}
floor.test_bars = {'a':0.0}
basiclist.append(floor)

ceil = ClientRequestTestCase("ceil")
ceil.include = "#include <math.h>"
ceil.ldflags = '-lm'
ceil.stmtd = "double c = ceil(a);"
ceil.stmtf = "float c = ceilf(a);"
ceil.stmtl = "long double c = ceill(a);"
ceil.stmtr4 = "real, target :: c; c = ceiling(a)"
ceil.stmtr8 = "double precision, target :: c; c = ceiling(a)"
ceil.stmtp = "c = np.ceil(a)"
ceil.vals = {'a':2.1}
ceil.dots = {'a':1.0}
ceil.bars = {'c':1.0}
ceil.test_vals = {'c':3.0}
ceil.test_dots = {'c':0.0}
ceil.test_bars = {'a':0.0}
basiclist.append(ceil)

ldexp = ClientRequestTestCase("ldexp")
ldexp.include = "#include <math.h>"
ldexp.ldflags = '-lm'
ldexp.stmtd = "double c = ldexp(a,-3);"
ldexp.stmtf = "float c = ldexpf(a,-3);"
ldexp.stmtl = "long double c = ldexpl(a,-3);"
ldexp.stmtp = "c = np.ldexp(a,-3)"
ldexp.vals = {'a':2.4}
ldexp.dots = {'a':-1.0}
ldexp.bars = {'c':-1.0}
ldexp.test_vals = {'c':0.3}
ldexp.test_dots = {'c':-1.0/8}
ldexp.test_bars = {'a':-1.0/8}
basiclist.append(ldexp)

frexp = ClientRequestTestCase("frexp")
frexp.include = "#include <math.h>"
frexp.ldflags = '-lm'
frexp.stmtd = "int e; double c = frexp(a,&e); double ee = e;"
frexp.stmtf = "int e; float c = frexpf(a,&e); float ee = e;"
frexp.stmtl = "int e; long double c = frexpl(a,&e); long double ee=e;"
frexp.stmtp = "c, e = np.frexp(a); ee=1.0*e"
frexp.vals = {'a':-5.0}
frexp.dots = {'a':-1.0}
frexp.bars = {'c':-1.0,'ee':100.0}
frexp.test_vals = {'c':-5.0/8, 'ee':3.0}
frexp.test_dots = {'c':-1.0/8, 'ee':0.0}
frexp.test_bars = {'a':-1.0/8}
basiclist.append(frexp)

### Memory operations from string.h ###

memcpy = ClientRequestTestCase("memcpy")
memcpy.include = "#include <string.h>"
memcpy.stmtd = "double aa[3],ac[3],c,d; aa[1] = a; aa[2] = 100*a; memcpy(ac,aa,3*sizeof(double)); c=ac[1]; d=ac[2];"
memcpy.stmtf = "float aa[3],ac[3],c,d; aa[1] = a; aa[2] = 100*a; memcpy(ac,aa,3*sizeof(float)); c=ac[1]; d=ac[2];"
memcpy.stmtl = "long double aa[3],ac[3],c,d; aa[1] = a; aa[2] = 100*a; memcpy(ac,aa,3*sizeof(long double)); c=ac[1]; d=ac[2];"
memcpy.vals = {'a':-12.34}
memcpy.dots = {'a':-56.78}
memcpy.bars = {'c':1,'d':3}
memcpy.test_vals = {'c':-12.34,'d':-1234}
memcpy.test_dots = {'c':-56.78,'d':-5678}
memcpy.test_bars = {'a':301.0}
basiclist.append(memcpy)

memmove = ClientRequestTestCase("memmove")
memmove.include = "#include <string.h>"
memmove.stmtd = "double aa[3],c,d; aa[0] = 3.14*a; aa[1] = a; memmove(aa+1,aa,2*sizeof(double)); c=aa[1]; d=aa[2];"
memmove.stmtf = "float aa[3],c,d; aa[0] = 3.14*a; aa[1] = a; memmove(aa+1,aa,2*sizeof(float)); c=aa[1]; d=aa[2];"
memmove.stmtl = "long double aa[3],c,d; aa[0] = 3.14*a; aa[1] = a; memmove(aa+1,aa,2*sizeof(long double)); c=aa[1]; d=aa[2];"
memmove.vals = {'a':-12.34}
memmove.dots = {'a':-56.78}
memmove.bars = {'c':2,'d':15}
memmove.test_vals = {'c':-12.34*3.14,'d':-12.34}
memmove.test_dots = {'c':-56.78*3.14,'d':-56.78}
memmove.test_bars = {'a':3.14*2+15}
basiclist.append(memmove)

memset = ClientRequestTestCase("memset")
memset.include = "#include <string.h>"
memset.stmtd = "memset(&a,0,sizeof(double));"
memset.stmtf = "memset(&a,0,sizeof(float));"
memset.stmtl = "memset(&a,0,sizeof(long double));"
memset.vals = {'a':-12.34}
memset.dots = {'a':-56.78}
memset.bars = {'a':1.0}
memset.test_vals = {'a':0.0}
memset.test_dots = {'a':0.0}
memset.test_bars = {'a':0.0}
basiclist.append(memset)


### Control structures ###

ifbranch = ClientRequestTestCase("ifbranch")
ifbranch.stmtd = "double c; if(a<1) c = 2+a; else c = 2*a; "
ifbranch.stmtf = "float c; if(a<1) c = 2+a; else c = 2*a; "
ifbranch.stmtl = "long double c; if(a<1) c = 2+a; else c = 2*a; "
ifbranch.stmtr4 = "real, target :: c; if(a<1) then; c = 2+a; else; c = 2*a; end if"
ifbranch.stmtr8 = "double precision, target :: c; if(a<1) then; c = 2+a; else; c = 2*a; end if"
ifbranch.stmtp = "if a<1:\n  c = 2+a\nelse:\n  c = 2*a\n"
ifbranch.disable = lambda mode, arch, compiler, typename : typename in ["np64", "np32"]
ifbranch.vals = {'a':0.0}
ifbranch.dots = {'a':1.0}
ifbranch.bars = {'c':1.0}
ifbranch.test_vals = {'c':2.0}
ifbranch.test_dots = {'c':1.0}
ifbranch.test_bars = {'a':1.0}
basiclist.append(ifbranch)

elsebranch = ClientRequestTestCase("elsebranch")
elsebranch.stmtd = "double c; if(a<-1) c = 2+a; else c = 2*a; "
elsebranch.stmtf = "float c; if(a<-1) c = 2+a; else c = 2*a; "
elsebranch.stmtl = "long double c; if(a<-1) c = 2+a; else c = 2*a; "
elsebranch.stmtr4 = "real, target :: c; if(a<-1) then; c = 2+a; else; c = 2*a; end if"
elsebranch.stmtr8 = "double precision, target :: c; if(a<-1) then; c = 2+a; else; c = 2*a; end if"
elsebranch.stmtp = "if a<-1:\n  c = 2+a\nelse:\n  c = 2*a\n"
elsebranch.disable = lambda mode, arch, compiler, typename : typename in ["np64", "np32"]
elsebranch.vals = {'a':0.0}
elsebranch.dots = {'a':1.0}
elsebranch.bars = {'c':1.0}
elsebranch.test_vals = {'c':0.0}
elsebranch.test_dots = {'c':2.0}
elsebranch.test_bars = {'a':2.0}
basiclist.append(elsebranch)

ternary_true = ClientRequestTestCase("ternary_true")
ternary_true.stmtd = "double c = (a>-1) ? (3*a) : (a*a);"
ternary_true.stmtf = "float c = (a>-1) ? (3*a) : (a*a);"
ternary_true.stmtl = "long double c = (a>-1) ? (3*a) : (a*a);"
ternary_true.stmtr4 = "real, target :: c; c = merge(3*a, a*a, a>-1)"
ternary_true.stmtr8 = "double precision, target :: c; c = merge(3*a, a*a, a>-1)"
ternary_true.stmtp = "c = (3*a) if (a>-1) else (a*a)"
ternary_true.disable = lambda mode, arch, compiler, typename : typename in ["np64", "np32"]
ternary_true.vals = {'a':10.0}
ternary_true.dots = {'a':1.0}
ternary_true.bars = {'c':1.0}
ternary_true.test_vals = {'c':30.0}
ternary_true.test_dots = {'c':3.0}
ternary_true.test_bars = {'a':3.0}
basiclist.append(ternary_true)

ternary_false = ClientRequestTestCase("ternary_false")
ternary_false.stmtd = "double c = (a>-1) ? (3*a) : (a*a);"
ternary_false.stmtf = "float c = (a>-1) ? (3*a) : (a*a);"
ternary_false.stmtl = "long double c = (a>-1) ? (3*a) : (a*a);"
ternary_false.stmtr4 = "real, target :: c; c = merge(3*a, a*a, a>-1)"
ternary_false.stmtr8 = "double precision, target :: c; c = merge(3*a, a*a, a>-1)"
ternary_false.stmtp = "c = (3*a) if (a>-1) else (a*a)"
ternary_false.disable = lambda mode, arch, compiler, typename : typename in ["np64", "np32"]
ternary_false.vals = {'a':-10.0}
ternary_false.dots = {'a':1.0}
ternary_false.bars = {'c':1.0}
ternary_false.test_vals = {'c':100.0}
ternary_false.test_dots = {'c':-20.0}
ternary_false.test_bars = {'a':-20.0}
basiclist.append(ternary_false)


addition_forloop = ClientRequestTestCase("addition_forloop")
addition_forloop.stmtd = "double c = 0; for(int i=0; i<10; i++) c+=a;"
addition_forloop.stmtf = "float c = 0; for(int i=0; i<10; i++) c+=a;"
addition_forloop.stmtl = "long double c = 0; for(int i=0; i<10; i++) c+=a;"
addition_forloop.stmtr4 = "real, target :: c = 0; integer :: i; do i=0, 9; c=c+a; end do"
addition_forloop.stmtr8 = "double precision, target :: c = 0; integer :: i; do i=0, 9; c=c+a; end do"
addition_forloop.stmtp = "c=0\nfor i in range(10):\n  c+=a"
addition_forloop.vals = {'a':2.0}
addition_forloop.dots = {'a':1.0}
addition_forloop.bars = {'c':1.0}
addition_forloop.test_vals = {'c':20.0}
addition_forloop.test_dots = {'c':10.0}
addition_forloop.test_bars = {'a':10.0}
basiclist.append(addition_forloop)

multiplication_forloop = ClientRequestTestCase("multiplication_forloop")
multiplication_forloop.stmtd = "double c = 1; for(int i=0; i<10; i++) c*=a;"
multiplication_forloop.stmtf = "float c = 1; for(int i=0; i<10; i++) c*=a;"
multiplication_forloop.stmtl = "long double c = 1; for(int i=0; i<10; i++) c*=a;"
multiplication_forloop.stmtr4 = "real, target :: c = 1; integer :: i; do i=0, 9; c=c*a; end do"
multiplication_forloop.stmtr8 = "double precision, target :: c = 1; integer :: i; do i=0, 9; c=c*a; end do"
multiplication_forloop.stmtp = "c=1\nfor i in range(10):\n  c*=a"
multiplication_forloop.vals = {'a':2.0}
multiplication_forloop.dots = {'a':1.0}
multiplication_forloop.bars = {'c':1.0}
multiplication_forloop.test_vals = {'c':1024.0}
multiplication_forloop.test_dots = {'c':5120.0}
multiplication_forloop.test_bars = {'a':5120.0}
basiclist.append(multiplication_forloop)

addition_whileloop = ClientRequestTestCase("addition_whileloop")
addition_whileloop.stmtd = "double c = 0; while(c<19) c+=a;"
addition_whileloop.stmtf = "float c = 0; while(c<19) c+=a;"
addition_whileloop.stmtl = "long double c = 0; while(c<19) c+=a;"
addition_whileloop.stmtr4 = "real, target :: c = 0; do while(c<19); c=c+a; end do"
addition_whileloop.stmtr8 = "double precision, target :: c = 0; do while(c<19); c=c+a; end do"
addition_whileloop.stmtp = "c=0\nwhile c<19:\n  c=c+a"
addition_whileloop.disable = lambda mode, arch, compiler, typename : typename in ["np64", "np32"]
addition_whileloop.vals = {'a':2.0}
addition_whileloop.dots = {'a':1.0}
addition_whileloop.bars = {'c':1.0}
addition_whileloop.test_vals = {'c':20.0}
addition_whileloop.test_dots = {'c':10.0}
addition_whileloop.test_bars = {'a':10.0}
basiclist.append(addition_whileloop)

multiplication_whileloop = ClientRequestTestCase("multiplication_whileloop")
multiplication_whileloop.stmtd = "double c = 1; while(c<1023) c*=a;"
multiplication_whileloop.stmtf = "float c = 1; while(c<1023) c*=a;"
multiplication_whileloop.stmtl = "long double c = 1; while(c<1023) c*=a;"
multiplication_whileloop.stmtr4 = "real, target :: c = 1; do while(c<1023); c=c*a; end do"
multiplication_whileloop.stmtr8 = "double precision, target :: c = 1; do while(c<1023); c=c*a; end do"
multiplication_whileloop.stmtp = "c=1\nwhile c<1023:\n  c=c*a"
multiplication_whileloop.disable = lambda mode, arch, compiler, typename : typename in ["np64", "np32"]
multiplication_whileloop.vals = {'a':2.0}
multiplication_whileloop.dots = {'a':1.0}
multiplication_whileloop.bars = {'c':1.0}
multiplication_whileloop.test_vals = {'c':1024.0}
multiplication_whileloop.test_dots = {'c':5120.0}
multiplication_whileloop.test_bars = {'a':5120.0}
basiclist.append(multiplication_whileloop)

addition_dowhileloop = ClientRequestTestCase("addition_dowhileloop")
addition_dowhileloop.stmtd = "double c = 0; do c+=a; while(c<19);"
addition_dowhileloop.stmtf = "float c = 0; do c+=a; while(c<19);"
addition_dowhileloop.stmtl = "long double c = 0; do c+=a; while(c<19);"
addition_dowhileloop.vals = {'a':2.0}
addition_dowhileloop.dots = {'a':1.0}
addition_dowhileloop.bars = {'c':1.0}
addition_dowhileloop.test_vals = {'c':20.0}
addition_dowhileloop.test_dots = {'c':10.0}
addition_dowhileloop.test_bars = {'a':10.0}
basiclist.append(addition_dowhileloop)

multiplication_dowhileloop = ClientRequestTestCase("multiplication_dowhileloop")
multiplication_dowhileloop.stmtd = "double c = 1; do c*=a; while(c<1023);"
multiplication_dowhileloop.stmtf = "float c = 1; do c*=a; while(c<1023);"
multiplication_dowhileloop.stmtl = "long double c = 1; do c*=a; while(c<1023);"
multiplication_dowhileloop.vals = {'a':2.0}
multiplication_dowhileloop.dots = {'a':1.0}
multiplication_dowhileloop.bars = {'c':1.0}
multiplication_dowhileloop.test_vals = {'c':1024.0}
multiplication_dowhileloop.test_dots = {'c':5120.0}
multiplication_dowhileloop.test_bars = {'a':5120.0}
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
addition_recursion.stmtp = "def f(n,x):\n  if n==0:\n    return 0.\n  else:\n    return x+f(n-1,x)\nc=f(10,a)"
addition_recursion.vals = {'a':2.0}
addition_recursion.dots = {'a':1.0}
addition_recursion.bars = {'c':1.0}
addition_recursion.test_vals = {'c':20.0}
addition_recursion.test_dots = {'c':10.0}
addition_recursion.test_bars = {'a':10.0}
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
multiplication_recursion.stmtp = "def f(n,x):\n  if n==0:\n    return 1.\n  else:\n    return x*f(n-1,x)\nc=f(10,a)"
multiplication_recursion.vals = {'a':2.0}
multiplication_recursion.dots = {'a':1.0}
multiplication_recursion.bars = {'c':1.0}
multiplication_recursion.test_vals = {'c':1024.0}
multiplication_recursion.test_dots = {'c':5120.0}
multiplication_recursion.test_bars = {'a':5120.0}
basiclist.append(multiplication_recursion)

### Auto-Vectorization ###
for (name, op, c_val, c_dot, a_bar, b_bar) in [ 
  ("addition", "+", 1184.,288.,120.,16.),
  ("subtraction", "-", 1216., 192., 120., -16.),
  ("multiplication", "*", -1200., 3360., -120., 1200.),
  ("division", "/", -1200., -3840.0, -120., -1200.)
  ]:
  autovectorization = ClientRequestTestCase(name+"_autovectorization")
  autovectorization_stmtbody_c = f"""
  for(int i=0; i<16; i++) {{ a_arr[i] = i*a; b_arr[i] = b; }}
  for(int i=0; i<16; i++) {{ c_arr[i] = a_arr[i] {op} b_arr[i]; }}
  for(int i=0; i<16; i++) {{ c += c_arr[i]; }}
  """
  autovectorization_stmtbody_fortran = f"""
  integer :: i
  do i=1,16; a_arr(i) = (i-1)*a; b_arr(i) = b; end do
  do i=1,16; c_arr(i) = a_arr(i) {op} b_arr(i); end do
  do i=1,16; c = c + c_arr(i); end do
  """
  autovectorization.stmtd = "double a_arr[16], b_arr[16], c_arr[16], c = 0;"+autovectorization_stmtbody_c
  autovectorization.stmtf = "float a_arr[16], b_arr[16], c_arr[16], c = 0;"+autovectorization_stmtbody_c
  autovectorization.stmtl = "long double a_arr[16], b_arr[16], c_arr[16], c = 0;"+autovectorization_stmtbody_c
  autovectorization.stmtr4 = "real, dimension(16) :: a_arr, b_arr, c_arr; real, target :: c = 0; "+autovectorization_stmtbody_fortran
  autovectorization.stmtr8 = "double precision, dimension(16) :: a_arr, b_arr, c_arr; double precision, target :: c = 0; "+autovectorization_stmtbody_fortran
  autovectorization.vals = {'a':10.0,'b':-1.0}
  autovectorization.dots = {'a':2.0,'b':3.0}
  autovectorization.bars = {'c':1.0}
  autovectorization.test_vals = {'c':c_val}
  autovectorization.test_dots = {'c':c_dot}
  autovectorization.test_bars = {'a':a_bar, 'b':b_bar}
  basiclist.append(autovectorization)

for (name, cfun, ffun, c_val, grad) in [ 
  ("abs", "fabs", "abs", 1200.,120.),
  ("negative", "-", "-", 64.,8.),
  ]:
  autovectorization = ClientRequestTestCase(name+"_autovectorization")
  cfun_suffix = {"d":"","f":"f","l":"l"}
  autovectorization_stmtbody_c = {}
  for type_ in cfun_suffix:
    autovectorization_stmtbody_c[type_] = f"""
    for(int i=0; i<16; i++) {{ a_arr[i] = i*a * pow(-1,i) + 1; }}
    for(int i=0; i<16; i++) {{ c_arr[i] = {cfun}{cfun_suffix[type_] if name=="abs" else ""}(a_arr[i]); }}
    for(int i=0; i<16; i++) {{ c += c_arr[i]; }}
    """
  autovectorization_stmtbody_fortran = f"""
  integer :: i
  do i=1,16; a_arr(i) = (i-1)*a*(-1)**(i-1) + 1; end do
  do i=1,16; c_arr(i) = {ffun}(a_arr(i)) ; end do
  do i=1,16; c = c + c_arr(i); end do
  """
  autovectorization.stmtd = "double a_arr[16], c_arr[16], c = 0;"+autovectorization_stmtbody_c["d"]
  autovectorization.stmtf = "float a_arr[16], c_arr[16], c = 0;"+autovectorization_stmtbody_c["f"]
  autovectorization.stmtl = "long double a_arr[16], c_arr[16], c = 0;"+autovectorization_stmtbody_c["l"]
  autovectorization.stmtr4 = "real, dimension(16) :: a_arr, c_arr; real, target :: c = 0; "+autovectorization_stmtbody_fortran
  autovectorization.stmtr8 = "double precision, dimension(16) :: a_arr, c_arr; double precision, target :: c = 0; "+autovectorization_stmtbody_fortran
  autovectorization.ldflags = "-lm"
  autovectorization.include = "#include <math.h>"
  autovectorization.vals = {'a':10.0}
  autovectorization.dots = {'a':1.0}
  autovectorization.bars = {'c':1.0}
  autovectorization.test_vals = {'c':c_val}
  autovectorization.test_dots = {'c':grad}
  autovectorization.test_bars = {'a':grad}
  basiclist.append(autovectorization)

### OpenMP ###

# The following test succeeds, but variations of
# it fail. E.g. you cannot put a reduction clause or
# an  atomic directive instead of the critical 
# directive.
omp_critical = ClientRequestTestCase("omp_critical")
omp_critical.cflags = "-fopenmp -lm"
omp_critical.cflags_clang = "-fopenmp=libgomp -lm"
omp_critical.include = "#include <omp.h>\n#include <math.h>"
omp_critical.stmtd = """
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
omp_critical.stmtf = None
omp_critical.stmtl = None
omp_critical.vals = {'a':1.0}
omp_critical.dots = {'a':1.0}
omp_critical.bars = {'sum':1.0}
omp_test_sum_val=0
omp_test_sum_grad=0
for i in range(100):
  omp_test_sum_val += np.sin(i*1.0)
  omp_test_sum_grad += np.cos(i*1.0)*i
omp_critical.test_vals = {'sum':omp_test_sum_val}
omp_critical.test_dots = {'sum':omp_test_sum_grad}
omp_critical.test_bars = {'a':omp_test_sum_grad}
basiclist.append(omp_critical)

omp_atomic = ClientRequestTestCase("omp_atomic")
omp_atomic.cflags = "-fopenmp -lm"
omp_atomic.cflags_clang = "-fopenmp=libgomp -lm"
omp_atomic.include = "#include <omp.h>\n#include <math.h>"
omp_atomic.stmtd = """
double sum=0;
#pragma omp parallel for
for(int i=0; i<100; i++){
  double s = sin(i*a);
  #pragma omp atomic
    sum += s;
}
"""
omp_atomic.stmtf = None
omp_atomic.stmtl = None
omp_atomic.vals = {'a':1.0}
omp_atomic.dots = {'a':1.0}
omp_atomic.bars = {'sum':1.0}
omp_test_sum_val=0
omp_test_sum_grad=0
for i in range(100):
  omp_test_sum_val += np.sin(i*1.0)
  omp_test_sum_grad += np.cos(i*1.0)*i
omp_atomic.test_vals = {'sum':omp_test_sum_val}
omp_atomic.test_dots = {'sum':omp_test_sum_grad}
omp_atomic.test_bars = {'a':omp_test_sum_grad}
omp_atomic.disable = lambda mode, arch, compiler, typename: arch=='x86' and (compiler=='gcc' or compiler=='g++')
basiclist.append(omp_atomic)

omp_reduction = ClientRequestTestCase("omp_reduction")
omp_reduction.cflags = "-fopenmp -lm"
omp_reduction.cflags_clang = "-fopenmp=libgomp -lm"
omp_reduction.include = "#include <omp.h>\n#include <math.h>"
omp_reduction.stmtd = """
double sum=0;
#pragma omp parallel for reduction(+: sum)
for(int i=0; i<100; i++){
  double s = sin(i*a);
  sum += s;
}
"""
omp_reduction.stmtf = None
omp_reduction.stmtl = None
omp_reduction.vals = {'a':1.0}
omp_reduction.dots = {'a':1.0}
omp_reduction.bars = {'sum':1.0}
omp_test_sum_val=0
omp_test_sum_grad=0
for i in range(100):
  omp_test_sum_val += np.sin(i*1.0)
  omp_test_sum_grad += np.cos(i*1.0)*i
omp_reduction.test_vals = {'sum':omp_test_sum_val}
omp_reduction.test_dots = {'sum':omp_test_sum_grad}
omp_reduction.test_bars = {'a':omp_test_sum_grad}
omp_reduction.disable = lambda mode, arch, compiler, typename: arch=='x86' and (compiler=='gcc' or compiler=='g++')
basiclist.append(omp_reduction)

### Misusing integer and logic operations for floating-point arithmetics ###
exponentadd = ClientRequestTestCase("exponentadd")
exponentadd.stmtd = "double c = a; *((char*)&c+6) += 0x10;"
exponentadd.stmtf = "float c = a; *((char*)&c+2) += 0xf0;"
exponentadd.vals = {'a':3.14}
exponentadd.dots = {'a': -42.0}
exponentadd.bars = {'c': 1.0}
exponentadd.test_vals = {'c':6.28}
exponentadd.test_dots = {'c':-84.0}
exponentadd.test_bars = {'a':2.0}
exponentadd.disable = lambda mode, arch, compiler, typename: True
basiclist.append(exponentadd)

exponentsub = ClientRequestTestCase("exponentsub")
exponentsub.stmtd = "double c = a; *((char*)&c+6) -= 0x10;"
exponentsub.stmtf = "float c = a; *((char*)&c+2) -= 0xf0;"
exponentsub.vals = {'a':6.28}
exponentsub.dots = {'a': -84.0}
exponentsub.bars = {'c': 1.0}
exponentsub.test_vals = {'c':3.14}
exponentsub.test_dots = {'c':-42.0}
exponentsub.test_bars = {'a':0.5}
exponentsub.disable = lambda mode, arch, compiler, typename: True
basiclist.append(exponentadd)

### C++ tests ###
constructornew = ClientRequestTestCase("constructornew")
constructornew.include = "template<typename T> struct A { T t; A(T t): t(t*t) {} };" 
constructornew.stmtd = "A<double>* a = new A<double>(x); double y=a->t; "
constructornew.stmtf = "A<float>* a = new A<float>(x); float y=a->t; "
constructornew.stmtl = "A<long double>* a = new A<long double>(x); long double y=a->t; "
constructornew.disable = lambda mode, arch, compiler, typename: not (compiler=='g++' or compiler=='clang++')
constructornew.vals = {'x': 2.0}
constructornew.dots = {'x': 3.0}
constructornew.bars = {'y': 1.0}
constructornew.test_vals = {'y': 4.0}
constructornew.test_dots = {'y': 12.0}
constructornew.test_bars = {'x': 4.0}
basiclist.append(constructornew)

virtualdispatch = ClientRequestTestCase("virtualdispatch")
virtualdispatch.include = """
template<typename T>
class A { 
  public:
  T t1, t2; 
  A(T t1, T t2): t1(t1), t2(t2) {}
    virtual void operator=(A<T> const& other){
    t1 = other.t1; t2 = other.t2;
  }
};
template<typename T>
class B : public A<T> {
  public:
  using A<T>::t1;
  using A<T>::t2;
  B(T t1, T t2): A<T>(t1,t2) {}
  void operator=(A<T> const& other) override {
    t2 = other.t1; t1 = other.t2;
  }
};
"""
virtualdispatch.stmtd = "B<double> b1(1,x), b2(3,4); b2 = static_cast<A<double> >(b1); double y = b2.t1;"
virtualdispatch.stmtf = "B<float> b1(1,x), b2(3,4); b2 = static_cast<A<float> >(b1); float y = b2.t1;"
virtualdispatch.stmtl = "B<long double> b1(1,x), b2(3,4); b2 = static_cast<A<long double> >(b1); long double y = b2.t1;"
virtualdispatch.disable = lambda mode, arch, compiler, typename: not (compiler=='g++' or compiler=='clang++')
virtualdispatch.vals = {'x': 2.}
virtualdispatch.dots = {'x': 2.1}
virtualdispatch.bars = {'y': 2.1}
virtualdispatch.test_vals = {'y': 2.}
virtualdispatch.test_dots = {'y': 2.1}
virtualdispatch.test_bars = {'x': 2.1}
basiclist.append(virtualdispatch)



### Interactive tests ###
addition_interactive = InteractiveTestCase("addition_interactive")
addition_interactive.stmtd = "double c = a+b;"
addition_interactive.stmtf = "float c = a+b;"
addition_interactive.stmtl = "long double c = a+b;"
addition_interactive.stmtr4 = "real :: c; c= a+b"
addition_interactive.stmtr8 = "double precision :: c; c= a+b"
addition_interactive.vals = {'a':1.0,'b':2.0}
addition_interactive.dots = {'a':3.0,'b':4.0}
addition_interactive.bars = {'c':1.0}
addition_interactive.test_vals = {'c':3.0}
addition_interactive.test_dots = {'c':7.0}
addition_interactive.test_bars = {'a':1.0,'b':1.0}
basiclist.append(addition_interactive)

multiplication_interactive = InteractiveTestCase("multiplication_interactive")
multiplication_interactive.stmtd = "double c = a*b;"
multiplication_interactive.stmtf = "float c = a*b;"
multiplication_interactive.stmtl = "long double c = a*b;"
multiplication_interactive.stmtr4 = "real :: c; c= a*b"
multiplication_interactive.stmtr8 = "double precision :: c; c= a*b"
multiplication_interactive.vals = {'a':1.0,'b':2.0}
multiplication_interactive.dots = {'a':3.0,'b':4.0}
multiplication_interactive.bars = {'c':-3.14}
multiplication_interactive.test_vals = {'c':2.0}
multiplication_interactive.test_dots = {'c':10.0}
multiplication_interactive.test_bars = {'a':-6.28,'b':-3.14}
basiclist.append(multiplication_interactive)

sin_100_interactive = InteractiveTestCase("sin_100_interactive")
sin_100_interactive.include = "#include <math.h>"
sin_100_interactive.ldflags = '-lm'
sin_100_interactive.stmtd = "double c = sin(a);"
sin_100_interactive.stmtf = "float c = sinf(a);"
sin_100_interactive.stmtl = "long double c = sinl(a);"
sin_100_interactive.stmtr4 = "real :: c; c= sin(a)"
sin_100_interactive.stmtr8 = "double precision :: c; c= sin(a)"
sin_100_interactive.vals = {'a':100}
sin_100_interactive.dots = {'a':3.1}
sin_100_interactive.bars = {'c':3.1}
sin_100_interactive.test_vals = {'c':np.sin(100)}
sin_100_interactive.test_dots = {'c':np.cos(100)*3.1}
sin_100_interactive.test_bars = {'a':np.cos(100)*3.1}
basiclist.append(sin_100_interactive)


### Take "cross product" with other configuation options ###
testlist = []
for test_mode in ["dot", "bar"]:
  for test_arch in ["x86", "amd64"]:
    for test_compiler in ["gcc", "g++", "clang", "clang++", "gfortran", "python"]:
      if test_compiler in ["gcc", "g++","clang","clang++"]:
        test_type_list = ["double", "float", "longdouble"]
      elif test_compiler in ["gfortran"]:
        test_type_list = ["real4", "real8"]
      elif test_compiler=='python':
        test_type_list = ["float","np64","np32"]
      for test_type in test_type_list:
        for basictest in basiclist:
          test = copy.deepcopy(basictest)
          test.name = test_mode+"_"+test_arch+"_"+test_compiler+"_"+test_type+"_"+basictest.name

          if test_arch == "x86":
            test.arch = 32
          elif test_arch == "amd64":
            test.arch = 64

          test.compiler = test_compiler
          if test_compiler == "python":
            # If the Python interpreter is 64-bit, disable the 32-bit Python tests
            # and vice versa. 
            if sys.maxsize > 2**32: # if 64 bit
              old = test.disable
              test.disable = lambda mode, arch, compiler, typename : old(mode,arch,compiler,typename) or arch=="x86"
            else:
              old = test.disable
              test.disable = lambda mode, arch, compiler, typename : old(mode,arch,compiler,typename) or arch=="amd64"

          if test_type == "double":
            test.stmt = test.stmtd
            test.type = TYPE_DOUBLE
          elif test_compiler in ["gcc","g++","clang","clang++"] and test_type == "float":
            test.stmt = test.stmtf
            test.type = TYPE_FLOAT
          elif test_type == "longdouble":
            test.stmt = test.stmtl
            test.type = TYPE_LONG_DOUBLE
          elif test_type == "real4":
            test.stmt = test.stmtr4
            test.type = TYPE_REAL4
          elif test_type == "real8":
            test.stmt = test.stmtr8
            test.type = TYPE_REAL8
          elif test_compiler=='python' and test_type == "float":
            test.stmt = test.stmtp
            test.type = TYPE_PYTHONFLOAT
          elif test_type == "np64":
            test.stmt = test.stmtp
            test.type = TYPE_NUMPYFLOAT64
          elif test_type == "np32":
            test.stmt = test.stmtp
            test.type = TYPE_NUMPYFLOAT32

          if test_mode=='dot':
            test.mode='d'
          else:
            test.mode='b'
          if test.stmt!=None and not test.disable(test_mode, test_arch, test_compiler, test_type):
            testlist.append(test)

### Run testcases ###
if not selected_testcase:
  selected_testcase = "*"
outcomes = []
for test in testlist:
  if fnmatch.fnmatchcase(test.name, selected_testcase):
    outcomes.append((test.name, test.run()))

print("Summary:")
number_of_failed_tests = 0
for testname,testresult in outcomes:
  if testresult:
    print("  "+testname+" : PASSED")
  else:
    print("* "+testname+" : FAILED")
    number_of_failed_tests += 1
print(f"Ran {len(outcomes)} tests, {number_of_failed_tests} failed.")

exit(number_of_failed_tests)

  
