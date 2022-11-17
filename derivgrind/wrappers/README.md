Derivgrind Client Request Wrappers
==================================

This directory contains sources for wrappers that expose Derivgrind client requests to different languages and systems:
- `compiled` builds a library libderivgrind_clientrequests.a providing functions performing client requests.
- `fortran` builds a Fortran 90 .mod that translates libderivgrind_clientrequests.a into Fortran functions.
- `python3` builds a Python extension module containing the macros.

Additionally, we provide a setup to apply Derivgrind to library functions from other AD tools:
- `library-caller` contains the small C program which loads the library and runs the function, and to which Derivgrind is applied.
- `torch` contains the Python module satisfying PyTorch's autograd.Function interface.

