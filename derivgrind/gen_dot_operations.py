"""Create a list of C "case" statements handling VEX IR operations.
"""

# Among the large set of VEX operations, there are always set of operations 
# that can be treated similarly, but these sets differ between tools.
# I hope that this script makes all the necessary distinctions and is as
# short as possible.

class IROp_Info:
  """Represents one Iop_...."""
  def __init__(self,name,diff,nargs,diffinputs,fpsize,simdsize,llo):
    self.name = name # e.g. Iop_Add64Fx2
    self.diff = diff # e.g. IRExpr_Triop(Iop_Add64Fx2, arg1, d2, d3) (C code assembling VEX for the derivative)
    self.nargs = nargs # e.g. 3 (number of operands)
    self.diffinputs = diffinputs # e.g. [2,3] (inputs whose derivative is needed)
    self.fpsize = fpsize # e.g. 8 (scalar size in bytes)
    self.simdsize = simdsize # e.g. 2 (number of scalar components)
    self.llo = llo # True if lowest-lane-only SIMD operation
    self.diff_pre = "" # Stuff in front of the return diff statement
  def makeCase(self):
    """Return the "case Iop_..: ..." statement."""
    s = f"case {self.name}: {{"
    for i in self.diffinputs:
      s += f"if(!d{i}) return NULL; "
    s += self.diff_pre
    s += f"return {self.diff}; }}"
    return s
  def apply(self,*operands):
    """Produce C code that applied the operation.
      @param operands - List of strings containing C code producing the operand expressions. "None" elements are discarded. If empty, apply to arg1, arg2, ...
    """
    operands = [op for op in operands if op]
    if len(operands)==0:
      operands = ["arg1", "arg2", "arg3", "arg4"][0:self.nargs]
    assert( len(operands)==self.nargs)
    if self.nargs==1:
      s = "IRExpr_Unop("
    elif self.nargs==2:
      s = "IRExpr_Binop("
    elif self.nargs==3:
      s = "IRExpr_Triop("
    elif self.nargs==4:
      s = "IRExpr_Qop("
    s += self.name
    for op in operands:
      s += ", "+op
    s += ")"
    return s

def makeprint(irop_info, fpsize, simdsize, llo):
  if fpsize==4:
    return f"IRExpr* value = {irop_info.apply()};" + applyComponentwisely({"value":"value_part"}, {}, fpsize, simdsize, "dg_add_print_stmt(1,diffenv->sb_out,IRExpr_Unop(Iop_ReinterpI32asF32, IRExpr_Unop(Iop_64to32, value_part)));")
  else:
    return f"IRExpr* value = {irop_info.apply()};" + applyComponentwisely({"value":"value_part"}, {}, fpsize, simdsize, "dg_add_print_stmt(1,diffenv->sb_out,IRExpr_Unop(Iop_ReinterpI64asF64, value_part));")


### Handling of floating-point arithmetic operations. ###

### SIMD utilities. ###
# To account for SIMD vectors, fpsize is the scalar size in bytes (4 or 8)
# and simdsize is the number of components (1,2,4 or 8).

# (suffix,fpsize,simdsize,lowest_line_only)
pF64 = ("F64",8,1,False)
pF32 = ("F32",4,1,False)
p64Fx2 = ("64Fx2",8,2,False)
p64Fx4 = ("64Fx4",8,4,False)
p32Fx2 = ("32Fx2",4,2,False)
p32Fx4 = ("32Fx4",4,4,False)
p32Fx8 = ("32Fx8",4,8,False)
p32F0x4 = ("32F0x4",4,4,True)
p64F0x2 = ("64F0x2",8,2,True)

def assembleSIMDVector(expressions,fpsize):
  """Produce C code for an expression that contains all the components.

  E.g. IRExpr_Binop(Iop_32HLto64, <expressions[1]>, <expressions[0]>.
  """
  simdsize = len(expressions)
  if fpsize==4:
    if simdsize==1:
      return expressions[0]
    elif simdsize==2:
      return f"IRExpr_Binop(Iop_32HLto64,{expressions[1]},{expressions[0]})"
    elif simdsize==4:
      return f"IRExpr_Binop(Iop_64HLtoV128, IRExpr_Binop(Iop_32HLto64,{expressions[3]},{expressions[2]}), IRExpr_Binop(Iop_32HLto64,{expressions[1]},{expressions[0]}))"
    elif simdsize==8:
      return f"IRExpr_Qop(Iop_64x4toV256, IRExpr_Binop(Iop_32HLto64,{expressions[7]},{expressions[6]}), IRExpr_Binop(Iop_32HLto64,{expressions[5]},{expressions[4]}), IRExpr_Binop(Iop_32HLto64,{expressions[3]},{expressions[2]}), IRExpr_Binop(Iop_32HLto64,{expressions[1]},{expressions[0]}) )"
  elif fpsize==8:
    if simdsize==1:
      return expressions[0]
    elif simdsize==2:
      return f"IRExpr_Binop(Iop_64HLtoV128,{expressions[1]},{expressions[0]})"
    elif simdsize==4:
      return f"IRExpr_Qop(Iop_64x4toV256, {expressions[3]}, {expressions[2]}, {expressions[1]}, {expressions[0]})"
  assert(False)

