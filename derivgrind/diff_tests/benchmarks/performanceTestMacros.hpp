
/*
   This file is part of Derivgrind, an automatic differentiation
   tool applicable to compiled programs.

   Copyright (C) 2022, Chair for Scientific Computing, TU Kaiserslautern
   Copyright (C) since 2023, Chair for Scientific Computing, University of Kaiserslautern-Landau
   Homepage: https://www.scicomp.uni-kl.de
   Contact: Prof. Nicolas R. Gauger

   Lead developer: Max Aehle

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, see <http://www.gnu.org/licenses/>.

   The GNU General Public License is contained in the file COPYING.
*/

/*
 * Note that if you combine this file with CoDiPack (-DCODI_DOT or -DCODI_BAR),
 * the result will also be subject to the license terms of CoDiPack, which are those
 * of the GNU General Public License version 3 or later. 
 */

#ifndef PERFORMANCETESTMACROS_HPP
#define PERFORMANCETESTMACROS_HPP

/*! \file performanceTestMacros.hpp
 * Include into the performance test client code.
 * 
 * Then declare inputs as
 *
 *     DOUBLE input = 3;
 *     HANDLE_INPUT(input);
 *
 */

static const double one = 1.;

#if defined(DG_DOT) 
  #include <valgrind/derivgrind.h> 
  #define DOUBLE double
  #define HANDLE_INPUT(var) DG_SET_DOTVALUE(&var, &one, 8);
#elif defined(DG_BAR) 
  #include <valgrind/derivgrind.h>
  #define DOUBLE double
  #define HANDLE_INPUT(var) DG_INPUTF(var)
#elif defined(CODI_DOT)
  #include "codi.hpp"
  #define DOUBLE codi::RealForward
  #define HANDLE_INPUT(var) var.setGradient(one);
#elif defined(CODI_BAR) 
  #include "codi.hpp"
  #define DOUBLE codi::RealReverse
  #define HANDLE_INPUT(var) tape.registerInput(var);
#else // no AD
  #define DOUBLE double
  #define HANDLE_INPUT(var) 
#endif 

#endif // PERFORMANCETESTMACROS_HPP
