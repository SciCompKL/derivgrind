

def makeTwo(fpsize,simdsize):
  if fpsize==4:
    if simdsize==1:
      return f"IRExpr_Const(IRConst_F32(2.))"
    elif simdsize==2:
      two = f"IRExpr_Unop(Iop_ReinterpF32asI32, {makeTwo(4,1)})"
      return f"IRExpr_Binop(Iop_32HLto64, {two}, {two})"
    elif simdsize==4:
      two = makeTwo(4,2)
      return f"IRExpr_Binop(Iop_64HLtoV128, {two}, {two})"
    elif simdsize==8:
      two = makeTwo(4,2)
      return f"IRExpr_Qop(Iop_64x4toV256, {two}, {two}, {two}, {two})"
  else:
    if simdsize==1:
      return f"IRExpr_Const(IRConst_F64(2.))"
    elif simdsize==2:
      two = f"IRExpr_Unop(Iop_ReinterpF64asI64, {makeTwo(8,1)})"
      return f"IRExpr_Binop(Iop_64HLtoV128, {two}, {two})"
    elif simdsize==4:
      two = f"IRExpr_Unop(Iop_ReinterpF64asI64, {makeTwo(8,1)})"
      return f"IRExpr_Qop(Iop_64x4toV256, {two}, {two}, {two}, {two})"
    


class IROp_Info:
  def __init__(self,name,diff,nargs,diffinputs,fpsize,simdsize,llo):
    self.name = name # e.g. Iop_Add64Fx2
    self.diff = diff # e.g. IRExpr_Triop(Iop_Add64Fx2, arg1, d2, d3) (C code assembling VEX for the derivative)
    self.nargs = nargs # e.g. 3 (number of operands)
    self.diffinputs = diffinputs # e.g. [2,3] (inputs whose derivative is needed)
    self.fpsize = fpsize # e.g. 8 (scalar size in bytes)
    self.simdsize = simdsize # e.g. 2 (number of scalar components)
    self.llo = llo # True if lowest-lane-only SIMD operation
  def makeCase(self):
    s = f"case {self.name}: {{"
    for i in self.diffinputs:
      s += f"if(!d{i}) return NULL; "
    s += f"return {self.diff}; }}"
    return s
  def apply(self,*operands):
    operands = [op for op in operands if op]
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



# Collect IR operations
IROp_Infos = []
# Basic scalar, SIMD, and lowest-lane-only SIMD arithmetic
for suffix,fpsize,simdsize,llo in [("F64",8,1,False),("F32",4,1,False),("64Fx2",8,2,False),("64Fx4",8,4,False),("32Fx4",4,4,False),("32Fx8",4,8,False),("32F0x4",4,4,True),("64F0x2",8,2,True)]:

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
  sqrt.diff = div.apply(arg1,d2,mul.apply(arg1,makeTwo(fpsize,simdsize),sqrt.apply(arg1,arg2)))

  IROp_Infos += [add,sub,mul,div,sqrt]
# Abs
for suffix,fpsize in [("F64",8),("F32",4)]:
  IROp_Infos += [ IROp_Info(f"Iop_Abs{suffix}", f"IRExpr_ITE(IRExpr_Unop(Iop_32to1,IRExpr_Binop(Iop_Cmp{suffix}, arg1, IRExpr_Const(IRConst_{suffix}(0.)))), IRExpr_Unop(Iop_Neg{suffix},d1), d1)", 1, [1], fpsize, 1, False) ]
# Min

# Pass-through unary operations
ops = ["F32toF64"]
for i in ["32","64"]:
  ops += [f"ReinterpI{i}asF{i}",f"ReinterpF{i}asI{i}",f"NegF{i}"]
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
IROp_Infos += [IROp_Info(f"Iop_I32{su}toF64", "IRExpr_Const(IRConst_F64(0.))", 1, [], 0,0,False) for su in ["S","U"]]

# Pass-through binary operations
ops = []
ops += [f"Iop_{n}HLto{2*n}" for n in [8,16,32,64]]
ops += ["Iop_64HLtoV128", "Iop_V128HLtoV256"]
ops += ["Iop_SetV128lo32", "Iop_SetV128lo64"]
ops += [f"Iop_Interleave{hilo}{n}x{128//n}" for hilo in ["HI","LO"] for n in [8,16,32,64]]
IROp_Infos += [IROp_Info(op, f"IRExpr_Binop({op},d1,d2)",2,[1,2],0,0,False) for op in ops]
# Zero-derivative binary operations
for op in ["I64StoF64","I64UtoF64","RoundF64toInt"]:
  IROp_Infos += [IROp_Info(f"Iop_{op}","IRExpr_Const(IRConst_F64(0.))",2,[],8,1,False)]
for op in ["I64StoF32","I64UtoF32","I32StoF32","I32UtoF32"]:
  IROp_Infos += [IROp_Info(f"Iop_{op}","IRExpr_Const(IRConst_F32(0.))",2,[],4,1,False)]

# Pass-through quarternary operations
IROp_Infos += [IROp_Info("Iop_64x4toV256","IRExpr_Qop(Iop_64x4toV256,d1,d2,d3,d4)",4,[1,2,3,4],0,0,0)]






IROp_Missing = ["Iop_Div32Fx2","Iop_Sqrt32Fx2","Iop_Abs64F0x2","Iop_Abs32F0x4","Iop_Abs32Fx8","Iop_Abs64Fx4"]
IROp_Infos = [irop_info for irop_info in IROp_Infos if irop_info.name not in IROp_Missing]

for irop_info in IROp_Infos:
  print(irop_info.makeCase())


