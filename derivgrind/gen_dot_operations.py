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
    self.barcode = ""
    self.disable_print_results = False
      

  def make_printcode(self,fpsize,simdsize,llo):
    if fpsize==4:
      return f"IRExpr* value = {irop_info.apply()};" + applyComponentwisely({"value":"value_part"}, {}, fpsize, simdsize, "dg_add_print_stmt(1,diffenv->sb_out,IRExpr_Unop(Iop_ReinterpI32asF32, IRExpr_Unop(Iop_64to32, value_part)));")
    else:
      return f"IRExpr* value = {irop_info.apply()};" + applyComponentwisely({"value":"value_part"}, {}, fpsize, simdsize, "dg_add_print_stmt(1,diffenv->sb_out,IRExpr_Unop(Iop_ReinterpI64asF64, value_part));")

  def makeCaseDot(self, print_results=False):
    """Return the "case Iop_..: ..." statement for dg_dot_operands.c.
      @param print_results - If True, add output statements that print the value and dotvalue.
    """
    s = f"\ncase {self.name}: {{\n"
    # check that diffinputs can be differentiated
    for i in self.diffinputs:
      s += f"if(!d{i}) return NULL;\n"
    # compute dot value into 'IRExpr* dotvalue'
    s += self.dotcode
    # add print statement if required
    if print_results and self.fpsize and self.simdsize and not self.disable_print_results:
      s += f"IRExpr* value = {self.apply()};\n" 
      if self.fpsize==4:
        s += applyComponentwisely({"value":"value_part","dotvalue":"dotvalue_part"}, {}, self.fpsize, self.simdsize, "dg_add_diffquotdebug(diffenv->sb_out,IRExpr_Unop(Iop_ReinterpI32asF32, IRExpr_Unop(Iop_64to32, value_part)),IRExpr_Unop(Iop_ReinterpI32asF32, IRExpr_Unop(Iop_64to32, dotvalue_part)));")
      else:
        s += applyComponentwisely({"value":"value_part","dotvalue":"dotvalue_part"}, {}, self.fpsize, self.simdsize, "dg_add_diffquotdebug(diffenv->sb_out,IRExpr_Unop(Iop_ReinterpI64asF64, value_part),IRExpr_Unop(Iop_ReinterpI64asF64, dotvalue_part));")
    # post-process
    s += "return dotvalue; \n}"
    return s

  def makeCaseBar(self):
    """Return the "case Iop_..: ..." statement for dg_bar_operands.c.
    """
    if self.barcode=="":
      return ""
    s = f"\ncase {self.name}: {{\n"
    # check that diffinputs can be differentiated
    for i in self.diffinputs:
      s += f"if(!i{i}Lo) return NULL;\n"
      s += f"if(!i{i}Hi) return NULL;\n"
    # compute result index into 'IRExpr *indexLo, *indexHi'
    s += self.barcode
    # post-process
    s += "return mkIRExprVec_2(indexLo, indexHi); \n}"
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
      // split every input vector and obtain its component [0], as I64 expression
      <bodyLowest> // may use component [0] of inputs, produces component [0] of outputs
      // store the component [0] of every output vector
      // split every input vector and obtain its component [1], as I64 expression
      <bodyNonLowest>
      // store the component [1] of every output vector
      // ... (further components, always use bodyNonLowest)
      // assemble total outputs from stored components

    The body can access the input components as I64 expressions. If fpsize==4, its more-significant four
    bytes are filled with zeros. The body may compute the output components of types
    - I64 or F64, if fpsize==8
    - I64 (only less-significant four bytes are relevant), I32 or F32, if fpsize==4.

    @param inputs - Dictionary: variable name of an input IRExpr* => variable name of component as used by body
    @param outputs - Dictionary: variable name of an output IRExpr* => variable name of component as used by body
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
    s += f"IRExpr* {outputs[outvar]}_arr[{simdsize}];\n"
  for component in range(simdsize): # for each component
    s += "{\n"
    for invar in inputs: # extract component
      s += f"  IRExpr* {inputs[invar]} = getSIMDComponent({invar},{fpsize},{simdsize},{component},diffenv);\n"
      if fpsize==4: # widen to 64 bit
        s += f"  {inputs[invar]} = IRExpr_Binop(Iop_32HLto64,IRExpr_Const(IRConst_U32(0)),{inputs[invar]});\n"
    s += bodyLowest if component==0 else bodyNonLowest
    for outvar in outputs:
      if fpsize==4: # extract the 32 bit
        s += f"  {outputs[outvar]} = IRExpr_Unop(Iop_64to32,{outputs[outvar]});\n"
      s += f"  {outputs[outvar]}_arr[{component}] = {outputs[outvar]};\n" 
    s += "}\n"
  for outvar in outputs:
    s += f"IRExpr* {outvar} = assembleSIMDVector({outputs[outvar]}_arr, {fpsize}, {simdsize}, diffenv);\n"
  return s

