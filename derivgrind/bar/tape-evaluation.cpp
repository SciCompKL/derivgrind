#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <iomanip>

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

int main(int argc, char* argv[]){

  constexpr ull bufsize = 100;

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

  auto loadfun = [&tapefile](ull i, ull count, ull* tape_buf){
    tapefile.seekg(i*4*sizeof(double), std::ios::beg);
    tapefile.read(reinterpret_cast<char*>(tape_buf), count*4*sizeof(double));
  };

  Tapefile<bufsize,decltype(loadfun)> tape(loadfun, number_of_blocks);

  if(argc>=3 && std::string(argv[2])=="--stats"){
    unsigned long long nZero, nOne, nTwo;
    tape.stats(nZero,nOne,nTwo);
    std::cout << nZero << " " << nOne << " " << nTwo << std::endl;
    exit(0);
  }

  // initialize adjoint vector
  double* adjointvec = new double[number_of_blocks];
  for(ull index=0; index<number_of_blocks; index++){
    adjointvec[index] = 0.;
  }

  // set bar values of output variables
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

  // evaluate tape
  tape.evaluate(adjointvec);


  // read indices of input variables and write corresponding adjoints
  std::ifstream inputindices(path+"/dg-input-indices");
  WARNING(!inputindices.good(), "Error: while opening dg-input-indices.")
  std::ofstream inputadjoints(path+"/dg-input-adjoints");
  while(true){
    ull index;
    inputindices >> index;
    if(inputindices.eof()) break;
    inputadjoints << std::setprecision(16) << adjointvec[index] << std::endl;
  }

  delete[] adjointvec;
}

