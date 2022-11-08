#include <iostream>
#include <cstdio>
#include <dlfcn.h>

/*! \file derivgrind-library-caller.cpp
 * Simple program registering inputs, loading and calling a function, 
 * and registering outputs.
 *
 * This program serves the use-case that a program needs to run and record
 * a single "external" function in a Derivgrind process, while the program 
 * itself does not run under Derivgrind. 
 *
 * Usage:
 * derivgrind-library-caller library.so functionname fptype 
 *
 * The signature of the external function must be 
 *     void(int, void*, fptype const*, fptype*)
 * The int and void* arguments specify the size and beginning of a buffer
 * containing non-differentiable input to the function. It might be used 
 * to pass hyperparameters like the size of the differentiable input and
 * output.
 * The fptype const* and fptype* arguments specify the beginning of the
 * contiguous input and output buffer. fptype should be either float or 
 * double.
 * 
 * This program expects to receive a binary stream in stdin, specifying,
 * in the following order,
 * - the size of the void* non-differentiable input buffer as an uint64,
 * - the respective number of bytes to fill the buffer,
 * - the number of elements of the differentiable input buffer as an uint64,
 * - the respective number of bytes to fill the buffer,
 * - the number of elements of the differentiable output buffer as an uint64.
 * It will then stream the output buffer back. Regarding the tape and the lists
 * of indices, the calling process must read them from the file system.
 */

template<typename fptype>
int main_fp(int argc, char* argv[]){
  // load library and symbol
  void* loaded_lib = dlopen(argv[1], RTLD_NOW);
  if(!loaded_lib){
    std::cerr << "Error loading shared object '" << argv[1] << "':\n" << dlerror() << std::endl;
    exit(EXIT_FAILURE);
  }
  using fptr = void (*)(fptype*, fptype*);
  fptr loaded_fun = (fptr)dlsym(loaded_lib, argv[2]);
  if(!loaded_fun){
    std::cerr << "Error loading symbol '" << argv[2] <<"':\n" << dlerror() << std::endl;
    exit(EXIT_FAILURE);
  }

  // read binary input from stdin
  freopen(NULL, "rb", stdin);
  unsigned long long nondiff_input_size;
  std::cin.read((char*)&nondiff_input_size, 8);
  char* nondiff_input = new char[nondiff_input_size];
  std::cin.read(nondiff_input, nondiff_input_size);
  unsigned long long diff_input_count;
  std::cin.read((char*)&diff_input_count, 8);
  fptype diff_input = new fptype[diff_input_count];
  std::cin.read((char*)diff_input, sizeof(fptype)*diff_input_count);
  unsigned long long diff_output_count;
  std::cin.read((char*)&diff_output_count, 8);
  fptype diff_output = new fptype[diff_output_count];

  // call the function
  loaded_fun(nondiff_input_size, nondiff_input, diff_input, diff_output);

  // write binary output to stdout
  freopen(NULL, "rb", stdout);
  std::cout.write(diff_output, sizeof(fptype)*diff_output_count);

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
