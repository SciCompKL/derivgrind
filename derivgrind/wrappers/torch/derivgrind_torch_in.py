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

      forward_process = subprocess.run([bin_path+"/valgrind", "--quiet", "--tool=derivgrind", "--record="+tempdir.name, libexec_path+"/valgrind/derivgrind-library-caller-"+arch, library, functionname, fptype, str(len(params)), str(len(input)), str(noutput), tempdir.name])
      
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
#      os.mkfifo(tempdir.name+"/dg-input-adjoints")
#      os.mkfifo(tempdir.name+"/dg-output-adjoints")
      with open(tempdir.name+"/dg-tape","wb") as tape_buf:
        tape_buf.write(ctx.tape)
      with open(tempdir.name+"/dg-input-indices","wb") as inputindices_buf:
        inputindices_buf.write(ctx.inputindices)
      with open(tempdir.name+"/dg-output-indices","wb") as outputindices_buf:
        outputindices_buf.write(ctx.outputindices)
      
      with open(tempdir.name+"/dg-output-adjoints","w") as outputadjoints_buf:
        outputadjoints_buf.writelines([str(float(adj))+"\n" for adj in grad_output])

      backward_process = subprocess.run([bin_path+"/tape-evaluation", tempdir.name])

      grad_input = torch.empty(ctx.ninput,dtype=grad_output.dtype)
      with open(tempdir.name+"/dg-input-adjoints","r") as inputadjoints_buf:
        for i in range(ctx.ninput):
          grad_input[i] = float( inputadjoints_buf.readline().strip() )
        
      return (None,grad_input,None)
  
  return DerivgrindLibraryCaller

bin_path = os.path.dirname(__file__)+"/../../../install/bin"
libexec_path = os.path.dirname(__file__)+"/../../../install/libexec"
