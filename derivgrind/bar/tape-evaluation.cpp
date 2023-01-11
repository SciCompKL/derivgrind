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
    exit(1); \
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

/*! Read vector of scalars from text file.
 *  \param filename Relative or absolute path.
 *  \returns Vector of scalars stored in the text file.
 */
template<typename T>
std::vector<T> readFromTextFile(std::string filename){
  std::ifstream file(filename);
  WARNING(!file.good(), "Error: while opening '"<<filename<<"'.")
  std::vector<T> result;
  while(true){
    T data;
    file >> data;
    if(file.eof()) break;
    result.push_back(data);
  }
  return result;
}

/*! Write vector of scalars to text file.
 *  \param filename Relative or absolute path.
 *  \param data Vector of scalars to be stored in the text file.
 */
template<typename T>
void writeToTextFile(std::string filename, std::vector<T> data){
  std::ofstream file(filename);
  WARNING(!file.good(), "Error: while opening '"<<filename<<"'.")
  for(unsigned int i=0; i<data.size(); i++){
    file << std::setprecision(16) << data[i] << "\n";
  }
}

/*! Read indices and gradients from text files and seed the gradient vector accordingly.
 *  \param filename_indices Relative or absolute path to text file containing indices.
 *  \param filename_gradients Relative or absolute path to text file containing dot or bar values.
 *  \param gradient_vector Gradient vector to be seeded.
 */
template<typename Vector>
void seedGradientVectorFromTextFile(std::string filename_indices, std::string filename_gradients, Vector& gradient_vector){
  std::vector<ull> indices = readFromTextFile<ull>(filename_indices);
  std::vector<double> gradients = readFromTextFile<double>(filename_gradients);
  WARNING(indices.size()!=gradients.size(),
          "Error: Sizes of '"<<filename_indices<<"' and '"<<filename_gradients<<"' mismatch.")
  for(unsigned int i=0; i<indices.size(); i++){
    gradient_vector[indices[i]] += gradients[i];
  }
}

/*! Read indices from a text file, extract the corresponding derivatives
 *  from the gradient vector, and write them to another text file.
 *  \param filename_indices Relative or absolute path to text file containing indices.
 *  \param filename_gradients Relative or absolute path to text file in which gradients are to be stored.
 *  \param gradient_vector Gradient vector from which derivatives are to be extracted.
 */
template<typename Vector>
void readGradientVectorToTextFile(std::string filename_indices, std::string filename_gradients, Vector const& gradient_vector){
  std::vector<ull> indices = readFromTextFile<ull>(filename_indices);
  std::vector<double> gradients(indices.size());
  for(unsigned int i=0; i<indices.size(); i++){
    gradients[i] = gradient_vector[indices[i]];
  }
  writeToTextFile(filename_gradients, gradients);
}

int main(int argc, char* argv[]){

  // open tape file
  if(argc<2){ // too few arguments
    std::cerr << "Usage: " << argv[0] << " path [--stats|--forward]" << std::endl;
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
    seedGradientVectorFromTextFile(path+"/dg-output-indices", path+"/dg-output-adjoints", adjointvec);
  } else { // set dot values of input variables
    seedGradientVectorFromTextFile(path+"/dg-input-indices", path+"/dg-input-dots", adjointvec);
  }

  // evaluate tape
  if(!forward){
    tape->evaluateBackward(adjointvec);
  } else {
    tape->evaluateForward(adjointvec);
  }

  if(!forward){ // read indices of input variables and write corresponding adjoints
    readGradientVectorToTextFile(path+"/dg-input-indices", path+"/dg-input-adjoints", adjointvec);
  } else { // read indices of output variables and write corresponding dots
    readGradientVectorToTextFile(path+"/dg-output-indices", path+"/dg-output-dots", adjointvec);
  }

  if(measure_evaluation_time){
    std::ofstream timefile(path+"/dg-perf-tapeeval-time");
    double time = std::chrono::duration_cast<std::chrono::microseconds>(tend - tbegin).count();
    timefile << time/1e6 << std::endl;
  }

  delete[] adjointvec;
}

