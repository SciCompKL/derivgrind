Replacement libm for DerivGrind
===============================

Differentiating programs on the level of machine code 
instructions sometimes compute the derivatives of the
wrong thing:

For instance, some versions of the glibc libm do not use
the x86 fsin instruction to implement sin. Rather,
they use several numerical approximations depending on
the magnitude of the angle. We observed that sometimes,
the derivatives of these approximation of sine seem to be 
bad approximations of the derivatives of sine. 
So we should utilize our knowledge on the exact analytical 
derivatives instead of relying on "black-box" 
differentiation of approximations.
A replacement libm allows us to wrap the libm functions in
order to perform our additional differentiation logic.

The replacement libm can use the functions of the 
original libm by dynamic loading. 
Access to the gradient information in DerivGrind is given 
through the Valgrind client request mechanism.

