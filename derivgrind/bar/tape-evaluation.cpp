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

  Tapefile<bufsize,decltype(loadfun)>* tape = new Tapefile<bufsize,decltype(loadfun)>(loadfun, number_of_blocks);

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

  delete[] adjointvec;
}

