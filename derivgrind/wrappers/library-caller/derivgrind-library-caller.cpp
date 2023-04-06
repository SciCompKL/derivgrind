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

#include <iostream>
#include <fstream>
#include <cstdio>
#include <dlfcn.h>
#include "derivgrind.h"

/*! \file derivgrind-library-caller.cpp
 * Simple program registering inputs, loading and calling a function, 
 * and registering outputs.
 *
 * This program serves the use-case that a program needs to run and record
 * a single "external" function in a Derivgrind process, while the program 
 * itself does not run under Derivgrind. 
 *
 * Usage:
 * derivgrind-library-caller library.so functionname fptype nParam nInput nOutput path
 *
 * This calls the symbol `functionname` from library.so, providing nParam
 * bytes of non-differentiable parameters, nInput differentiable input scalars of
 * type fptype, and nOutput differentiable output scalars if type fptype.
 *
 * The signature of the external function must be 
 *     void functionname(int, char*, int, fptype const*, int, fptype*)
 * The three pairs of an integer and a pointer specify the size/count of
 * bytes/scalars in the parameter, input and output buffer, respectively.
 * 
 * This program gets the parameters and inputs from binary files 
 * dg-libcaller-param, dg-libcaller-inputs, 
 * which it expects in the specified path.
 * It will then write the output buffer into a binary file dg-libcaller-outputs
 * in the same path. 
 * If can make sense to set the `--record` argument to Derivgrind to this path as well.
 */

template<typename fptype>
int main_fp(int argc, char* argv[]){
  // load library and symbol
  void* loaded_lib = dlopen(argv[1], RTLD_NOW);
  if(!loaded_lib){
    std::cerr << "Error loading shared object '" << argv[1] << "':\n" << dlerror() << std::endl;
    exit(EXIT_FAILURE);
  }
  using fptr = void (*)(int,char*,int,fptype const*,int, fptype*);
  fptr loaded_fun = (fptr)dlsym(loaded_lib, argv[2]);
  if(!loaded_fun){
    std::cerr << "Error loading symbol '" << argv[2] <<"':\n" << dlerror() << std::endl;
    exit(EXIT_FAILURE);
  }

  // sizes of non-differentiable parameters, differentiable inputs, differentiable outputs
  long long param_size, input_count, output_count;
  try { // parse from command-line arguments
    param_size = std::stoll(argv[4]);
    input_count = std::stoll(argv[5]);
    output_count = std::stoll(argv[6]);
  } catch (std::invalid_argument const& ex) {
    std::cerr << "Invalid argument:\n" << ex.what() << std::endl;
    exit(EXIT_FAILURE);
  } catch (std::out_of_range const& ex) {
    std::cerr << "Argument out of range:\n" << ex.what() << std::endl;
    exit(EXIT_FAILURE);
  }

  // buffers for non-diff parameters, diff inputs, diff outputs
  char* param_buf;
  fptype *input_buf, *output_buf;
 
  // read content from files
  std::string path = argv[7];
  std::ifstream param_file(path+"/dg-libcaller-params", std::ios::binary);
  std::ifstream input_file(path+"/dg-libcaller-inputs", std::ios::binary);

  param_buf = new char[param_size+1]; // +1 to avoid allocations of length zero
  input_buf = new fptype[input_count+1];
  output_buf = new fptype[output_count+1];
  if(!param_buf || !input_buf || !output_buf){
    std::cerr << "Failure to allocate buffers." << std::endl;
    exit(EXIT_FAILURE);
  }

  param_file.read((char*)param_buf, param_size);
  input_file.read((char*)input_buf, input_count*sizeof(fptype));

  // register inputs
  for(unsigned long long i=0; i<input_count; i++){
    DG_INPUTF(input_buf[i]);
  }
  // call the function
  loaded_fun(param_size, param_buf, input_count, input_buf, output_count, output_buf);
  // register outputs
  for(unsigned long long i=0; i<output_count; i++){
    DG_OUTPUTF(output_buf[i]);
  }

  // write binary output
  std::ofstream output_file(path+"/dg-libcaller-outputs", std::ios::binary);
  output_file.write((char*)output_buf, sizeof(fptype)*output_count);

  return 0;
}


int main(int argc, char* argv[]){
  if(argc<4){
    std::cerr << "Error: Bad number of arguments." << std::endl;
    exit(EXIT_FAILURE);
  }
  if(argv[3][0]=='d') return main_fp<double>(argc,argv);
  else if(argv[3][0]=='f') return main_fp<float>(argc,argv);
  else {
    std::cerr << "Error: Bad floating point type specification '" << argv[3] << "'" << std::endl;
    exit(EXIT_FAILURE);
  }
}