def applyComponentwisely(inputs,outputs,fpsize,simdsize,bodyLowest,bodyNonLowest=None):
  """
    Apply C code for all components of a SIMD expression individually, 
    and assemble the output from the individual results.

    E.g. 
      // obtain components [0] of inputs
      <bodyLowest>
      // remember outputs as the total output contains them as components [0]
      // obtain components [1] of inputs
      <bodyNonLowest>
      // remember outputs as the total output contains them as components [1]
      // ... (use bodyNonLowest)
      // assemble total outputs from remembered partial outputs

    @param inputs - Dictionary: variable name of an input IRExpr* => variable name of portion as used by body
    @param outputs - Dictionary: variable name of an output IRExpr* => variable name of portion as used by body
    @param fpsize - Size of component, either 4 or 8 (bytes)
    @param simdsize - Number of components
    @param bodyLowest - C code applied to lowest-lane component
    @param bodyNonLowest - C code applied to non-lowest-lane components
    @returns C code applying body to all components.
  """
  if not bodyNonLowest:
    bodyNonLowest = bodyLowest
  s = ""
  for outvar in outputs:
    s += f"IRExpr* {outputs[outvar]}_arr[{simdsize}]; "
  for component in range(simdsize): # for each component
    s += "{"
    for invar in inputs: # extract component
      s += f"IRExpr* {inputs[invar]} = getSIMDComponent({invar},{fpsize},{simdsize},{component},diffenv); "
      if fpsize==4: # widen to 64 bit
        s += f"{inputs[invar]} = IRExpr_Binop(Iop_32HLto64,IRExpr_Const(IRConst_U32(0)),{inputs[invar]}); "
    s += bodyLowest if component==0 else bodyNonLowest
    for outvar in outputs:
      if fpsize==4: # extract the 32 bit
        s += f"{outputs[outvar]} = IRExpr_Unop(Iop_64to32,{outputs[outvar]});"
      s += f"{outputs[outvar]}_arr[{component}] = {outputs[outvar]};" 
    s += "}"
  for outvar in outputs:
    s += f"IRExpr* {outvar} = " + assembleSIMDVector([f"{outputs[outvar]}_arr[{i}]" for i in range(simdsize)], fpsize) + ";"
  return s
      


### Collect IR operations in this list. ###
IROp_Infos = []


### Basic scalar, SIMD, and lowest-lane-only SIMD arithmetic. ###

for suffix,fpsize,simdsize,llo in [pF64, pF32, p64Fx2, p64Fx4, p32Fx4, p32Fx8, p32F0x4, p64F0x2]:

  # lowest-lane-only operations have no rounding mode, so it's e.g. IRExpr_Binop(Iop_Add32F0x2,d1,d2) instead of IRExpr_Triop(Iop_Add32Fx2,arg1,d2,d3)
  if llo:
    arg1 = None; arg2 = "arg1"; arg3 = "arg2"; d2 = "d1"; d3 = "d2";
  else:
    arg1 = "arg1"; arg2 = "arg2"; arg3 = "arg3"; d2 = "d2"; d3 = "d3";
  add = IROp_Info(f"Iop_Add{suffix}",None,3-llo,[2-llo,3-llo],fpsize,simdsize,llo)
  sub = IROp_Info(f"Iop_Sub{suffix}",None,3-llo,[2-llo,3-llo],fpsize,simdsize,llo)
  mul = IROp_Info(f"Iop_Mul{suffix}",None,3-llo,[2-llo,3-llo],fpsize,simdsize,llo)
  div = IROp_Info(f"Iop_Div{suffix}",None,3-llo,[2-llo,3-llo],fpsize,simdsize,llo)
  sqrt = IROp_Info(f"Iop_Sqrt{suffix}",None,2-llo,[2-llo],fpsize,simdsize,llo)

  add.diff = add.apply(arg1,d2,d3)
  sub.diff = sub.apply(arg1,d2,d3)
  mul.diff = add.apply(arg1,mul.apply(arg1,d2,arg3),mul.apply(arg1,d3,arg2))
  div.diff = div.apply(arg1,sub.apply(arg1,mul.apply(arg1,d2,arg3),mul.apply(arg1,arg2,d3)),mul.apply(arg1,arg3,arg3))
  sqrt.diff = div.apply(arg1,d2,mul.apply(arg1,f"mkIRConst_fptwo({fpsize},{simdsize})",sqrt.apply(arg1,arg2)))

  add.diff_pre += makeprint(add,fpsize,simdsize,llo)
  sub.diff_pre += makeprint(sub,fpsize,simdsize,llo)
  mul.diff_pre += makeprint(mul,fpsize,simdsize,llo)
  div.diff_pre += makeprint(div,fpsize,simdsize,llo)
  sqrt.diff_pre += makeprint(sqrt,fpsize,simdsize,llo)

  IROp_Infos += [add,sub,mul,div,sqrt]
