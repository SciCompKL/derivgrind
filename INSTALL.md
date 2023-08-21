# Installing Derivgrind

The Derivgrind package has the following main components:
- The executable file `derivgrind` that bundles the Valgrind framework with the Derivgrind instrumentation tool. 
- The executable file `tape-evaluation` that allows to compute reverse-mode derivatives, using the tape file recorded by `derivgrind` in recording mode.
- The C/C++ header file `derivgrind.h`, which users can `#include` into their primal program in order to declare input and output variables in an easy way.
- Additional header and shared object files wrapping this header to Python and Fortran.
- The executable file `derivgrind-config` that provides flags for C, C++ and Fortran compilers, so they can find the above headers and wrappers.

You can [build Derivgrind from source](), or download a pre-built installation via [snap](). 

## Building from source
Make sure you have the required tools, headers and libraries, which are listed in
[environment.def](environment.def). You may run a Singularity image built from 
this file to reproducibly obtain an environment that contains all dependencies.

Clone this repository with `git clone --recursive`, and run the following commands in the root directory of the repository: 
```bash
./autogen.sh
./configure
make install
```

You may provide the following additional flags to `./configure`:
- `--prefix=<path>` specifies the installation directory. If you provide a path that your user account
  can write to (e.g. `$PWD/install`), you do not need superuser rights!
- `--enable-python` to build Python wrappers, requires the Python development headers to be installed on your system.
- `--enable-fortran` to build Fortran wrappers, requires a Fortran compiler to be installed on your system.
- `SHADOW_LAYERS_64=16,16,16,16` to specify an alternative structure of the shadow memory tool, which requires less memory but is slightly slower.

## Installation with Snap
TODO