def createBarCode(op, inputs, partials,fpsize,simdsize,llo):
  """
    Record an operation on the tape for every component of a SIMD vector.
  """
  assert(len(inputs) in [1,2,3])
  input_names = {}
  for i in inputs:
    input_names[f"arg{i}"] = f"arg{i}_part"
    input_names[f"i{i}Lo"] = f"i{i}Lo_part"
    input_names[f"i{i}Hi"] = f"i{i}Hi_part"
  output_names = {"indexIntLo":"indexIntLo_part", "indexIntHi":"indexIntHi_part"}
  if len(inputs)==1: # use index 0 to indicate missing input
    bodyLowest = f'  IRExpr** indexIntHiLo_part = dg_bar_writeToTape(diffenv,i{inputs[0]}Lo_part,i{inputs[0]}Hi_part,IRExpr_Const(IRConst_U64(0)),IRExpr_Const(IRConst_U64(0)), {partials[0]}, IRExpr_Const(IRConst_F64(0.)));\n  IRExpr* indexIntLo_part = indexIntHiLo_part[0];\n  IRExpr* indexIntHi_part = indexIntHiLo_part[1];\n'
  elif len(inputs)==2:
    bodyLowest = f'  IRExpr** indexIntHiLo_part = dg_bar_writeToTape(diffenv,i{inputs[0]}Lo_part,i{inputs[0]}Hi_part,i{inputs[1]}Lo_part,i{inputs[1]}Hi_part, {partials[0]}, {partials[1]});\n  IRExpr* indexIntLo_part = indexIntHiLo_part[0];\n  IRExpr* indexIntHi_part = indexIntHiLo_part[1];\n'
  elif len(inputs)==3: # add two tape entries to combine three inputs
    bodyLowest = f'  IRExpr** indexIntermediateIntHiLo_part = dg_bar_writeToTape(diffenv,i{inputs[0]}Lo_part,i{inputs[0]}Hi_part,i{inputs[1]}Lo_part,i{inputs[1]}Hi_part, {partials[0]}, {partials[1]});\n  IRExpr* indexIntermediateIntLo_part = indexIntermediateIntHiLo_part[0];\n  IRExpr* indexIntermediateIntHi_part = indexIntermediateIntHiLo_part[1];\n'
    bodyLowest += f'  IRExpr** indexIntHiLo_part = dg_bar_writeToTape(diffenv,indexIntermediateIntLo_part,indexIntermediateIntHi_part,i{inputs[2]}Lo_part,i{inputs[2]}Hi_part, IRExpr_Const(IRConst_F64(1.)), {partials[2]});\n  IRExpr* indexIntLo_part = indexIntHiLo_part[0];\n  IRExpr* indexIntHi_part = indexIntHiLo_part[1];\n'
  if llo:
    bodyNonLowest = f'  IRExpr* indexIntLo_part = i{inputs[0]}Lo_part;\n  IRExpr* indexIntHi_part = i{inputs[0]}Hi_part;\n'
  else:
    bodyNonLowest = bodyLowest
  barcode = applyComponentwisely(input_names, output_names, fpsize, simdsize, bodyLowest, bodyNonLowest);
  barcode += f"IRExpr* indexLo = reinterpretType(diffenv,indexIntLo, typeOfIRExpr(diffenv->sb_out->tyenv,{op.apply()}));\n"
  barcode += f"IRExpr* indexHi = reinterpretType(diffenv,indexIntHi, typeOfIRExpr(diffenv->sb_out->tyenv,{op.apply()}));\n"
  return barcode

