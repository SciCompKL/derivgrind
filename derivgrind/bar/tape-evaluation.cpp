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

using ull = unsigned long long;

#define BUFSIZE 1000000

void print_usage(){
  std::cerr << 
    "Usage: ./tape-evaluation path [input index]... [output index=output bar value]...\n"
    "  tapefile     .. Path to the directory containing the file dg-tape recorded by Derivgrind.\n"
    "  input index  .. Index of input variable as assigned by Derivgrind.\n"
    "  output index .. Index of output variable as assigned by Derivgrind.\n"
    "  output bar value   .. Bar value of output.\n" 
    "If at least one input or output is specified, the adjoints of the inputs\n"
    "are written to stdout.\n"
    "If no inputs and outputs are specified, they are read from the files\n"
    "path/dg-input-indices path/dg-output-indices path/dg-output-adjoints\n"
    "and the input adjoints are written to path/dg-input-adjoints.\n" ;
}


int main(int argc, char* argv[]){
  // read tape file into memory
  if(argc<2){ print_usage();return 1; }
  std::string path = argv[1];
  std::ifstream tapefile(path+"/dg-tape",std::ios::binary);
  if(!tapefile.good()){ std::cerr << "Cannot open tape file '"<<argv[1]<<"'." << std::endl; return 1; }
  tapefile.seekg(0,std::ios::end);
  ull tapesize = tapefile.tellg(); // in bytes
  ull nIndex = tapesize / 32; // number of entries

  // initialize tape buffer
  ull* tape_buf = new ull[4*BUFSIZE];
  char* tape_buf_c = reinterpret_cast<char*>(tape_buf);

  // initialize adjoint vector
  double* adjointvec = new double[nIndex];
  for(int index=0; index<nIndex; index++)
    adjointvec[index] = 0.;

  // set bar values of output variables
  if(argc==2){ // from file
    std::ifstream outputindices(path+"/dg-output-indices");
    if(!outputindices.good()){ std::cerr << "Error: while opening dg-output-indices." << std::endl; return 1; }
    std::ifstream outputadjoints(path+"/dg-output-adjoints");
    if(!outputadjoints.good()){ std::cerr << "Error: while opening dg-output-adjoints." << std::endl; return 1; }
    while(true){
      ull index;
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
    for(ull iArg=2; iArg<argc; iArg++){
      std::string arg(argv[iArg]);
      int eq = arg.find("=");
      if(eq==-1) continue; // specifies an input
      ull index = stoll(arg.substr(0,eq));
      double barvalue = stod(arg.substr(eq+1));
      if(adjointvec[index]!=0.){
        std::cerr << "Warning: The output index " << index << " is specified multiple times, I'm summing up." << std::endl;
      }
      adjointvec[index] += barvalue;
    }
  }

  // collect input variables
  std::vector<ull> inputindices_list;
  if(argc==2){ // from file
    std::ifstream inputindices(path+"/dg-input-indices");
    if(!inputindices.good()){ std::cerr << "Error: while opening dg-input-indices." << std::endl; return 1; }
    while(true){
      ull index;
      inputindices >> index;
      if(inputindices.eof()) break;
      inputindices_list.push_back(index);
    }
  } else { // from command-line arguments
    for(ull iArg=2; iArg<argc; iArg++){
      std::string arg(argv[iArg]);
      int eq = arg.find("=");
      if(eq!=-1) continue; // specifies an output
      ull index = stoll(arg);
      inputindices_list.push_back(index);
    }
  }

  // load the last 1,...,BUFSIZE entries of the tape
  // such that the number of previous entries is a multiple of BUFSIZE
  ull nIndexBeginning = (nIndex-1)%BUFSIZE + 1;
  tapefile.seekg((nIndex-nIndexBeginning)*32, std::ios::beg);
  tapefile.read(tape_buf_c,nIndexBeginning*32);

  // reverse evaluation of tape
  for(ull iIndex=nIndex-1; iIndex!=0; iIndex--){
    ull index1 = tape_buf[4*(iIndex%BUFSIZE)];
    ull index2 = tape_buf[4*(iIndex%BUFSIZE)+1];
    double diff1 = *reinterpret_cast<double*>(&tape_buf[4*(iIndex%BUFSIZE)+2]);
    double diff2 = *reinterpret_cast<double*>(&tape_buf[4*(iIndex%BUFSIZE)+3]);
    if(adjointvec[iIndex]!=0) {
      // The !=0 check is necessary in situations like this:
      // a = 0 is an input
      // b = 1/a is an irrelevant intermediate variable
      // c = a+1 is the output
      // The division by 0 is not defined, but as c does not depend
      // on b, this should not affect the derivatives.
      // The adjoint update for the assignment to b would add
      // (adjoint value of b = 0) * (partial derivative db/da = -inf)
      // = NaN to the adjoint of a if the check were not performed.
      if(index1!=0) adjointvec[index1] += adjointvec[iIndex] * diff1;
      if(index2!=0) adjointvec[index2] += adjointvec[iIndex] * diff2;
    }
    if(iIndex%BUFSIZE==0 && iIndex>0){ // load tape blocks of BUFSIZE elements 
      tapefile.seekg((iIndex-BUFSIZE)*32, std::ios::beg);
      tapefile.read(tape_buf_c, BUFSIZE*32);
    }
  }

  // output adjoints of inputs
  if(argc==2){ // to file
    std::ofstream inputadjoints(path+"/dg-input-adjoints");
    for(ull index : inputindices_list){
      inputadjoints << std::setprecision(16) << adjointvec[index] << std::endl;
    }
  } else { // to stdout
    for(ull index : inputindices_list){
      std::cout << adjointvec[index] << std::endl;
    }
  }

  delete[] tape_buf;
  delete[] adjointvec;
}

