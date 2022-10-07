#include <iostream>
#include <fstream>
#include <string>

/*! \file tape-evaluation.cpp
 * Simple program to perform the "backpropagation" / tape evaluation 
 * step of reverse-mode AD. 
 * 
 * This program is independent from Valgrind, which performs the 
 * recording step of reverse-mode AD (when invoked with 
 * --record=<tapefile>).
 */

void print_usage(){
  std::cerr << 
    "Usage: ./tape-evaluation tapefile [output index=output bar value]...\n"
    "  tapefile    .. Path to the tape file recorded by Derivgrind.\n"
    "  output index.. Index of output variable as assigned by Derivgrind.\n"
    "  output bar value   .. Bar value of output.\n" 
    "If no outputs were specified, they are read from the file dg-outputs.\n";
}

unsigned long long sizeOfStream(std::ifstream& stream){
  stream.seekg(0,std::ios::end);
  unsigned long long size = stream.tellg();
  stream.seekg(0,std::ios::beg);
  return size;
}

int main(int argc, char* argv[]){
  // read tape file into memory
  if(argc<2){ print_usage();return 1; }
  std::ifstream tapefile(argv[1],std::ios::binary);
  if(!tapefile.good()){ std::cerr << "Cannot open tape file '"<<argv[1]<<"'." << std::endl; return 1; }
  unsigned long long tapesize = sizeOfStream(tapefile);
  unsigned long long* tape = new unsigned long long[tapesize];
  tapefile.read(reinterpret_cast<char*>(tape),tapesize);

  // initialize adjoint vector
  unsigned long long nIndex = tapesize / 32;
  double* adjoint = new double[nIndex];
  for(int index=0; index<nIndex; index++)
    adjoint[index] = 0.;

  // set bar values of output variables
  if(argc==2){ // from file
    std::ifstream outputindices("dg-outputindices",std::ios::binary);
    std::ifstream outputadjoints("dg-outputadjoints",std::ios::binary);
    unsigned long long nOutput = sizeOfStream(outputindices) / 8;
    if(nOutput != sizeOfStream(outputadjoints)/8){
      std::cerr << "Sizes of dg-outputindices and dg-outputadjoints mismatch." << std::endl;
      return 1;
    }
    for(unsigned long long iOutput=0; iOutput<nOutput; iOutput++){
      unsigned long long index;
      outputindices.read(reinterpret_cast<char*>(&index),8);
      double barvalue;
      outputadjoints.read(reinterpret_cast<char*>(&barvalue),8);
      adjoint[index] = barvalue;
    }
  } else { // from command-line arguments
    unsigned long long nOutput = argc-2;
    for(unsigned long long iOutput=0; iOutput<nOutput; iOutput++){
      std::string arg(argv[2+iOutput]);
      int eq = arg.find("=");
      if(eq==-1){ print_usage(); return 1; }
      unsigned long long index = stoll(arg.substr(0,eq));
      double barvalue = stod(arg.substr(eq+1));
      adjoint[index] = barvalue;
    }
  }

  // reverse evaluation of tape
  tapefile.seekg(-4,std::ios::end);
  for(unsigned long long iIndex=nIndex-1; iIndex!=0; iIndex--){
    unsigned long long index1 = tape[4*iIndex];
    unsigned long long index2 = tape[4*iIndex+1];
    double diff1 = *reinterpret_cast<double*>(&tape[4*iIndex+2]);
    double diff2 = *reinterpret_cast<double*>(&tape[4*iIndex+3]);
    if(index1==0 && index2==0){ // input variable
      std::cout << "Input variable " << iIndex << " with adjoint value " << adjoint[iIndex] << std::endl;
    } else { // variable depends on one or two other variables
      adjoint[index1] += adjoint[iIndex] * diff1;
      if(index2!=0) adjoint[index2] += adjoint[iIndex] * diff2;
    }
  }
  delete[] tape;
  delete[] adjoint;
}