### Collect IR operations in this list. ###
IROp_Infos = []


dv = lambda expr: f"IRExpr* dotvalue = {expr};\n" # assign expression to dotvalue

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

  if fpsize==4: # argument values are provided as I64, reinterpret them as F32 or F64 and convert to F64
  # this F64 value is then used for the computation of the partial derivatives written to the tape
    arg2_part_f = f"IRExpr_Unop(Iop_F32toF64,IRExpr_Unop(Iop_ReinterpI32asF32,IRExpr_Unop(Iop_64to32,{arg2}_part)))"
    arg3_part_f = f"IRExpr_Unop(Iop_F32toF64,IRExpr_Unop(Iop_ReinterpI32asF32,IRExpr_Unop(Iop_64to32,{arg3}_part)))"
  else:
    arg2_part_f = f"IRExpr_Unop(Iop_ReinterpI64asF64,{arg2}_part)"
    arg3_part_f = f"IRExpr_Unop(Iop_ReinterpI64asF64,{arg3}_part)"
  add.barcode = createBarCode(add, [2-llo,3-llo], ["IRExpr_Const(IRConst_F64(1.))", "IRExpr_Const(IRConst_F64(1.))"], fpsize, simdsize, llo)
  sub.barcode = createBarCode(sub, [2-llo,3-llo], ["IRExpr_Const(IRConst_F64(1.))", "IRExpr_Const(IRConst_F64(-1.))"], fpsize, simdsize, llo)
  mul.barcode = createBarCode(mul, [2-llo,3-llo], [arg3_part_f, arg2_part_f], fpsize, simdsize, llo)
  div.barcode = createBarCode(div, [2-llo,3-llo], [f"IRExpr_Triop(Iop_DivF64,dg_bar_rounding_mode,IRExpr_Const(IRConst_F64(1.)),{arg3_part_f})", f"IRExpr_Triop(Iop_DivF64,dg_bar_rounding_mode,IRExpr_Triop(Iop_MulF64,dg_bar_rounding_mode,IRExpr_Const(IRConst_F64(-1.)), {arg2_part_f}),IRExpr_Triop(Iop_MulF64,dg_bar_rounding_mode,{arg3_part_f},{arg3_part_f}))"], fpsize, simdsize, llo)
  sqrt.barcode = createBarCode(sqrt, [2-llo], [f"IRExpr_Triop(Iop_DivF64,dg_bar_rounding_mode,IRExpr_Const(IRConst_F64(0.5)), IRExpr_Binop(Iop_SqrtF64,dg_bar_rounding_mode,{arg2_part_f}))"], fpsize, simdsize, llo)

  IROp_Infos += [add,sub,mul,div,sqrt]

# Neg - different set of SIMD setups, no rounding mode
for suffix,fpsize,simdsize in [("F64",8,1),("F32",4,1),("64Fx2",8,2),("32Fx2",4,2),("32Fx4",4,4)]:
  neg = IROp_Info(f"Iop_Neg{suffix}", 1,[1],fpsize,simdsize,False)
  neg.dotcode = dv(neg.apply("d1"))
  neg.barcode = createBarCode(neg, [1], ["IRExpr_Const(IRConst_F64(-1.))"], fpsize, simdsize, False)
  IROp_Infos += [ neg ]
# Abs 
for suffix,fpsize,simdsize,llo in [pF64,pF32]: # p64Fx2, p32Fx2, p32Fx4 exist, but AD logic is different
  abs_ = IROp_Info(f"Iop_Abs{suffix}", 1, [1], fpsize, simdsize, False)
  abs_.dotcode = dv(f"IRExpr_ITE(IRExpr_Unop(Iop_32to1,IRExpr_Binop(Iop_Cmp{suffix}, arg1, IRExpr_Const(IRConst_{suffix}i(0)))), IRExpr_Unop(Iop_Neg{suffix},d1), d1)")
  if fpsize==4:  # conversion to F64, see above
    arg1_part_f = "IRExpr_Unop(Iop_F32toF64,IRExpr_Unop(Iop_ReinterpI32asF32,IRExpr_Unop(Iop_64to32,arg1_part)))"
  else:
    arg1_part_f = "IRExpr_Unop(Iop_ReinterpI64asF64,arg1_part)"
  abs_.barcode = createBarCode(abs_, [1], [f"IRExpr_ITE( IRExpr_Unop(Iop_32to1,IRExpr_Binop(Iop_Cmp{suffix}, {arg1_part_f}, IRExpr_Const(IRConst_{suffix}i(0)))) , IRExpr_Const(IRConst_F64(-1.)), IRExpr_Const(IRConst_F64(1.)))"], fpsize, simdsize, llo)
  IROp_Infos += [ abs_ ]