# Neg - different set of SIMD setups, no rounding mode
for suffix,fpsize,simdsize in [("F64",8,1),("F32",4,1),("64Fx2",8,2),("32Fx2",4,2),("32Fx4",4,4)]:
  neg = IROp_Info(f"Iop_Neg{suffix}", None,1,[1],fpsize,simdsize,False)
  neg.diff = neg.apply("d1")
  IROp_Infos += [ neg ]
# Abs 
for suffix,fpsize,simdsize,llo in [pF64,pF32]: # p64Fx2, p32Fx2, p32Fx4 exist, but AD logic is different
  abs_ = IROp_Info(f"Iop_Abs{suffix}", None, 1, [1], fpsize, simdsize, False)
  abs_.diff = f"IRExpr_ITE(IRExpr_Unop(Iop_32to1,IRExpr_Binop(Iop_Cmp{suffix}, arg1, IRExpr_Const(IRConst_{suffix}i(0)))), IRExpr_Unop(Iop_Neg{suffix},d1), d1)"
  IROp_Infos += [ abs_ ]
# Min, Max
for Op, op in [("Min", "min"), ("Max", "max")]:
  for suffix,fpsize,simdsize,llo in [p32Fx2,p32Fx4,p32F0x4,p64Fx2,p64F0x2,p32Fx8,p64Fx4]:
    the_op = IROp_Info(f"Iop_{Op}{suffix}", "result", 2, [1,2], fpsize, simdsize, llo)
    the_op.diff_pre = applyComponentwisely({"arg1":"arg1_part","d1":"d1_part","arg2":"arg2_part","d2":"d2_part"}, {"result":"result_part"}, fpsize, simdsize, f'IRExpr* result_part = mkIRExprCCall(Ity_I64,0,"dg_arithmetic_{op}{fpsize*8}", &dg_arithmetic_{op}{fpsize*8}, mkIRExprVec_4(arg1_part, d1_part, arg2_part, d2_part));')
    IROp_Infos += [ the_op ]
# fused multiply-add
for Op in ["Add", "Sub"]:
  for suffix in ["F64", "F32"]:
    IROp_Infos += [ IROp_Info(f"Iop_M{Op}{suffix}", f"IRExpr_Triop(Iop_{Op}{suffix}, arg1, IRExpr_Triop(Iop_{Op}{suffix},arg1,d2,arg3), IRExpr_Qop(Iop_M{Op}{suffix},arg1,arg2,d3,d4))", 4, [2,3,4],0,0,False) ]

# Miscellaneous
IROp_Infos += [
  IROp_Info("Iop_ScaleF64", "IRExpr_Triop(Iop_ScaleF64,arg1,d2,arg3)", 3, [2], 8, 1, False),
  IROp_Info("Iop_Yl2xF64", "IRExpr_Triop(Iop_AddF64,arg1,IRExpr_Triop(Iop_Yl2xF64,arg1,d2,arg3),IRExpr_Triop(Iop_DivF64,arg1,IRExpr_Triop(Iop_MulF64,arg1,arg2,d3),IRExpr_Triop(Iop_MulF64,arg1,IRExpr_Const(IRConst_F64(0.6931471805599453094172321214581)),arg3)))", 3, [2,3], 8, 1, False),
  IROp_Info("Iop_Yl2xp1F64", "IRExpr_Triop(Iop_AddF64,arg1,IRExpr_Triop(Iop_Yl2xp1F64,arg1,d2,arg3),IRExpr_Triop(Iop_DivF64,arg1,IRExpr_Triop(Iop_MulF64,arg1,arg2,d3),IRExpr_Triop(Iop_MulF64,arg1,IRExpr_Const(IRConst_F64(0.6931471805599453094172321214581)),IRExpr_Triop(Iop_AddF64, arg1, arg3, IRExpr_Const(IRConst_F64(1.))))))", [3], [2,3], 8, 1, False),
]

  



