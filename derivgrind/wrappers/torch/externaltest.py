import torch
import numpy as np
import struct
import tempfile
import os
import subprocess

def derivgrind(library,functionname):
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

      forward_process = subprocess.run(["/home/aehle/valgrind/install/bin/valgrind", "--tool=derivgrind", "--record="+tempdir.name, "/home/aehle/valgrind/derivgrind/wrappers/torch/derivgrind-library-caller", library, functionname, fptype, str(len(params)), str(len(input)), str(noutput), tempdir.name])
      
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

      backward_process = subprocess.run(["/home/aehle/valgrind/install/bin/tape-evaluation", tempdir.name])

      grad_input = torch.empty(ctx.ninput,dtype=grad_output.dtype)
      with open(tempdir.name+"/dg-input-adjoints","r") as inputadjoints_buf:
        for i in range(ctx.ninput):
          grad_input[i] = float( inputadjoints_buf.readline().strip() )
        
      return (None,grad_input,None)
  
  return DerivgrindLibraryCaller

x = torch.tensor([4.0],dtype=torch.float64,requires_grad=True)
y = derivgrind("mylib.so", "myfun").apply(b"",x,1)
y.backward()
print(x.grad)
