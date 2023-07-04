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
#include <set>

/*! \file tape-evaluation.cpp
 * Simple program to perform the "backpropagation" / tape evaluation 
 * step of reverse-mode AD. 
 * 
 * This program is independent from Valgrind, which performs the 
 * recording step of reverse-mode AD (when invoked with 
 * --record=path).
 */

#include "dg_bar_tape_eval.hpp"
#include "tape-evaluation-utils.hpp"

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
  if(argc<2){ // too few arguments
    std::cerr << "Usage: " << argv[0] << " path [--stats|--forward|--print]" << std::endl;
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

  if(argc>=3 && std::string(argv[2])=="--print"){
    std::vector<ull> inputindices_vec = readFromTextFile<ull>(path+"/dg-input-indices");
    std::set<ull> inputindices_set(inputindices_vec.begin(), inputindices_vec.end());
    std::vector<ull> outputindices_vec = readFromTextFile<ull>(path+"/dg-output-indices");
    std::set<ull> outputindices_set(outputindices_vec.begin(), outputindices_vec.end());

    tape->iterate(0,number_of_blocks-1, [&inputindices_set,&outputindices_set](ull index, ull index1, ull index2, double diff1, double diff2){
      std::cout << "|------------------|------------------|------------------|\n";
      std::cout << "| " << std::setfill(' ') << std::setw(16) << std::hex << index;
      std::cout << " | " << std::setfill(' ') << std::setw(16) << std::hex << index1;
      std::cout << " | " << std::setfill(' ') << std::setw(16) << std::hex << index2 << " |\n";
      bool is_input = inputindices_set.count(index);
      bool is_output = outputindices_set.count(index);
      if(index==0){
        std::cout << "|            dummy | ";
      } else if(is_input && is_output){
        std::cout << "|     input/output | ";
      } else if (is_input) {
        std::cout << "|            input | ";
      } else if (is_output) {
        std::cout << "|           output | ";
      } else {
        std::cout << "|                  | ";
      }
      std::cout << std::setfill(' ') << std::setw(16) << std::scientific << diff1;
      std::cout << " | " << std::setfill(' ') << std::setw(16) << std::scientific << diff2 << " |\n";
    });
    std::cout << "|------------------|------------------|------------------|" << std::endl;
    exit(0);
  }

  bool forward = false; // if true, perform forward evaluation of tape instead of reverse evaluation
  if(argc>=3 && std::string(argv[2])=="--forward"){
    forward = true;
  }

  // Initialize the derivative vector ("adjoint vector") storing the bar values, 
  // or dot values if the user specified --forward.
  double* derivativevec = new double[number_of_blocks];
  for(ull index=0; index<number_of_blocks; index++){
    derivativevec[index] = 0.;
  }

  if(forward){
    seedGradientVectorFromTextFile(path+"/dg-input-indices", path+"/dg-input-dots", derivativevec);
    tape->evaluateForward(derivativevec);
    readGradientVectorToTextFile(path+"/dg-output-indices", path+"/dg-output-dots", derivativevec);
  } else {
    seedGradientVectorFromTextFile(path+"/dg-output-indices", path+"/dg-output-bars", derivativevec);
    tape->evaluateBackward(derivativevec);
    readGradientVectorToTextFile(path+"/dg-input-indices", path+"/dg-input-bars", derivativevec);
  }

  if(measure_evaluation_time){
    std::ofstream timefile(path+"/dg-perf-tapeeval-time");
    double time = std::chrono::duration_cast<std::chrono::microseconds>(tend - tbegin).count();
    timefile << time/1e6 << std::endl;
  }

  delete[] derivativevec;
}