# Min, Max
for Op, op in [("Min", "min"), ("Max", "max")]:
  for suffix,fpsize,simdsize,llo in [p32Fx2,p32Fx4,p32F0x4,p64Fx2,p64F0x2,p32Fx8,p64Fx4]:
    the_op = IROp_Info(f"Iop_{Op}{suffix}", 2, [1,2], fpsize, simdsize, llo)
    the_op.dotcode = applyComponentwisely({"arg1":"arg1_part","d1":"d1_part","arg2":"arg2_part","d2":"d2_part"}, {"dotvalue":"dotvalue_part"}, fpsize, simdsize, f'IRExpr* dotvalue_part = mkIRExprCCall(Ity_I64,0,"dg_dot_arithmetic_{op}{fpsize*8}", &dg_dot_arithmetic_{op}{fpsize*8}, mkIRExprVec_4(arg1_part, d1_part, arg2_part, d2_part));') 
    if fpsize==4:  # conversion to F64, see above
      arg1_part_f = "IRExpr_Unop(Iop_F32toF64,IRExpr_Unop(Iop_ReinterpI32asF32,IRExpr_Unop(Iop_64to32,arg1_part)))"
      arg2_part_f = "IRExpr_Unop(Iop_F32toF64,IRExpr_Unop(Iop_ReinterpI32asF32,IRExpr_Unop(Iop_64to32,arg2_part)))"
    else:
      arg1_part_f = "IRExpr_Unop(Iop_ReinterpI64asF64,arg1_part)"
      arg2_part_f = "IRExpr_Unop(Iop_ReinterpI64asF64,arg2_part)"
    the_op.barcode = createBarCode(the_op, [1,2], [f"IRExpr_ITE(IRExpr_Unop(Iop_32to1,IRExpr_Binop(Iop_Cmp{'F32' if fpsize==4 else 'F64'},{arg1_part_f},{arg2_part_f})),  IRExpr_Const(IRConst_F64({'1.' if op=='min' else '0.'})),  IRExpr_Const(IRConst_F64({'0.' if op=='min' else '1.'})) )",     f"IRExpr_ITE(IRExpr_Unop(Iop_32to1,IRExpr_Binop(Iop_Cmp{'F32' if fpsize==4 else 'F64'},{arg1_part_f},{arg2_part_f})),  IRExpr_Const(IRConst_F64({'0.' if op=='min' else '1.'})),  IRExpr_Const(IRConst_F64({'1.' if op=='min' else '0.'})) )"], fpsize, simdsize,llo)
    IROp_Infos += [ the_op ]
