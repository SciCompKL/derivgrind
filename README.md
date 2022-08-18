# Derivgrind

Derivgrind is a tool for forward-mode algorithmic differentiation (AD) 
of compiled programs, implemented in the [Valgrind](https://valgrind.org/)
instrumentation framework for building dynamic analysis tools. 

We give more details on Derivgrind's mechanics in our paper.

## Quick start
- Make sure you have the required tools, headers and libraries, which are listed in
  [environment.def](environment.def). You may run a Singularity image built from 
  this file to reproducibly obtain an environment where everything is installed.
- Download and compile the source code:

      git clone --recursive <repository url>
      cd valgrind/derivgrind
      python3 gen_replace_math.py
      cd ..
      ./autogen.sh
      ./configure --prefix=$PWD/install
      make install
- Then either run the testcases, which together take some hours:

      cd derivgrind/diff_tests/
      ./setup.sh
      python3 run_tests.py
- Or try Derivgrind on the following simple C program:
      
      #include <stdio.h>
      #include <valgrind/derivgrind.h>
      int main(){
        double x, x_d = 1;
        scanf("%lf", &x);
        VALGRIND_SET_DERIVATIVE(&x,&x_d,sizeof(double));
        double y = x*x*x, y_d;
        VALGRIND_GET_DERIVATIVE(&y,&y_d,sizeof(double));
        printf("f(%lf) = %lf\n", x, y);
        printf("df/dx(%lf) = %lf\n", x, y_d);
      }
  Compile it via `gcc test.c -o test -Iinstall/include` and run it via
   
      install/bin/valgrind --tool=derivgrind ./test
  The two preprocessor macros seed `x`, and obtain the dot value from `y`, 
  via Valgrind's client request mechanism. In between, the code can 
  perform any kind of calculations, including calls to closed-source libraries.
  However, have a look at the [limitations](#limitations).
- Or try Derivgrind on the Python interpreter on your system, which should be a 
  version of [CPython](https://github.com/python/cpython/):

      cd derivgrind/diff_tests/
      ./setup.sh && -
      export PYTHONPATH=$PWD/python
      install/bin/valgrind --tool=derivgrind python3 
  This provides you with a (slow) Python interpreter with AD functionality:

      >>> import derivgrind
      >>> x = derivgrind.set_derivative(4,1)
      >>> y = x*x*x
      >>> y
      64.0
      >>> derivgrind.get_derivative(y)
      48.0




## <a name="limitations"></a>Limitations
- Machine code can "hide" real arithmetics behind integer or logical instructions 
  in manifold ways. For example, a bitwise logical "and" can be used to pull the
  sign bit to zero, and thereby compute the absolute value. Derivgrind does only
  recognize the some of these ways. Avoid direct manipulation of a floating-point
  number's binary representation in your program, and avoid using highly optimized 
  numerical libraries.
- Valgrind might not know all the instructions used in your program, and makes 
  some floating-point operations behave slightly differently than they do outside
  of Valgrind.

We give more details on our paper.
  

