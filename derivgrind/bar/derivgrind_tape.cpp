#include <pybind11/pybind11.h>
#include <pybind11/eigen.h>
#include "dg_bar_tape_eval.hpp"
#include <iostream>
#include <fstream>

namespace py = pybind11;
using ull = unsigned long long;

// Chunks with bufsize-many blocks are loaded from the tape file into the heap.
static constexpr ull bufsize = 100;


struct LoadedFile {
  std::ifstream file;

  LoadedFile(std::string filename){
    file.open(filename,std::ios::binary);
    if(!file.good()){
      std::cerr << "Cannot open tape file '" << filename << "/dg-tape'." << std::endl;
    }
  }

  std::function<void(ull,ull,void*)> make_loadfun(){
    return [this](ull i, ull count, void* tape_buf) -> void {
      file.seekg(i*4*sizeof(double), std::ios::beg);
      file.read(reinterpret_cast<char*>(tape_buf), count*4*sizeof(double));
    };
  }

  ull number_of_blocks(){
    file.seekg(0,std::ios::end);
    return file.tellg() / 32;
  }

};


PYBIND11_MODULE(derivgrind_tape, m){
  m.doc() = "Python bindings for the Derivgrind tape evaluator.";

  py::class_<LoadedFile>(m, "LoadedFile")
    .def(py::init<std::string>())
    .def("number_of_blocks", &LoadedFile::number_of_blocks) ;

  using TF = Tapefile<bufsize,std::function<void(ull,ull,void*)>,nullptr>;
  py::class_<TF>(m, "TapeFile")
    .def(py::init<>( [](LoadedFile& file){
        auto loadfun = file.make_loadfun();
        TF* tape = new TF(loadfun,file.number_of_blocks());
        return tape;
      } ) )
    .def("evaluateBackward", &TF::evaluateBackward<Eigen::Ref<Eigen::VectorXd> >)
    .def("evaluateForward", &TF::evaluateForward<Eigen::Ref<Eigen::VectorXd> >)
    .def("stats", [](TF* tape){
        unsigned long long nZero, nOne, nTwo; 
        tape->stats(nZero,nOne,nTwo);
        return std::make_tuple(nZero,nOne,nTwo);
      }) ;
}