# fused multiply-add
for Op in ["Add", "Sub"]:
  for suffix,fpsize,simdsize,llo in [pF64,pF32]:
    the_op = IROp_Info(f"Iop_M{Op}{suffix}", 4, [2,3,4], fpsize,simdsize,llo)
    the_op.dotcode = dv(f"IRExpr_Triop(Iop_{Op}{suffix}, arg1, IRExpr_Triop(Iop_{Op}{suffix},arg1,d2,arg3), IRExpr_Qop(Iop_M{Op}{suffix},arg1,arg2,d3,d4))")
    if fpsize==4:  # conversion to F64, see above
      arg2_part_f = "IRExpr_Unop(Iop_F32toF64,IRExpr_Unop(Iop_ReinterpI32asF32,IRExpr_Unop(Iop_64to32,arg2_part)))"
      arg3_part_f = "IRExpr_Unop(Iop_F32toF64,IRExpr_Unop(Iop_ReinterpI32asF32,IRExpr_Unop(Iop_64to32,arg3_part)))"
      arg4_part_f = "IRExpr_Unop(Iop_F32toF64,IRExpr_Unop(Iop_ReinterpI32asF32,IRExpr_Unop(Iop_64to32,arg4_part)))"
    else:
      arg2_part_f = "IRExpr_Unop(Iop_ReinterpI64asF64,arg2_part)"
      arg3_part_f = "IRExpr_Unop(Iop_ReinterpI64asF64,arg3_part)"
      arg4_part_f = "IRExpr_Unop(Iop_ReinterpI64asF64,arg4_part)"
    the_op.barcode = createBarCode(the_op, [2,3,4], [arg3_part_f, arg2_part_f, f"IRExpr_Const(IRConst_F64({'1.' if Op=='Add' else '-1.'}))"], fpsize, simdsize,llo)
    IROp_Infos += [ the_op ]

# Miscellaneous
scalef64 = IROp_Info("Iop_ScaleF64",  3, [2], 8, 1, False)
scalef64.dotcode = dv(scalef64.apply("arg1","d2","arg3"))
scalef64.barcode = createBarCode(scalef64, [2], ["IRExpr_Triop(Iop_ScaleF64,arg1,IRExpr_Const(IRConst_F64(1.)),arg3)"], 8, 1, False)
yl2xf64 = IROp_Info("Iop_Yl2xF64", 3, [2,3], 8, 1, False)
yl2xf64.dotcode = dv("IRExpr_Triop(Iop_AddF64,arg1,IRExpr_Triop(Iop_Yl2xF64,arg1,d2,arg3),IRExpr_Triop(Iop_DivF64,arg1,IRExpr_Triop(Iop_MulF64,arg1,arg2,d3),IRExpr_Triop(Iop_MulF64,arg1,IRExpr_Const(IRConst_F64(0.6931471805599453094172321214581)),arg3)))")
yl2xf64.barcode = createBarCode(yl2xf64, [2,3], ["IRExpr_Triop(Iop_Yl2xF64,arg1,IRExpr_Const(IRConst_F64(1.)),arg3)",  "IRExpr_Triop(Iop_DivF64,arg1,arg2,IRExpr_Triop(Iop_MulF64,arg1,IRExpr_Const(IRConst_F64(0.6931471805599453094172321214581)), arg3))"], 8, 1, False)
yl2xp1f64 = IROp_Info("Iop_Yl2xp1F64", 3, [2,3], 8, 1, False)
yl2xp1f64.dotcode = dv("IRExpr_Triop(Iop_AddF64,arg1,IRExpr_Triop(Iop_Yl2xp1F64,arg1,d2,arg3),IRExpr_Triop(Iop_DivF64,arg1,IRExpr_Triop(Iop_MulF64,arg1,arg2,d3),IRExpr_Triop(Iop_MulF64,arg1,IRExpr_Const(IRConst_F64(0.6931471805599453094172321214581)),IRExpr_Triop(Iop_AddF64, arg1, arg3, IRExpr_Const(IRConst_F64(1.))))))")
yl2xp1f64.barcode = createBarCode(yl2xp1f64, [2,3], ["IRExpr_Triop(Iop_Yl2xp1F64,arg1,IRExpr_Const(IRConst_F64(1.)),arg3)",  "IRExpr_Triop(Iop_DivF64,arg1,arg2,IRExpr_Triop(Iop_MulF64,arg1,IRExpr_Const(IRConst_F64(0.6931471805599453094172321214581)),IRExpr_Triop(Iop_AddF64, arg1, arg3, IRExpr_Const(IRConst_F64(1.)))))"], 8, 1, False)
IROp_Infos += [ scalef64, yl2xf64, yl2xp1f64 ]

### Bitwise logical instructions. ###

