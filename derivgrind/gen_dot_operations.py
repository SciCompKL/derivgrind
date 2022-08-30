"""Create a list of C "case" statements handling VEX IR operations.
"""

# Among the large set of VEX operations, there are always subset of operations 
# that can be treated similarly, but these sets differ between tools.
# I hope that this script makes all the necessary distinctions and is as
# short as possible.

class IROp_Info:
  """Represents one Iop_...."""
  def __init__(self,name,nargs,diffinputs,fpsize=None,simdsize=None,llo=None):
    """Constructor.

      @param name - C identifier of the IROp, e.g. Iop_Add64Fx2
      @param nargs - Number of operands
      @param diffinputs - Indices of operands whose derivative is required, as a list.
      @param fpsize - Size of a component in bytes, 4 or 8.
      @param simdsize - Number of components, 1, 2, 4 or 8.
      @param llo - Is this a lowest-lane-only operation?
    """
    self.name = name
    self.nargs = nargs
    self.diffinputs = diffinputs
    self.fpsize = fpsize
    self.simdsize = simdsize
    self.llo = llo
    # C code computing dotvalue from arg1,arg2,.. and d1,d2,.., must be set after construction.
    self.dotcode = "" 
      

  def make_printcode(self,fpsize,simdsize,llo):
    if fpsize==4:
      return f"IRExpr* value = {irop_info.apply()};" + applyComponentwisely({"value":"value_part"}, {}, fpsize, simdsize, "dg_add_print_stmt(1,diffenv->sb_out,IRExpr_Unop(Iop_ReinterpI32asF32, IRExpr_Unop(Iop_64to32, value_part)));")
    else:
      return f"IRExpr* value = {irop_info.apply()};" + applyComponentwisely({"value":"value_part"}, {}, fpsize, simdsize, "dg_add_print_stmt(1,diffenv->sb_out,IRExpr_Unop(Iop_ReinterpI64asF64, value_part));")

  def makeCase(self, print_results=False):
    """Return the "case Iop_..: ..." statement for dg_dot_operands.c.
      @param print_results - If True, add output statements that print the value and dotvalue.
    """
    s = f"\ncase {self.name}: {{\n"
    # check that diffinputs can be differentiated
    for i in self.diffinputs:
      s += f"if(!d{i}) return NULL; "
    # compute dot value
    s += self.dotcode
    # add print statement if required
    if print_results and self.fpsize and self.simdsize:
      s += f"IRExpr* value = {self.apply()};" 
      if self.fpsize==4:
        s += applyComponentwisely({"value":"value_part"}, {}, self.fpsize, self.simdsize, "dg_add_print_stmt(1,diffenv->sb_out,IRExpr_Unop(Iop_ReinterpI32asF32, IRExpr_Unop(Iop_64to32, value_part)));")
      else:
        s += applyComponentwisely({"value":"value_part"}, {}, self.fpsize, self.simdsize, "dg_add_print_stmt(1,diffenv->sb_out,IRExpr_Unop(Iop_ReinterpI64asF64, value_part));")
    s += f"return dotvalue; \n}}"
    return s
  def apply(self,*operands):
    """Produce C code that applies the operation.
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
    s += f"IRExpr* {outvar} = assembleSIMDVector({outputs[outvar]}_arr, {simdsize}, diffenv);"
  return s
      


### Collect IR operations in this list. ###
IROp_Infos = []


dv = lambda expr: f"IRExpr* dotvalue = {expr};" # assign expression to dotvalue

### Basic scalar, SIMD, and lowest-lane-only SIMD arithmetic. ###

for suffix,fpsize,simdsize,llo in [pF64, pF32, p64Fx2, p64Fx4, p32Fx4, p32Fx8, p32F0x4, p64F0x2]:

  # lowest-lane-only operations have no rounding mode, so it's e.g. IRExpr_Binop(Iop_Add32F0x2,d1,d2) instead of IRExpr_Triop(Iop_Add32Fx2,arg1,d2,d3)
  if llo:
    arg1 = None; arg2 = "arg1"; arg3 = "arg2"; d2 = "d1"; d3 = "d2";
  else:
    arg1 = "arg1"; arg2 = "arg2"; arg3 = "arg3"; d2 = "d2"; d3 = "d3";
  add = IROp_Info(f"Iop_Add{suffix}",3-llo,[2-llo,3-llo],fpsize,simdsize,llo)
  sub = IROp_Info(f"Iop_Sub{suffix}",3-llo,[2-llo,3-llo],fpsize,simdsize,llo)
  mul = IROp_Info(f"Iop_Mul{suffix}",3-llo,[2-llo,3-llo],fpsize,simdsize,llo)
  div = IROp_Info(f"Iop_Div{suffix}",3-llo,[2-llo,3-llo],fpsize,simdsize,llo)
  sqrt = IROp_Info(f"Iop_Sqrt{suffix}",2-llo,[2-llo],fpsize,simdsize,llo)

  add.dotcode = dv(add.apply(arg1,d2,d3))
  sub.dotcode = dv(sub.apply(arg1,d2,d3))
  mul.dotcode = dv(add.apply(arg1,mul.apply(arg1,d2,arg3),mul.apply(arg1,d3,arg2)))
  div.dotcode = dv(div.apply(arg1,sub.apply(arg1,mul.apply(arg1,d2,arg3),mul.apply(arg1,arg2,d3)),mul.apply(arg1,arg3,arg3)))
  sqrt.dotcode = dv(div.apply(arg1,d2,mul.apply(arg1,f"mkIRConst_fptwo({fpsize},{simdsize})",sqrt.apply(arg1,arg2))))

  IROp_Infos += [add,sub,mul,div,sqrt]

# Neg - different set of SIMD setups, no rounding mode
for suffix,fpsize,simdsize in [("F64",8,1),("F32",4,1),("64Fx2",8,2),("32Fx2",4,2),("32Fx4",4,4)]:
  neg = IROp_Info(f"Iop_Neg{suffix}", 1,[1],fpsize,simdsize,False)
  neg.dotcode = dv(neg.apply("d1"))
  IROp_Infos += [ neg ]
# Abs 
for suffix,fpsize,simdsize,llo in [pF64,pF32]: # p64Fx2, p32Fx2, p32Fx4 exist, but AD logic is different
  abs_ = IROp_Info(f"Iop_Abs{suffix}", 1, [1], fpsize, simdsize, False)
  abs_.dotcode = dv(f"IRExpr_ITE(IRExpr_Unop(Iop_32to1,IRExpr_Binop(Iop_Cmp{suffix}, arg1, IRExpr_Const(IRConst_{suffix}i(0)))), IRExpr_Unop(Iop_Neg{suffix},d1), d1)")
  IROp_Infos += [ abs_ ]
# Min, Max
for Op, op in [("Min", "min"), ("Max", "max")]:
  for suffix,fpsize,simdsize,llo in [p32Fx2,p32Fx4,p32F0x4,p64Fx2,p64F0x2,p32Fx8,p64Fx4]:
    the_op = IROp_Info(f"Iop_{Op}{suffix}", 2, [1,2], fpsize, simdsize, llo)
    the_op.dotcode = applyComponentwisely({"arg1":"arg1_part","d1":"d1_part","arg2":"arg2_part","d2":"d2_part"}, {"dotvalue":"dotvalue_part"}, fpsize, simdsize, f'IRExpr* dotvalue_part = mkIRExprCCall(Ity_I64,0,"dg_arithmetic_{op}{fpsize*8}", &dg_arithmetic_{op}{fpsize*8}, mkIRExprVec_4(arg1_part, d1_part, arg2_part, d2_part));') 
    IROp_Infos += [ the_op ]
# fused multiply-add
for Op in ["Add", "Sub"]:
  for suffix,fpsize,simdsize,llo in [pF64,pF32]:
    the_op = IROp_Info(f"Iop_M{Op}{suffix}", 4, [2,3,4], fpsize,simdsize,llo)
    the_op.dotcode = dv(f"IRExpr_Triop(Iop_{Op}{suffix}, arg1, IRExpr_Triop(Iop_{Op}{suffix},arg1,d2,arg3), IRExpr_Qop(Iop_M{Op}{suffix},arg1,arg2,d3,d4))")
    IROp_Infos += [ the_op ]

# Miscellaneous
scalef64 = IROp_Info("Iop_ScaleF64",  3, [2], 8, 1, False)
scalef64.dotcode = dv(scalef64.apply("arg1","d2","arg3"))
yl2xf64 = IROp_Info("Iop_Yl2xF64", 3, [2,3], 8, 1, False)
yl2xf64.dotcode = dv("IRExpr_Triop(Iop_AddF64,arg1,IRExpr_Triop(Iop_Yl2xF64,arg1,d2,arg3),IRExpr_Triop(Iop_DivF64,arg1,IRExpr_Triop(Iop_MulF64,arg1,arg2,d3),IRExpr_Triop(Iop_MulF64,arg1,IRExpr_Const(IRConst_F64(0.6931471805599453094172321214581)),arg3)))")
yl2xp1f64 = IROp_Info("Iop_Yl2xp1F64", 3, [2,3], 8, 1, False)
yl2xp1f64.dotcode = dv("IRExpr_Triop(Iop_AddF64,arg1,IRExpr_Triop(Iop_Yl2xp1F64,arg1,d2,arg3),IRExpr_Triop(Iop_DivF64,arg1,IRExpr_Triop(Iop_MulF64,arg1,arg2,d3),IRExpr_Triop(Iop_MulF64,arg1,IRExpr_Const(IRConst_F64(0.6931471805599453094172321214581)),IRExpr_Triop(Iop_AddF64, arg1, arg3, IRExpr_Const(IRConst_F64(1.))))))")
IROp_Infos += [ scalef64, yl2xf64, yl2xp1f64 ]

### Logical instructions. ###

for Op, op in [("And","and"), ("Or","or"), ("Xor","xor")]:
  # the dirty calls handling 8-byte blocks also consider the case of 2 x 4 bytes
  for (simdsize,fpsize) in [(1,4),(1,8),(2,8),(4,8)]:
    size = simdsize*fpsize*8
    the_op = IROp_Info(f"Iop_{Op}{'V' if size>=128 else ''}{size}", 2, [1,2], 0,0,False)
    the_op.dotcode = applyComponentwisely({"arg1":"arg1_part","d1":"d1_part","arg2":"arg2_part","d2":"d2_part"}, {"dotvalue":"dotvalue_part"}, fpsize, simdsize, f'IRExpr* dotvalue_part = mkIRExprCCall(Ity_I64,0,"dg_logical_{op}64", &dg_logical_{op}64, mkIRExprVec_4(arg1_part, d1_part, arg2_part, d2_part));') 
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
for op in ops:
  the_op = IROp_Info(f"Iop_{op}",1,[1])
  the_op.dotcode = dv(f"IRExpr_Unop(Iop_{op},d1)")
  IROp_Infos += [the_op]

# Zero-derivative unary operations
for op in ["I32StoF64", "I32UtoF64"]:
  the_op = IROp_Info(f"Iop_{op}",1,[])
  the_op.dotcode = dv("IRExpr_Const(IRConst_F64i(0))")
  IROp_Infos += [the_op]

# Pass-through binary operations
ops = []
ops += [f"Iop_{n}HLto{2*n}" for n in [8,16,32,64]]
ops += ["Iop_64HLtoV128", "Iop_V128HLtoV256"]
ops += ["Iop_SetV128lo32", "Iop_SetV128lo64"]
ops += [f"Iop_Interleave{hilo}{n}x{128//n}" for hilo in ["HI","LO"] for n in [8,16,32,64]]
for op in ops:
  the_op = IROp_Info(op,2,[1,2])
  the_op.dotcode = dv(f"IRExpr_Binop({op},d1,d2)")
  IROp_Infos += [the_op]

# Partial pass-through binary operation
f64tof32 = IROp_Info("Iop_F64toF32",2,[2])
f64tof32.dotcode = dv(f64tof32.apply("arg1","d2"))
IROp_Infos += [f64tof32]

# Zero-derivative binary operations
for op in ["I64StoF64","I64UtoF64","RoundF64toInt"]:
  the_op = IROp_Info(f"Iop_{op}",2,[],8,1,False)
  the_op.dotcode = dv("IRExpr_Const(IRConst_F64i(0))")
  IROp_Infos += [the_op]
for op in ["I64StoF32","I64UtoF32","I32StoF32","I32UtoF32"]:
  the_op = IROp_Info(f"Iop_{op}",2,[],4,1,False)
  the_op.dotcode = dv("IRExpr_Const(IRConst_F32i(0))")
  IROp_Infos += [the_op]

# Pass-through quarternary operations
i64x4tov256 = IROp_Info("Iop_64x4toV256",4,[1,2,3,4])
i64x4tov256.dotcode = dv("IRExpr_Qop(Iop_64x4toV256,d1,d2,d3,d4)")
IROp_Infos += [i64x4tov256]




### Produce code. ###
# Some operations created above do actually not exist in VEX, remove them.
IROp_Missing = ["Iop_Div32Fx2","Iop_Sqrt32Fx2"]
IROp_Infos = [irop_info for irop_info in IROp_Infos if irop_info.name not in IROp_Missing]

for irop_info in IROp_Infos:
  print(irop_info.makeCase(False))


