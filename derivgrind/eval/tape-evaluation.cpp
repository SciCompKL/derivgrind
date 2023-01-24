/*
   ----------------------------------------------------------------
   Notice that the following MIT license applies to this one file
   (tape-evaluation.cpp) only.  The rest of Valgrind is licensed under the
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
   (tape-evaluation.cpp) only.  The rest of Valgrind is licensed under the
   terms of the GNU General Public License, version 2, unless
   otherwise indicated.  See the COPYING file in the source
   distribution for details.
   ----------------------------------------------------------------
*/

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <iomanip>
#include <chrono>

/*! \file tape-evaluation.cpp
 * Simple program to perform the "backpropagation" / tape evaluation 
 * step of reverse-mode AD. 
 * 
 * This program is independent from Valgrind, which performs the 
 * recording step of reverse-mode AD (when invoked with 
 * --record=path).
 */

using ull = unsigned long long;

#include "dg_bar_tape_eval.hpp"

#define WARNING(condition, message) \
  if(condition) {\
    std::cerr << message << std::endl; \
    return 1; \
  }

// Chunks with bufsize-many blocks are loaded from the tape file into the heap.
static constexpr ull bufsize = 100;

// In order to run a performance measurement, set measure_evaluation_time
// to true and make bufsize large enough so only a single chunk is loaded.
static constexpr bool measure_evaluation_time = false;
static std::chrono::steady_clock::time_point tbegin;
static std::chrono::steady_clock::time_point tend = tbegin;
static bool tbegin_has_been_taken = false;
void eventhandler(TapefileEvent event){
  if(measure_evaluation_time){
    if(event==EvaluateChunkBegin){
      if(tbegin_has_been_taken){
        std::cerr << "Performance measurements have been activated but chunk size is not large enough." << std::endl;
        exit(1);
      }
      tbegin = std::chrono::steady_clock::now();
      tbegin_has_been_taken = true;
    } else if(event==EvaluateChunkEnd) {
      tend = std::chrono::steady_clock::now();
    }
  }
}

int main(int argc, char* argv[]){

  // open tape file
  if(argc<2){ // to few arguments
    std::cerr << "Usage: " << argv[0] << " path [--stats]" << std::endl;
    return 1;
  }
  std::string path = argv[1];
  std::ifstream tapefile(path+"/dg-tape",std::ios::binary);
  WARNING(!tapefile.good(), "Cannot open tape file '"<<argv[1]<<"/dg-tape'.")
  tapefile.seekg(0,std::ios::end);
  ull number_of_blocks = tapefile.tellg() / 32; // number of entries

  auto loadfun = [&tapefile](ull i, ull count, ull* tape_buf) -> void {
    tapefile.seekg(i*4*sizeof(double), std::ios::beg);
    tapefile.read(reinterpret_cast<char*>(tape_buf), count*4*sizeof(double));
  };

  Tapefile<bufsize,decltype(loadfun),eventhandler>* tape = new Tapefile<bufsize,decltype(loadfun),eventhandler>(loadfun, number_of_blocks);

  if(argc>=3 && std::string(argv[2])=="--stats"){
    unsigned long long nZero, nOne, nTwo;
    tape->stats(nZero,nOne,nTwo);
    std::cout << nZero << " " << nOne << " " << nTwo << std::endl;
    exit(0);
  }
  bool forward = false; // if true, perform forward evaluation of tape instead of reverse evaluation
  if(argc>=3 && std::string(argv[2])=="--forward"){
    forward = true;
  }

  // initialize adjoint vector
  double* adjointvec = new double[number_of_blocks];
  for(ull index=0; index<number_of_blocks; index++){
    adjointvec[index] = 0.;
  }

  if(!forward) { // set bar values of output variables
    std::ifstream outputindices(path+"/dg-output-indices");
    WARNING(!outputindices.good(), "Error: while opening dg-output-indices.")
    std::ifstream outputadjoints(path+"/dg-output-adjoints");
    WARNING(!outputadjoints.good(), "Error: while opening dg-output-adjoints.")
    while(true){
      ull index;
      outputindices >> index;
      double adjoint;
      outputadjoints >> adjoint;
      if(outputindices.eof() ^ outputadjoints.eof()){
        WARNING(true, "Error: Sizes of dg-output-indices and dg-output-adjoints mismatch.")
      } else if(outputindices.eof() && outputadjoints.eof()){
        break;
      }
      adjointvec[index] += adjoint;
    }
  } else { // set dot values of input variables
    std::ifstream inputindices(path+"/dg-input-indices");
    WARNING(!inputindices.good(), "Error: while opening dg-input-indices.")
    std::ifstream inputdots(path+"/dg-input-dots");
    WARNING(!inputdots.good(), "Error: while opening dg-input-dots.")
    while(true){
      ull index;
      inputindices >> index;
      double dot;
      inputdots >> dot;
      if(inputindices.eof() ^ inputdots.eof()){
        WARNING(true, "Error: Sizes of dg-input-indices and dg-input-dots mismatch.")
      } else if(inputindices.eof() && inputdots.eof()){
        break;
      }
      adjointvec[index] += dot;
    }
  }

  // evaluate tape
  if(!forward){
    tape->evaluateBackward(adjointvec);
  } else {
    tape->evaluateForward(adjointvec);
  }

  if(!forward){ // read indices of input variables and write corresponding adjoints
    std::ifstream inputindices(path+"/dg-input-indices");
    WARNING(!inputindices.good(), "Error: while opening dg-input-indices.")
    std::ofstream inputadjoints(path+"/dg-input-adjoints");
    while(true){
      ull index;
      inputindices >> index;
      if(inputindices.eof()) break;
      inputadjoints << std::setprecision(16) << adjointvec[index] << std::endl;
    }
  } else { // read indices of output variables and write corresponding dots
    std::ifstream outputindices(path+"/dg-output-indices");
    WARNING(!outputindices.good(), "Error: while opening dg-output-indices.")
    std::ofstream outputdots(path+"/dg-output-dots");
    while(true){
      ull index;
      outputindices >> index;
      if(outputindices.eof()) break;
      outputdots << std::setprecision(16) << adjointvec[index] << std::endl;
    }
  }

  if(measure_evaluation_time){
    std::ofstream timefile(path+"/dg-perf-tapeeval-time");
    double time = std::chrono::duration_cast<std::chrono::microseconds>(tend - tbegin).count();
    timefile << time/1e6 << std::endl;
  }

  delete[] adjointvec;
}

