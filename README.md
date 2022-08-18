*This README is about Derivgrind; see [`README_ORIGINAL`](README_ORIGINAL)
for information about Valgrind.*

# Derivgrind

Derivgrind is a tool for forward-mode algorithmic differentiation (AD) 
of compiled programs, implemented in the [Valgrind](https://valgrind.org/)
instrumentation framework for building dynamic analysis tools. 

For more information, have a look at our paper.

## Building Derivgrind
Make sure you have the required tools, headers and libraries, which are listed in
[environment.def](environment.def). You may run a Singularity image built from 
this file to reproducibly obtain an environment containing all dependencies.

Download and compile Derivgrind with the following commands:

    git clone --recursive <repository url>
    cd valgrind/derivgrind
    python3 gen_replace_math.py
    cd ..
    ./autogen.sh
    ./configure --prefix=$PWD/install
    make install
    cd derivgrind/diff_tests/
    ./setup.py
    cd ../..

## Running Testcases

      cd derivgrind/diff_tests/
      python3 run_tests.py
      cd ../..

## Differentiating a Simple C Program
Compile a simple C program
      
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
via `gcc test.c -o test -Iinstall/include` and run it via
   
    install/bin/valgrind --tool=derivgrind ./test
Enter an input value `x` on the command line (e.g. `4.0`), and see whether 
the derivative computed by Derivgrind matches `3*x*x` (e.g. `48.0`).

The two preprocessor macros seed `x`, and obtain the dot value from `y`, 
via Valgrind's client request mechanism. Access to the client 
program's source code are only necessary to set and get dot values. 
In between, the client program may e.g. make calls to closed-source 
libraries.

## Differentiating a Python Interpreter
To apply Derivgrind on the Python interpreter of your system, which is likely a 
version of [CPython](https://github.com/python/cpython/), run

    PYTHONPATH=$PWD/derivgrind/diff_tests/python install/bin/valgrind --tool=derivgrind python3 
This provides you with a (slow) Python interpreter with AD functionality:

    >>> import derivgrind
    >>> x = derivgrind.set_derivative(4,1)
    >>> y = x*x*x
    >>> y
    64.0
    >>> derivgrind.get_derivative(y)
    48.0


## <a name="limitations"></a>Limitations
- Machine code can "hide" real arithmetic behind integer or logical instructions 
  in manifold ways. For example, a bitwise logical "and" can be used to set the
  sign bit to zero, and thereby compute the absolute value. Derivgrind does only
  recognize the most important of these constructs. Thus, avoid direct manipulation 
  of a floating-point number's binary representation in your program, and avoid 
  differentiation of highly optimized numerical libraries.
- Valgrind might not know all the instructions used in your program, and makes 
  some floating-point operations behave slightly differently than they do outside
  of Valgrind.

We give more details on our paper.
  

