/*--------------------------------------------------------------------*/
/*--- Wrap client request            derivgrind_clientrequests.cpp ---*/
/*--- functions for Python.                                        ---*/
/*--------------------------------------------------------------------*/

/*
   This file is part of Derivgrind, a tool performing forward-mode
   algorithmic differentiation of compiled programs, implemented
   in the Valgrind framework.

   Copyright (C) 2022 Chair for Scientific Computing (SciComp), TU Kaiserslautern
   Homepage: https://www.scicomp.uni-kl.de
   Contact: Prof. Nicolas R. Gauger (derivgrind@scicomp.uni-kl.de)

   Lead developer: Max Aehle (SciComp, TU Kaiserslautern)

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

#include <pybind11/pybind11.h>
#include <valgrind/derivgrind.h>

namespace py = pybind11;

PYBIND11_MODULE(derivgrind, m){
  m.doc() = "Wrapper for Derivgrind client requests.";

  m.def( "set_derivative", [](double val, double grad)->double { 
    double ret = val; 
    VALGRIND_SET_DERIVATIVE(&ret, &grad, 8);
    return ret; 
  });
  m.def( "get_derivative", [](double val)->double { 
    double grad; 
    VALGRIND_GET_DERIVATIVE(&val, &grad, 8);
    return grad; 
  });
  m.def( "set_derivative", [](float val, float grad)->float { 
    float ret = val; 
    VALGRIND_SET_DERIVATIVE(&ret, &grad, 4);
    return ret; 
  });
  m.def( "get_derivative", [](float val)->float { 
    float grad; 
    VALGRIND_GET_DERIVATIVE(&val, &grad, 4);
    return grad; 
  });
}
