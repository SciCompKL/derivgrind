*This README is about Derivgrind; see [README_ORIGINAL](README_ORIGINAL)
for information about Valgrind.*

# Derivgrind

Derivgrind is an automatic differentiation tool applicable to compiled programs.
It has been implemented in the [Valgrind](https://valgrind.org/)
framework for building dynamic analysis tools. 

For more information beyond this [README.md](README.md), you may have a look at our papers:
- M. Aehle, J. Blühdorn, M. Sagebaum, N. R. Gauger: *Forward-Mode Automatic Differentiation of Compiled Programs*. [arXiv:2209.01895](https://arxiv.org/abs/2209.01895), 2022.
- M. Aehle, J. Blühdorn, M. Sagebaum, N. R. Gauger: *Reverse-Mode Automatic Differentiation of Compiled Programs*. [arXiv:2212.13760](https://arxiv.org/abs/2212.13760), 2022.

## Building Derivgrind
Make sure you have the required tools, headers and libraries, which are listed in
[environment.def](environment.def). You may run a Singularity image built from 
this file to reproducibly obtain an environment that contains all dependencies.

Clone this repository with `git clone --recursive`, and run the following commands in the root directory: 
```bash
./autogen.sh
./configure --prefix=$PWD/install --enable-python --enable-fortran
make install
```
The flags `--enable-python` and `--enable-fortran` enable wrappers for client request (see below), which
are necessary to run all unit tests successfully. For many applications, you may however leave them out.

## Running Testcases

In `derivgrind/diff_tests`, run something like
```bash
python3 run_tests.py dot_amd64_gcc_double_addition
```
The names of the unit tests are composed of an AD mode, architecture, language/compiler, floating-point type and 
arithmetic formula. You may use `*` as a wildcard to run several tests at once. You may specify the 
Derivgrind installation directory with `--prefix=path`. Specify a directory with `--tempdir=path` if
you want to inspect the temporary files created by Derivgrind and the test system.

## Differentiating a Simple C++ Program in Forward Mode
Compile a simple C++ "client" program from 
```c++
// forward.cpp:

#include <iostream>
#include <valgrind/derivgrind.h>

int main(){
  double x, x_d = 1;
  std::cin >> x;
  DG_SET_DOTVALUE(&x,&x_d,sizeof(double));
  double y = x*x*x, y_d;
  DG_GET_DOTVALUE(&y,&y_d,sizeof(double));
  std::cout << "f(" << x << ") = " << y << std::endl;
  std::cout << "df/dx(" << x << ") = " << y_d << std::endl;
}
```
via `g++ forward.cpp -o forward -Iinstall/include`. Here, `install`
is the directory specified by `--prefix` during `./configure` if Derivgrind 
was built from source, or the unpacked .tar.gz archive containing the binary release. 

Then, run `forward` via
```bash
install/bin/valgrind --tool=derivgrind ./forward
```
if Derivgrind was built from source, or 
```bash
install/bin/derivgrind ./forward
```
with the binary release.
Enter an input value `x` on the command line (e.g. `4.0`), and see whether 
the derivative computed by Derivgrind matches the analytic derivative
`3*x*x` (e.g. `48.0`).

The two preprocessor macros seed `x` and obtain the dot value from `y`, 
respectively, via Valgrind's client request mechanism. Access to the client 
program's source code is only necessary to identify input and output variables
in this way. In between, the client program may,  e. g., make calls to closed-source 
libraries.

## Differentiating a Simple C++ Program in Reverse Mode
Compile another simple C++ "client" program from
```c++
// reverse.cpp:

#include <iostream>
#include <valgrind/derivgrind.h>

int main(){
  double x;
  std::cin >> x;
  DG_INPUTF(x);
  double y = x*x*x;
  DG_OUTPUTF(y);
}
```
via `g++ reverse.cpp -o reverse -Iinstall/include` and run it via
```bash
install/bin/valgrind --tool=derivgrind --record=$PWD ./reverse
```
if Derivgrind was built from source, or
```bash
install/bin/derivgrind --record=$PWD ./reverse
```
with the binary release.
Enter an input value `x` on the command line (e.g. `4.0`). Once the program
finishes, you should find a binary file `dg-tape` storing the recorded tape,
and text files `dg-input-indices`, `dg-output-indices` storing identifiers for
`x` and `y`, respectively. Create a text file `dg-output-bars` containing 
`1.0` and run 
```bash
install/bin/tape-evaluation $PWD
``` 
This stores the reverse-mode automatic derivative in a text file `dg-input-bars`. 
If you have multiple input or output variables, their bar values should appear 
in the respective text files as separate lines, in the same order in which they
are declared.

To evaluate the tape in forward direction, create a text file `dg-input-dots` containing
the dot values of the inputs, and call `tape-evaluation $PWD --forward`. This will
store the dot values of the outputs in `dg-output-dots`. This alternative way
of computing forward-mode derivatives can be more efficient than the way presented above,
if you have many input variables.

Instead of `$PWD`, you can choose any other directory with sufficient read/write permissions.
Placing the directory on a ramdisk like `/dev/shm/` might speed the recording up.

## Additional Features
- Specifying `SHADOW_LAYERS_64=16,16,16,16` behind the `./configure` command during build
  will reduce the upfront memory allocation on a 64-bit system from 4 GB to under 100 MB,
  at the price of a slightly slower execution.
- If the client program has been compiled with debugging symbols and optimizations turned off,
  interactive *monitor commands* provide an alternative to inserting client requests into the
  code. Start Valgrind with `--vgdb-error=0` and follow the instructions to connect a GDB
  session, in which you set breakpoints and query for addresses of variables, which you can then
  pass to Valgrind via monitor commands. 

## Limitations
- Derivgrind differentiates programs in a "black-box fashion", and does not provide
  more sophisticated techniques like checkpointing, preaccumulation, or reverse accumulation.
- Machine code can "hide" real arithmetic behind integer or logical instructions 
  in manifold ways. For example, a bitwise logical "and" can be used to set the
  sign bit to zero, and thereby compute the absolute value. Derivgrind recognizes only
  the most important of these constructs. More details can be found in our 
  [forward-mode paper](https://arxiv.org/abs/2209.01895). Generally, avoid direct manipulation 
  of a floating-point number's binary representation in your program, and avoid the 
  differentiation of highly optimized numerical libraries.
- While Valgrind supports many more platforms, only X86/Linux and AMD64/Linux 
  are supported by Derivgrind at the moment.
- Valgrind and Derivgrind might not know all the instructions used in your program.
  For example, Valgrind stops when it reads an AVX-512 instruction. Furthermore, Valgrind 
  makes some floating-point operations behave slightly differently than they do outside of Valgrind.
- Running a program under Derivgrind will significantly slow it down. For our 
  Burgers' PDE benchmark compiled in release mode, we have measured a factor of 
  around 30 in forward mode and 180 for the tape recording, respectively.

## License
Derivgrind is licensed under the GNU General Public License, version 2. 
Read the file [COPYING](COPYING) in the source distribution for details.

Some parts of Derivgrind might end up being permanently inserted into 
the client program, like the `valgrind/derivgrind.h` header providing the client
request macros. Generally, we have put these parts under the permissive
MIT license, and we try to avoid third-party dependencies that would induce a strong copyleft 
in these parts. 

  

