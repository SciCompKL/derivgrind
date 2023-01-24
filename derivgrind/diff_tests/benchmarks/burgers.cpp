
/*
   This file is part of Derivgrind, an automatic differentiation
   tool applicable to compiled programs.

   Copyright (C) 2022, Chair for Scientific Computing, TU Kaiserslautern
   Copyright (C) since 2023, Chair for Scientific Computing, University of Kaiserslautern-Landau
   Homepage: https://www.scicomp.uni-kl.de
   Contact: Prof. Nicolas R. Gauger

   Lead developer: Max Aehle

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, see <http://www.gnu.org/licenses/>.

   The GNU General Public License is contained in the file COPYING.
*/

/*!
 * Note that if you combine this file with CoDiPack (-DCODI_DOT or -DCODI_BAR),
 * the result will also be subject to the license terms of CoDiPack, which are those
 * of the GNU General Public License version 3 or later. 
 *
 * Original author: Max Sagebaum
 * Modified by: Max Aehle
 */

/*! \file burgers.cpp
 * Simple program solving Burgers' PDE, as a benchmark for Derivgrind.
 * 
 * Compile the program with a flag -Dx_y where x=DG,CODI specifies the AD tool
 * and y=DOT,BAR specifies the mode.
 */
#include <iostream>
#include <fstream>
#include <sstream>
#include <chrono>
#include "burgers-Problem.hpp"
#include <unistd.h>
#include <stdlib.h>
#include <iomanip>

#include "performanceTestMacros.hpp"

int main(int nArgs, char** args) {

  Problem<DOUBLE, double> problem;
  #if defined(CODI_BAR)
    typename DOUBLE::Tape& tape = DOUBLE::getTape();
    tape.setActive();
  #endif

  auto props = problem.setup(nArgs, args);

  // == Seed / register inputs. ==
  for (size_t i = 0; i < props.totalSize; ++i) {
    HANDLE_INPUT(problem.uStart[i]);
    problem.u1[i] = problem.uStart[i];
  }
  for (size_t i = 0; i < props.totalSize; ++i) {
    HANDLE_INPUT(problem.vStart[i]);
    problem.v1[i] = problem.vStart[i];
  }

  // == Solve PDE, measure recording time. ==
  std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();
  problem.mainLoop(problem.u1, problem.u2, problem.v1, problem.v2, props);
  DOUBLE w = 0.0;
  problem.computeL2Norm(problem.u1, problem.v1, props, w);
  std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
  double time = std::chrono::duration_cast<std::chrono::microseconds>(end - begin).count();

  // == Memory measurement. ==
  std::stringstream s;
  s << "cat /proc/"<<getpid()<<"/status | awk '$1~/VmHWM:/ {print $2}' > tmp_memory_measurement ";
  system(s.str().c_str());
  std::ifstream memfile("tmp_memory_measurement");
  int mem;
  memfile >> mem;

  // == Store results. ==
  std::ofstream resfile(args[1]);
  resfile << "{" << std::endl;
  resfile << "\"forward_time_in_s\": " << time/1e6 << ",\n"
          << "\"forward_vmhwm_in_kb\": " << mem << ",\n"
          << std::setprecision(16)
          << "\"output\" : [ " << w << " ]";
  #if defined(DG_DOT)
    double w_d;
    DG_GET_DOTVALUE(&w, &w_d, 8);
    resfile << ",\n \"output_dot\" : [" << w_d << " ]";
  #elif defined(DG_BAR)
    DG_OUTPUTF(w);
  #elif defined(CODI_DOT)
    resfile << ",\n \"output_dot\" : [" << w.getGradient() << " ]";
  #elif defined(CODI_BAR)
    tape.registerOutput(w);
    tape.setPassive();
    w.setGradient(one);
    {
      std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();
      tape.evaluate();
      std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
      double time = std::chrono::duration_cast<std::chrono::microseconds>(end - begin).count();
      resfile << ",\n \"reverse_time_in_s\": " << time/1e6 ;
    }
    resfile << ",\n \"number_of_jacobians\" : " << tape.getParameter(codi::TapeParameters::JacobianSize);
    resfile << ",\n \"tape_size_in_b\" : " << 
      (5 * tape.getParameter(codi::TapeParameters::StatementSize) + 
       12 * tape.getParameter(codi::TapeParameters::JacobianSize) );
    resfile << ",\n \"input_bar\" : [" << problem.u1[0].getGradient();
    for (size_t i = 1; i < props.totalSize; ++i){
      resfile << ", " << problem.uStart[i].getGradient();
    }
    for (size_t i = 0; i < props.totalSize; ++i){
      resfile << ", " << problem.vStart[i].getGradient();
    }
    resfile << "]";
  #endif
  resfile << "\n}";

  problem.clear();
  #if defined(CODI_BAR)
    tape.reset();
  #endif

}
