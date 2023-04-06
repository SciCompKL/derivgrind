/*
   ----------------------------------------------------------------
   Notice that the following MIT license applies to this one file
   (tape-evaluation-utils.hpp) only.  The rest of Valgrind is licensed under the
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
   (tape-evaluation-utils.hpp) only.  The rest of Valgrind is licensed under the
   terms of the GNU General Public License, version 2, unless
   otherwise indicated.  See the COPYING file in the source
   distribution for details.
   ----------------------------------------------------------------
*/

#ifndef TAPEEVALUATIONUTILS_HPP
#define TAPEEVALUATIONUTILS_HPP

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <iomanip>

using ull = unsigned long long;

#define WARNING(condition, message) \
  if(condition) {\
    std::cerr << message << std::endl; \
    exit(1); \
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

#endif // TAPEEVALUATIONUTILS_HPP
