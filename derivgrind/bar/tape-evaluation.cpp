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
 * --record=<tapefile>).
 */

void print_usage(){
  std::cerr << 
    "Usage: ./tape-evaluation tapefile [input index]... [output index=output bar value]...\n"
    "  tapefile     .. Path to the tape file recorded by Derivgrind.\n"
    "  input index  .. Index of input variable as assigned by Derivgrind.\n"
    "  output index .. Index of output variable as assigned by Derivgrind.\n"
    "  output bar value   .. Bar value of output.\n" 
    "If at least one input or output is specified, the adjoints of the inputs\n"
    "are written to stdout.\n"
    "If no inputs and outputs are specified, they are read from the files\n"
    "dg-input-indices dg-output-indices dg-output-adjoints and the input\n"
    "adjoints are written to dg-input-adjoints.\n" ;
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
  unsigned long long* tape = reinterpret_cast<unsigned long long*>(new char[tapesize]);
  tapefile.read(reinterpret_cast<char*>(tape),tapesize);

  // initialize adjoint vector
  unsigned long long nIndex = tapesize / 32;
  double* adjointvec = new double[nIndex];
  for(int index=0; index<nIndex; index++)
    adjointvec[index] = 0.;

  // set bar values of output variables
  if(argc==2){ // from file
    std::ifstream outputindices("dg-output-indices");
    if(!outputindices.good()){ std::cerr << "Error: while opening dg-output-indices." << std::endl; return 1; }
    std::ifstream outputadjoints("dg-output-adjoints");
    if(!outputadjoints.good()){ std::cerr << "Error: while opening dg-output-adjoints." << std::endl; return 1; }
    while(true){
      unsigned long long index;
      outputindices >> index;
      double adjoint;
      outputadjoints >> adjoint;
      if(outputindices.eof() ^ outputadjoints.eof()){
        std::cerr << "Error: Sizes of dg-output-indices and dg-output-adjoints mismatch." << std::endl;
        return 1;
      } else if(outputindices.eof() && outputadjoints.eof()){
        break;
      }
      if(adjointvec[index]!=0.){
        std::cerr << "Warning: The output index " << index << " is specified multiple times, I'm summing up." << std::endl;
      }
      adjointvec[index] += adjoint;
    }
  } else { // from command-line arguments
    for(unsigned long long iArg=2; iArg<argc; iArg++){
      std::string arg(argv[iArg]);
      int eq = arg.find("=");
      if(eq==-1) continue; // specifies an input
      unsigned long long index = stoll(arg.substr(0,eq));
      double barvalue = stod(arg.substr(eq+1));
      if(adjointvec[index]!=0.){
        std::cerr << "Warning: The output index " << index << " is specified multiple times, I'm summing up." << std::endl;
      }
      adjointvec[index] += barvalue;
    }
  }

  // collect input variables
  std::vector<unsigned long long> inputindices_list;
  if(argc==2){ // from file
    std::ifstream inputindices("dg-input-indices");
    if(!inputindices.good()){ std::cerr << "Error: while opening dg-input-indices." << std::endl; return 1; }
    while(true){
      unsigned long long index;
      inputindices >> index;
      if(inputindices.eof()) break;
      inputindices_list.push_back(index);
    }
  } else { // from command-line arguments
    for(unsigned long long iArg=2; iArg<argc; iArg++){
      std::string arg(argv[iArg]);
      int eq = arg.find("=");
      if(eq!=-1) continue; // specifies an output
      unsigned long long index = stoll(arg);
      inputindices_list.push_back(index);
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
      // Right now we get input indices from the user, but we might also do something like this:
      //std::cout << "Input variable " << iIndex << " with adjoint value " << adjointvec[iIndex] << std::endl;
    } else { // variable depends on one or two other variables
      adjointvec[index1] += adjointvec[iIndex] * diff1;
      if(index2!=0) adjointvec[index2] += adjointvec[iIndex] * diff2;
    }
  }

  // output adjoints of inputs
  if(argc==2){ // to file
    std::ofstream inputadjoints("dg-input-adjoints");
    for(unsigned long long index : inputindices_list){
      inputadjoints << std::setprecision(16) << adjointvec[index] << std::endl;
    }
  } else { // to stdout
    for(unsigned long long index : inputindices_list){
      std::cout << adjointvec[index] << std::endl;
    }
  }

  delete[] tape;
  delete[] adjointvec;
}






