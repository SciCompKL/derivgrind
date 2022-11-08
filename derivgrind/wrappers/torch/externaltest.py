import torch
import struct

class Myfun(torch.autograd.Function):
  @staticmethod
  def forward(ctx, input, noutput, params=b""):
    binary_input = struct.pack("Q", len(params)) + params + struct.pack("Q", len(input)) + b"".join( [struct.pack("", float(input_i))] for input_i in input)] ) + struct.pack("Q", noutput)
    forward_process = subprocess.run(["valgrind", "--tool=derivgrind", "--record=$PWD", "./derivgrind-library-caller", input=binary_input, capture_output=True)
    binary_output = forward_process.stdout
    
    ctx.save_for_backward(x)
    return x**3
  @staticmethod
  def backward(ctx, grad_output):
    x, = ctx.saved_tensors
    return 3*x**2

x = torch.tensor([4.0],requires_grad=True)
y = Myfun.apply(x)
y.backward()
print(x.grad)
