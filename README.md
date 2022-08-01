# DerivGrind

DerivGrind is a tool for forward-mode algorithmic differentiation (AD) 
of compiled programs, implemented in the [Valgrind](https://valgrind.org/)
instrumentation framework for building dynamic analysis tools. 

We give more details on DerivGrind's mechanics in our paper.

## Quick start
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
- Or check out the following simple program:
      
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
  The two preprocessor macros seed x, and obtain the dot value from y, 
  via Valgrind's client request mechanism. In between, the code can 
  perform any kind of calculations, including calls to closed-source libraries.
  However, have a look at the limitations.

## Limitations
- Machine code can "hide" real arithmetics behind integer or logical instructions 
  in manifold ways. For example, a bitwise logical "and" can be used to pull the
  sign bit to zero, and thereby compute the absolute value. DerivGrind does only
  recognize the more frequent patterns.
- Valgrind might not know all the instructions used in your program, and makes 
  some floating-point operations behave slightly differently than they do outside
  Valgrind.

