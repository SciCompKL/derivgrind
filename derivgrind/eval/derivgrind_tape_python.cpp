/*
   ----------------------------------------------------------------
   Notice that the following MIT license applies to this one file
   (derivgrind_type_python.cpp) only.  The rest of Valgrind is licensed under the
   terms of the GNU General Public License, version 2, unless
   otherwise indicated.  See the COPYING file in the source
   distribution for details.
   ----------------------------------------------------------------

   This file is part of Derivgrind, an automatic differentiation
   tool applicable to compiled programs.

   Copyright (C) 2022, Chair for Scientific Computing, TU Kaiserslautern
   Copyright (C) since 2023, Chair for Scientific Computing, University of Kaiserslautern-Landau
   Homepage: https://www.scicomp.uni-kl.de
   Contact: Prof. Nicolas R. Gauger

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
   (derivgrind_tape_python.cpp) only.  The rest of Valgrind is licensed under the
   terms of the GNU General Public License, version 2, unless
   otherwise indicated.  See the COPYING file in the source
   distribution for details.
   ----------------------------------------------------------------
*/

#include <pybind11/pybind11.h>
#include <pybind11/eigen.h>
#include "dg_bar_tape_eval.hpp"
#include <iostream>
#include <fstream>

namespace py = pybind11;
using ull = unsigned long long;

// Chunks with bufsize-many blocks are loaded from the tape file into the heap.
static constexpr ull bufsize = 100;


struct LoadedFile {
  std::ifstream file;

  LoadedFile(std::string filename){
    file.open(filename,std::ios::binary);
    if(!file.good()){
      std::cerr << "Cannot open tape file '" << filename << "/dg-tape'." << std::endl;
    }
  }

  std::function<void(ull,ull,void*)> make_loadfun(){
    return [this](ull i, ull count, void* tape_buf) -> void {
      file.seekg(i*4*sizeof(double), std::ios::beg);
      file.read(reinterpret_cast<char*>(tape_buf), count*4*sizeof(double));
    };
  }

  ull number_of_blocks(){
    file.seekg(0,std::ios::end);
    return file.tellg() / 32;
  }

};


PYBIND11_MODULE(derivgrind_tape, m){
  m.doc() = "Python bindings for the Derivgrind tape evaluator.";

  py::class_<LoadedFile>(m, "LoadedFile")
    .def(py::init<std::string>())
    .def("number_of_blocks", &LoadedFile::number_of_blocks) ;

  using TF = Tapefile<bufsize,std::function<void(ull,ull,void*)>,nullptr>;
  py::class_<TF>(m, "TapeFile")
    .def(py::init<>( [](LoadedFile& file){
        auto loadfun = file.make_loadfun();
        TF* tape = new TF(loadfun,file.number_of_blocks());
        return tape;
      } ) )
    .def("evaluateBackward", &TF::evaluateBackward<Eigen::Ref<Eigen::VectorXd> >)
    .def("evaluateForward", &TF::evaluateForward<Eigen::Ref<Eigen::VectorXd> >)
    .def("stats", [](TF* tape){
        unsigned long long nZero, nOne, nTwo; 
        tape->stats(nZero,nOne,nTwo);
        return std::make_tuple(nZero,nOne,nTwo);
      }) ;
}