for Op, op in [("And","and"), ("Or","or"), ("Xor","xor")]:
  # the dirty calls handling 8-byte blocks also consider the case of 2 x 4 bytes
  for (simdsize,fpsize) in [(1,4),(1,8),(2,8),(4,8)]:
    size = simdsize*fpsize*8
    the_op = IROp_Info(f"Iop_{Op}{'V' if size>=128 else ''}{size}", 2, [1,2], 0,0,False)
    the_op.dotcode = applyComponentwisely({"arg1":"arg1_part","d1":"d1_part","arg2":"arg2_part","d2":"d2_part"}, {"dotvalue":"dotvalue_part"}, fpsize, simdsize, f'IRExpr* dotvalue_part = mkIRExprCCall(Ity_I64,0,"dg_dot_bitwise_{op}64", &dg_dot_bitwise_{op}64, mkIRExprVec_4(arg1_part, d1_part, arg2_part, d2_part));') 
    the_op.barcode = applyComponentwisely({"arg1":"arg1_part","i1Lo":"i1Lo_part","i1Hi":"i1Hi_part","arg2":"arg2_part","i2Lo":"i2Lo_part","i2Hi":"i2Hi_part"}, {"indexLo":"indexLo_part","indexHi":"indexHi_part"}, fpsize, simdsize, f'IRTemp t = newIRTemp(diffenv->sb_out->tyenv, Ity_V128);\n  IRDirty* di = unsafeIRDirty_1_N( t, 0, "dg_bar_bitwise_{op}64", &dg_bar_bitwise_{op}64, mkIRExprVec_7(IRExpr_VECRET(), arg1_part, i1Lo_part, i1Hi_part, arg2_part, i2Lo_part, i2Hi_part));  \n addStmtToIRSB(diffenv->sb_out, IRStmt_Dirty(di));\n      IRExpr* indexInt_part = IRExpr_RdTmp(t); \nIRExpr* indexLo_part = IRExpr_Unop(Iop_V128to64, indexInt_part);\nIRExpr* indexHi_part = IRExpr_Unop(Iop_V128HIto64, indexInt_part); ') 
    the_op.disable_print_results = True # because many are not floating-point operations
    IROp_Infos += [ the_op ]

# Unary operations that simply move data and/or set data to zero, apply them analogously to dot value and index.
ops = []
for i in ["32","64"]:
  ops += [f"ReinterpI{i}asF{i}",f"ReinterpF{i}asI{i}"]
for j in ["","HI"]:
  ops += [f"16{j}to8", f"32{j}to16", f"64{j}to32", f"V128{j}to64", f"128{j}to64"]
ops += ["64to8","32to8","64to16","V256toV128_0","V256toV128_1","64UtoV128","32UtoV128","V128to32"]
ops += ["ZeroHI64ofV128", "ZeroHI96ofV128", "ZeroHI112ofV128", "ZeroHI120ofV128"] # the latter two might be misused? TODO
for j1 in [8,16,32,64]:
  for j2 in [8,16,32,64]:
    if j1<j2:
      ops += [f"{j1}Uto{j2}", f"{j1}Sto{j2}"]
for op in ops:
  the_op = IROp_Info(f"Iop_{op}",1,[1])
  the_op.dotcode = dv(f"IRExpr_Unop(Iop_{op},d1)")
  the_op.barcode = f"IRExpr* indexLo = IRExpr_Unop(Iop_{op},i1Lo);\nIRExpr* indexHi = IRExpr_Unop(Iop_{op},i1Hi);"
  IROp_Infos += [the_op]

# Conversion F32 -> F64: Apply analogously to dot value, and add zero bytes to index.
the_op = IROp_Info("Iop_F32toF64",1,[1])
the_op.dotcode = dv("IRExpr_Unop(Iop_F32toF64,d1)")
the_op.barcode = "\n".join([f"IRExpr* index{HiLo} = IRExpr_Unop(Iop_ReinterpI64asF64,IRExpr_Binop(Iop_32HLto64,IRExpr_Const(IRConst_U32(0)),IRExpr_Unop(Iop_ReinterpF32asI32,i1{HiLo})));" for HiLo in ["Lo","Hi"]])
IROp_Infos += [the_op]


# Zero-derivative unary operations
for op in ["I32StoF64", "I32UtoF64"]:
  the_op = IROp_Info(f"Iop_{op}",1,[])
  the_op.dotcode = dv("IRExpr_Const(IRConst_F64i(0))")
  the_op.barcode = "\n".join([f"IRExpr* index{HiLo} = IRExpr_Unop(Iop_ReinterpI64asF64, IRExpr_Const(IRConst_U64(0)));" for HiLo in ["Lo","Hi"]])
  IROp_Infos += [the_op]

