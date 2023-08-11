
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
#  Contact: Prof. Nicolas R. Gauger (derivgrind@projects.rptu.de)
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

import torch
import numpy as np
import struct
import tempfile
import os
import subprocess

#! \file derivgrind_torch_in.py
# Template for a Python module exposing Derivgrind-differentiated 
# library functions to PyTorch.
#
# Usage:
# 
#     x = torch.tensor([4.0],dtype=torch.float64,requires_grad=True)
#     y = derivgrind("/path/to/mylib.so", "myfun").apply(b"",x,1)
#     y.backward()
#     print(x.grad)
# 
# Here, myfun is a symbol defined by the shared object mylib.so 
# that is either contained in the LD_LIBRARY_PATH or the default 
# library path, e.g. specified by the full path.
# 
# The bytestring (here b"") can be used to pass any non-differentiable
# parameters to the function. The second argument x contains the 
# differentiable inputs. The third argument (here 1) specifies the number 
# of expected outputs. 
#
# The signature of myfun must be
#
#     void myfun(int, char*, int, fptype const*, int, fptype*)
#
# The argument list consists of three pairs of an integer and a pointer.
# They refer to the three buffers of non-differentiable parameters, 
# AD inputs and AD outputs, respectively. The pointer stores the start
# address of the buffer while the integer specifies its length, in bytes
# (non-differentiable parameters) or scalars (AD inputs and outputs).
#
# Basically, we create a class derived from autograd.Function and
# providing functions 
# - `forward` for the tape recording pass, creating a Derivgrind 
#   process to record the evaluation of the library function, and
# - `backward` for the tape evaluation pass, creating a 
#   tape-evaluation process. 
# 
# The relative directory containing the valgrind and tape-evaluation 
# executables is hard-coded in the end of this file. During the build 
# process, another assignment with the proper installation directory 
# is appended.
#
#


def derivgrind(library,functionname,arch='amd64'):
  """Create a DerivgrindLibraryCaller Torch Function class.
    @param library Full path to the shared object.
    @param functionname Symbol name of the compiled function.
    @param arch Target architecture of the library, 'amd64' (default) or 'x86'.
  """
  class DerivgrindLibraryCaller(torch.autograd.Function):
    @staticmethod
    def forward(ctx, params, input, noutput):
      if input.dtype==torch.float32:
        fptype="float"
      elif input.dtype==torch.float64:
        fptype="double"
      else:
        raise Exception("Unhandled torch tensor type.")

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

      forward_process = subprocess.run([bin_path+"/valgrind", "--quiet", "--tool=derivgrind", "--record="+tempdir.name, libexec_path+"/valgrind/derivgrind-library-caller-"+arch+"_linux", library, functionname, fptype, str(len(params)), str(len(input)), str(noutput), tempdir.name])
      
      with open(tempdir.name+"/dg-libcaller-outputs",'rb') as output_buf:
        output = torch.tensor(np.fromfile(output_buf, dtype=input.numpy().dtype, count=noutput))
      with open(tempdir.name+"/dg-tape",'rb') as tape_buf:
        ctx.tape = tape_buf.read()
      with open(tempdir.name+"/dg-input-indices",'rb') as inputindices_buf:
        ctx.inputindices = inputindices_buf.read()
      with open(tempdir.name+"/dg-output-indices",'rb') as outputindices_buf:
        ctx.outputindices = outputindices_buf.read()
      ctx.ninput = len(input)

      return output

    @staticmethod
    def backward(ctx, grad_output):
      tempdir = tempfile.TemporaryDirectory()
#      os.mkfifo(tempdir.name+"/dg-tape")
#      os.mkfifo(tempdir.name+"/dg-input-indices")
#      os.mkfifo(tempdir.name+"/dg-output-indices")
#      os.mkfifo(tempdir.name+"/dg-input-bars")
#      os.mkfifo(tempdir.name+"/dg-output-bars")
      with open(tempdir.name+"/dg-tape","wb") as tape_buf:
        tape_buf.write(ctx.tape)
      with open(tempdir.name+"/dg-input-indices","wb") as inputindices_buf:
        inputindices_buf.write(ctx.inputindices)
      with open(tempdir.name+"/dg-output-indices","wb") as outputindices_buf:
        outputindices_buf.write(ctx.outputindices)
      
      with open(tempdir.name+"/dg-output-bars","w") as outputbars_buf:
        outputbars_buf.writelines([str(float(bar))+"\n" for bar in grad_output])

      backward_process = subprocess.run([bin_path+"/tape-evaluation", tempdir.name])

      grad_input = torch.empty(ctx.ninput,dtype=grad_output.dtype)
      with open(tempdir.name+"/dg-input-bars","r") as inputbars_buf:
        for i in range(ctx.ninput):
          grad_input[i] = float( inputbars_buf.readline().strip() )
        
      return (None,grad_input,None)
  
  return DerivgrindLibraryCaller

bin_path = os.path.dirname(__file__)+"/../../../install/bin"
libexec_path = os.path.dirname(__file__)+"/../../../install/libexec"
