/*--------------------------------------------------------------------*/
/*--- Wrap client request     derivgrind_clientrequests_python.cpp ---*/
/*--- functions for Python.                                        ---*/
/*--------------------------------------------------------------------*/

/*
   ----------------------------------------------------------------
   Notice that the following MIT license applies to this one file
   only.  The rest of Valgrind is licensed under the
   terms of the GNU General Public License, version 2, unless
   otherwise indicated.  See the COPYING file in the source
   distribution for details.
   ----------------------------------------------------------------

   This file is part of Derivgrind, an automatic differentiation
   tool applicable to compiled programs.

   Copyright (C) 2022, Chair for Scientific Computing, TU Kaiserslautern
   Copyright (C) since 2023, Chair for Scientific Computing, University of Kaiserslautern-Landau
   Homepage: https://www.scicomp.uni-kl.de
   Contact: Prof. Nicolas R. Gauger (derivgrind@projects.rptu.de)

   Lead developer: Max Aehle

   Permission is hereby granted, free of charge, to any person obtaining a copy
   of this software and associated documentation files (the "Software"), to deal
   in the Software without restriction, including without limitation the rights
   to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
   copies of the Software, and to permit persons to whom the Software is
   furnished to do so, subject to the following conditions:

   The above copyright notice and this permission notice shall be included in all
   copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
   OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
   SOFTWARE.

   ----------------------------------------------------------------
   Notice that the above MIT license applies to this one file
   only.  The rest of Valgrind is licensed under the
   terms of the GNU General Public License, version 2, unless
   otherwise indicated.  See the COPYING file in the source
   distribution for details.
   ----------------------------------------------------------------
*/

#include <pybind11/pybind11.h>
#include "derivgrind.h"

namespace py = pybind11;

PYBIND11_MODULE(derivgrind, m){
  m.doc() = "Wrapper for Derivgrind client requests.";

  // Forward mode
  m.def( "set_dotvalue", [](double val, double grad)->double { 
    double ret = val; 
    DG_SET_DOTVALUE(&ret, &grad, 8);
    return ret; 
  });
  m.def( "get_dotvalue", [](double val)->double { 
    double grad; 
    DG_GET_DOTVALUE(&val, &grad, 8);
    return grad; 
  });
  m.def( "set_dotvalue", [](float val, float grad)->float { 
    float ret = val; 
    DG_SET_DOTVALUE(&ret, &grad, 4);
    return ret; 
  });
  m.def( "get_dotvalue", [](float val)->float { 
    float grad; 
    DG_GET_DOTVALUE(&val, &grad, 4);
    return grad; 
  });

  // Recording mode
  m.def( "inputf", [](double val)->double {
    double ret = val;
    DG_INPUTF(ret);
    return ret;
  });
  m.def( "outputf", [](double val)->void {
    DG_OUTPUTF(val);
  });
}