# Binary operations that move data, apply analogously to dot values and indices.
ops = []
ops += [f"Iop_{n}HLto{2*n}" for n in [8,16,32,64]]
ops += ["Iop_64HLtoV128", "Iop_V128HLtoV256"]
ops += ["Iop_SetV128lo32", "Iop_SetV128lo64"]
ops += [f"Iop_Interleave{hilo}{n}x{128//n}" for hilo in ["HI","LO"] for n in [8,16,32,64]]
for op in ops:
  the_op = IROp_Info(op,2,[1,2])
  the_op.dotcode = dv(f"IRExpr_Binop({op},d1,d2)")
  the_op.barcode = "\n".join([f"IRExpr* index{HiLo} = IRExpr_Binop({op},i1{HiLo},i2{HiLo});" for HiLo in ["Lo","Hi"]])
  IROp_Infos += [the_op]

# Conversion F64 -> F32: Apply analogously to dot value, and cut bytes from index.
f64tof32 = IROp_Info("Iop_F64toF32",2,[2])
f64tof32.dotcode = dv(f64tof32.apply("arg1","d2"))
f64tof32.barcode = "\n".join([f"IRExpr* index{HiLo} = IRExpr_Unop(Iop_ReinterpI32asF32,IRExpr_Unop(Iop_64to32,IRExpr_Unop(Iop_ReinterpF64asI64,i2{HiLo})));" for HiLo in ["Lo","Hi"]])
IROp_Infos += [f64tof32]

# Zero-derivative binary operations
for op in ["I64StoF64","I64UtoF64","RoundF64toInt"]:
  the_op = IROp_Info(f"Iop_{op}",2,[],8,1,False)
  the_op.dotcode = dv("IRExpr_Const(IRConst_F64i(0))")
  the_op.barcode = "\n".join([f"IRExpr* index{HiLo} = IRExpr_Unop(Iop_ReinterpI64asF64,IRExpr_Const(IRConst_U64(0)));" for HiLo in ["Lo","Hi"]])
  IROp_Infos += [the_op]
for op in ["I64StoF32","I64UtoF32","I32StoF32","I32UtoF32"]:
  the_op = IROp_Info(f"Iop_{op}",2,[],4,1,False)
  the_op.dotcode = dv("IRExpr_Const(IRConst_F32i(0))")
  the_op.barcode = "\n".join([f"IRExpr* index{HiLo} = IRExpr_Unop(Iop_ReinterpI32asF32,IRExpr_Const(IRConst_U32(0)));" for HiLo in ["Lo","Hi"]])
  IROp_Infos += [the_op]

# Quaternary operation that moves data, apply analogously to dot values and indices.
i64x4tov256 = IROp_Info("Iop_64x4toV256",4,[1,2,3,4])
i64x4tov256.dotcode = dv("IRExpr_Qop(Iop_64x4toV256,d1,d2,d3,d4)")
i64x4tov256.barcode = "\n".join([f"IRExpr* index{HiLo} = IRExpr_Qop(Iop_64x4toV256,i1{HiLo},i2{HiLo},i3{HiLo},i4{HiLo});" for HiLo in ["Lo","Hi"]])
IROp_Infos += [i64x4tov256]




### Produce code. ###
# Some operations created above do actually not exist in VEX, remove them.
IROp_Missing = ["Iop_Div32Fx2","Iop_Sqrt32Fx2"]
IROp_Infos = [irop_info for irop_info in IROp_Infos if irop_info.name not in IROp_Missing]

import sys
mode = 'dot'
if(len(sys.argv)>=2):
  mode = sys.argv[1]

for irop_info in IROp_Infos:
  if mode=='dot':
    print(irop_info.makeCaseDot(False))
  elif mode=='dot-dqd':
    print(irop_info.makeCaseDot(True))
  elif mode=='bar':
    print(irop_info.makeCaseBar())
  else:
    print(f"Error: Bad mode '{mode}'.",file=sys.stderr)
    exit(1)