### Logical instructions. ###

for Op, op in [("And","and"), ("Or","or"), ("Xor","xor")]:
  # the dirty calls handling 8-byte blocks also consider the case of 2 x 4 bytes
  for (simdsize,fpsize) in [(1,4),(1,8),(2,8),(4,8)]:
    size = simdsize*fpsize*8
    the_op = IROp_Info(f"Iop_{Op}{'V' if size>=128 else ''}{size}", "result", 2, [1,2], 0,0,False)
    the_op.diff_pre = applyComponentwisely({"arg1":"arg1_part","d1":"d1_part","arg2":"arg2_part","d2":"d2_part"}, {"result":"result_part"}, fpsize, simdsize, f'IRExpr* result_part = mkIRExprCCall(Ity_I64,0,"dg_logical_{op}64", &dg_logical_{op}64, mkIRExprVec_4(arg1_part, d1_part, arg2_part, d2_part));') 
    IROp_Infos += [ the_op ]

# Pass-through unary operations
ops = ["F32toF64"]
for i in ["32","64"]:
  ops += [f"ReinterpI{i}asF{i}",f"ReinterpF{i}asI{i}"]
for j in ["","HI"]:
  ops += [f"16{j}to8", f"32{j}to16", f"64{j}to32", f"V128{j}to64", f"128{j}to64"]
ops += ["64to8","32to8","64to16","V256toV128_0","V256toV128_1","64UtoV128","32UtoV128","V128to32"]
ops += ["ZeroHI64ofV128", "ZeroHI96ofV128", "ZeroHI112ofV128", "ZeroHI120ofV128"]
for j1 in [8,16,32,64]:
  for j2 in [8,16,32,64]:
    if j1<j2:
      ops += [f"{j1}Uto{j2}", f"{j1}Sto{j2}"]
IROp_Infos += [IROp_Info(f"Iop_{op}", f"IRExpr_Unop(Iop_{op},d1)",1,[1],0,0,False) for op in ops]
# Zero-derivative unary operations
IROp_Infos += [IROp_Info(f"Iop_I32{su}toF64", "IRExpr_Const(IRConst_F64i(0))", 1, [], 0,0,False) for su in ["S","U"]]

IROp_Infos += [IROp_Info("Iop_F64toF32","IRExpr_Binop(Iop_F64toF32,arg1,d2)",2,[2],0,0,False)]
# Pass-through binary operations
ops = []
ops += [f"Iop_{n}HLto{2*n}" for n in [8,16,32,64]]
ops += ["Iop_64HLtoV128", "Iop_V128HLtoV256"]
ops += ["Iop_SetV128lo32", "Iop_SetV128lo64"]
ops += [f"Iop_Interleave{hilo}{n}x{128//n}" for hilo in ["HI","LO"] for n in [8,16,32,64]]
IROp_Infos += [IROp_Info(op, f"IRExpr_Binop({op},d1,d2)",2,[1,2],0,0,False) for op in ops]
# Zero-derivative binary operations
for op in ["I64StoF64","I64UtoF64","RoundF64toInt"]:
  IROp_Infos += [IROp_Info(f"Iop_{op}","IRExpr_Const(IRConst_F64i(0))",2,[],8,1,False)]
for op in ["I64StoF32","I64UtoF32","I32StoF32","I32UtoF32"]:
  IROp_Infos += [IROp_Info(f"Iop_{op}","IRExpr_Const(IRConst_F32i(0))",2,[],4,1,False)]

# Pass-through quarternary operations
IROp_Infos += [IROp_Info("Iop_64x4toV256","IRExpr_Qop(Iop_64x4toV256,d1,d2,d3,d4)",4,[1,2,3,4],0,0,0)]




### Produce code. ###
# Some operations created above do actually not exist in VEX, remove them.
IROp_Missing = ["Iop_Div32Fx2","Iop_Sqrt32Fx2"]
IROp_Infos = [irop_info for irop_info in IROp_Infos if irop_info.name not in IROp_Missing]

for irop_info in IROp_Infos:
  print(irop_info.makeCase())


