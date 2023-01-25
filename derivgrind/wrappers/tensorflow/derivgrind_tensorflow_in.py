
#
#  ----------------------------------------------------------------
#  Notice that the following MIT license applies to this one file
#  only.  The rest of Valgrind is licensed under the
#  terms of the GNU General Public License, version 2, unless
#  otherwise indicated.  See the COPYING file in the source
#  distribution for details.
#  ----------------------------------------------------------------
#
#  This file is part of Derivgrind, an automatic differentiation
#  tool applicable to compiled programs.
#
#  Copyright (C) 2022, Chair for Scientific Computing, TU Kaiserslautern
#  Copyright (C) since 2023, Chair for Scientific Computing, University of Kaiserslautern-Landau
#  Homepage: https://www.scicomp.uni-kl.de
#  Contact: Prof. Nicolas R. Gauger
#
#  Lead developer: Max Aehle
#
#  Permission is hereby granted, free of charge, to any person obtaining a copy
#  of this software and associated documentation files (the "Software"), to deal
#  in the Software without restriction, including without limitation the rights
#  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
#  copies of the Software, and to permit persons to whom the Software is
#  furnished to do so, subject to the following conditions:
#  
#  The above copyright notice and this permission notice shall be included in all
#  copies or substantial portions of the Software.
#  
#  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
#  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
#  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
#  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
#  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
#  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
#  SOFTWARE.
#
#  ----------------------------------------------------------------
#  Notice that the above MIT license applies to this one file
#  only.  The rest of Valgrind is licensed under the
#  terms of the GNU General Public License, version 2, unless
#  otherwise indicated.  See the COPYING file in the source
#  distribution for details.
#  ----------------------------------------------------------------
#


import tensorflow as tf
import numpy as np
import struct
import tempfile
import os
import subprocess

#! \file derivgrind_tensorflow_in.py
# Template for a Python module exposing Derivgrind-differentiated 
# library functions to TensorFlow.
#
# Usage:
# 
#     x = tf.Variable([4.0],dtype=tf.float64)
#     with tf.GradientTape() as tape:
#       y = derivgrind("/path/to/mylib.so", "myfun").apply(b"",x,1)
#     dy_dx = tape.gradient(y,x)
#     print(dy_dx.numpy())
# 
# Here, myfun is a symbol defined by the shared object mylib.so 
# that is either contained in the LD_LIBRARY_PATH or the default 
# library path, e.g. specified by the full path.
# 
# The bytestring (here b"") can be used to pass any non-differentiable 
# parameters to the function, x contains the differentiable inputs,
# and the third argument (here 1) specifies the number of expected
# outputs. 
#
# The signature of myfun must be
#
#     void functionname(int, char*, int, fptype const*, int, fptype*)
#
# The three pairs of an integer and a pointer specify the size/count of
# bytes/scalars in the parameter, input and output buffer, respectively.
#
# Basically, we create a function with the tf.custom_gradient decorator
# that returns
# - the output computed during the forward pass, and
# - a function performing the tape evaluation pass.
# 
# The relative directory containing the valgrind and tape-evaluation 
# executables is hard-coded in the end of this file. During the build 
# process, another assignment with the proper installation directory 
# is appended.
#
#


def derivgrind(library,functionname,arch='amd64'):
  """Create a class with a DerivgrindLibraryCaller Tensorflow custom-gradient decorated function apply().
    @param library Full path to the shared object.
    @param functionname Symbol name of the compiled function.
    @param arch Target architecture of the library, 'amd64' (default) or 'x86'.
  """
  class DerivgrindLibraryCaller:
    @tf.custom_gradient
    def apply(params, input, noutput):
      if input.dtype==tf.float32:
        fptype="float"
      elif input.dtype==tf.float64:
        fptype="double"
      else:
        raise Exception("Unhandled TensorFlow tensor type.")

      tempdir = tempfile.TemporaryDirectory()
#      os.mkfifo(tempdir.name+"/dg-libcaller-params")
#      os.mkfifo(tempdir.name+"/dg-libcaller-inputs")
#      os.mkfifo(tempdir.name+"/dg-libcaller-outputs")
#      os.mkfifo(tempdir.name+"/dg-tape")
#      os.mkfifo(tempdir.name+"/dg-input-indices")
#      os.mkfifo(tempdir.name+"/dg-output-indices")
      with open(tempdir.name+"/dg-libcaller-params", "wb") as param_buf:
        param_buf.write(params)
      with open(tempdir.name+"/dg-libcaller-inputs", "wb") as input_buf:
        input.numpy().tofile(input_buf)

      forward_process = subprocess.run([bin_path+"/valgrind", "--quiet", "--tool=derivgrind", "--record="+tempdir.name, libexec_path+"/valgrind/derivgrind-library-caller-"+arch, library, functionname, fptype, str(len(params)), str(len(input.numpy())), str(noutput), tempdir.name])
      
      with open(tempdir.name+"/dg-libcaller-outputs",'rb') as output_buf:
        output = tf.Variable(np.fromfile(output_buf, dtype=input.numpy().dtype, count=noutput))
      with open(tempdir.name+"/dg-tape",'rb') as tape_buf:
        ctx_tape = tape_buf.read()
      with open(tempdir.name+"/dg-input-indices",'rb') as inputindices_buf:
        ctx_inputindices = inputindices_buf.read()
      with open(tempdir.name+"/dg-output-indices",'rb') as outputindices_buf:
        ctx_outputindices = outputindices_buf.read()
      ctx_ninput = len(input.numpy())

      def grad(grad_output):
        tempdir = tempfile.TemporaryDirectory()
#       os.mkfifo(tempdir.name+"/dg-tape")
#       os.mkfifo(tempdir.name+"/dg-input-indices")
#       os.mkfifo(tempdir.name+"/dg-output-indices")
#       os.mkfifo(tempdir.name+"/dg-input-bars")
#       os.mkfifo(tempdir.name+"/dg-output-bars")
        with open(tempdir.name+"/dg-tape","wb") as tape_buf:
          tape_buf.write(ctx_tape)
        with open(tempdir.name+"/dg-input-indices","wb") as inputindices_buf:
          inputindices_buf.write(ctx_inputindices)
        with open(tempdir.name+"/dg-output-indices","wb") as outputindices_buf:
          outputindices_buf.write(ctx_outputindices)
        
        with open(tempdir.name+"/dg-output-bars","w") as outputbars_buf:
          outputbars_buf.writelines([str(float(bar))+"\n" for bar in grad_output])

        backward_process = subprocess.run([bin_path+"/tape-evaluation", tempdir.name])

        grad_input_np = np.empty(ctx_ninput,dtype=grad_output.numpy().dtype)
        with open(tempdir.name+"/dg-input-bars","r") as inputbars_buf:
          for i in range(ctx_ninput):
            grad_input_np[i] = float( inputbars_buf.readline().strip() )
        grad_input = tf.Variable(grad_input_np)
          
        return (None,grad_input,None)

      return output, grad

  return DerivgrindLibraryCaller

bin_path = os.path.dirname(__file__)+"/../../../install/bin"
libexec_path = os.path.dirname(__file__)+"/../../../install/libexec"
