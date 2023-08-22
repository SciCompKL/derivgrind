# -------------------------------------------------------------------- #
# --- Generation of the VEX operation handling code.               --- #
# ---                                gen_operationhandling_code.py --- #
# -------------------------------------------------------------------- #
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
#  This program is free software; you can redistribute it and/or
#  modify it under the terms of the GNU General Public License as
#  published by the Free Software Foundation; either version 2 of the
#  License, or (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful, but
#  WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
#  General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, see <http://www.gnu.org/licenses/>.
#
#  The GNU General Public License is contained in the file COPYING.
#


"""Create a list of C "case" statements handling VEX IR operations
to be #include'd into 
- the dg_dot_operation function in dg_dot.c, in forward mode. The code may 
  create CCalls to functions in dg_dot_bitwise.c, dg_dot_minmax.c.
- the dg_bar_operation function in dg_bar.c, in recording mode. The code may
  create dirty calls to functions in dg_bar_bitwise.c.
- the dg_trick_operation function in dg_trick.c, in bit-trick mode. The code may
  create dirty calls to functions in dg_trick_bitwise.c.
"""

# Among the large set of VEX operations, there are always subset of operations 
# that can be treated similarly, but these sets differ between tools.
# I hope that this script makes all the necessary distinctions and is as
# short as possible.

class IROp_Info:
  """Represents one Iop_...."""
  def __init__(self,name,nargs,diffinputs,fpsize=0,simdsize=1,disable_print_results=True):
    """Constructor.

      @param name - C identifier of the IROp, e.g. Iop_Add64Fx2
      @param nargs - Number of operands, 1..4
      @param diffinputs - Indices of operands whose dot value or index is required, as a list
      @param fpsize - Size of a component in bytes, 4 or 8.
      @param simdsize - Number of components, 1, 2, 4 or 8.
      @param disable_print_results - Whether or not to include this operation type into the difference quotient debugging
    """
    self.name = name
    self.nargs = nargs
    self.diffinputs = diffinputs
    # dotcode is C code computing an IRExpr* dotvalue for the dot value of the result.
    self.dotcode = "" 
    # barcode is C code computing IRExpr* indexLo, IRExpr* indexHi for the lower and 
    # higher layer of the index
    self.barcode = ""
    # trickcode is C code computing IRExpr* flagsLo, IRExpr* flagsHi for the lower and
    # higher layer of the flags
    self.trickcode = ""
    # The remaining arguments are only needed for the difference quotient debugging (dqd). 
    self.fpsize = fpsize
    self.simdsize = simdsize
    self.disable_print_results = False

  def makeCaseDot(self, print_results=False):
    """Return the forward mode "case Iop_..: ..." statement for dg_dot_operations.c.
      @param print_results - If True, add output statements that print the value and dotvalue for 
                             difference quotient debugging.
    """
    if self.dotcode=="":
      return ""
    s = f"\ncase {self.name}: {{\n"
    # check that diffinputs can be differentiated
    for i in self.diffinputs:
      s += f"if(!d{i}) return NULL;\n"
    # compute dot value into 'IRExpr* dotvalue'
    s += self.dotcode
    # add print statement if required
    if print_results and self.fpsize!=0 and not self.disable_print_results:
      s += f"IRExpr* value = {self.apply()};\n" 
      if self.fpsize==4:
        s += applyComponentwisely({"value":"value_part","dotvalue":"dotvalue_part"}, {}, self.fpsize, self.simdsize, "dg_add_diffquotdebug(diffenv->sb_out,IRExpr_Unop(Iop_ReinterpI32asF32, IRExpr_Unop(Iop_64to32, value_part)),IRExpr_Unop(Iop_ReinterpI32asF32, IRExpr_Unop(Iop_64to32, dotvalue_part)));")
      else:
        s += applyComponentwisely({"value":"value_part","dotvalue":"dotvalue_part"}, {}, self.fpsize, self.simdsize, "dg_add_diffquotdebug(diffenv->sb_out,IRExpr_Unop(Iop_ReinterpI64asF64, value_part),IRExpr_Unop(Iop_ReinterpI64asF64, dotvalue_part));")
    # and return
    s += "return dotvalue; \n}"
    return s

  def makeCaseBar(self):
    """Return the "case Iop_..: ..." statement for dg_bar_operations.c.
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
    # and return
    s += "return mkIRExprVec_2(indexLo, indexHi); \n}"
    return s

  def makeCaseTrick(self):
    """Return the "case Iop_..: ..." statement for dg_trick_operations.c.
    """
    if self.trickcode=="":
      return ""
    s = f"\ncase {self.name}: {{\n"
    # check that diffinputs can be differentiated
    for i in self.diffinputs:
      s += f"if(!f{i}Lo) return NULL;\n"
      s += f"if(!f{i}Hi) return NULL;\n"
    # compute result flags into 'IRExpr *flagsLo, *flagsHi'
    s += self.trickcode
    # and return
    s += "return mkIRExprVec_2(flagsLo, flagsHi); \n}"
    return s

  def apply(self,*operands):
    """Produce C code that applies the operation to operands.
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

### SIMD utilities. ###
# To account for SIMD vectors, fpsize is the scalar size in bytes (4 or 8)
# and simdsize is the number of scalar components (1,2,4 or 8). 
# suffix is the typical suffix in the names of VEX operations. 
# llo indicates whether the operation is "lowest-lane-only", 
# i.e. performed only on the component 0 while the other
# components of the result are simply copied from the first operand. 

# SIMD layouts: (suffix,fpsize,simdsize,llo)
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

      // split every input vector and obtain its component [0], a as I64 expression
      <bodyLowest> // may use component [0] of inputs, produces component [0] of outputs
      // store the component [0] of every output vector

      // split every input vector and obtain its component [1], as I64 expression
      <bodyNonLowest>
      // store the component [1] of every output vector

      // ... (further components, always use bodyNonLowest)

      // assemble total outputs from stored components

    The body can access the input components as I64 expressions. If fpsize==4, its more-significant four
    bytes are filled with zeros. 
    The body must compute the output components as I64 expressions. If fpsize==4, its more-significant four
    bytes are discarded.

    @param inputs - Dictionary: variable name of an input IRExpr* => variable name of component IRExpr* as used by body
    @param outputs - Dictionary: variable name of an output IRExpr* => variable name of component IRExpr* as used by body
    @param fpsize - Size of component, either 4 or 8 (bytes)
    @param simdsize - Number of components
    @param bodyLowest - C code applied to lowest-lane component
    @param bodyNonLowest - C code applied to non-lowest-lane components
    @returns C code applying body to all components.
  """
  if not bodyNonLowest:
    bodyNonLowest = bodyLowest
  s = ""
  for outvar in outputs: # declare C array of output component IRExpr*'s.
    s += f"IRExpr* {outputs[outvar]}_arr[{simdsize}];\n"
  for component in range(simdsize): # for each component
    s += "{\n"
    for invar in inputs: # extract component
      s += f"  IRExpr* {inputs[invar]} = getSIMDComponent({invar},{fpsize},{simdsize},{component},diffenv);\n"
      if fpsize==4: # widen to 64 bit
        s += f"  {inputs[invar]} = IRExpr_Binop(Iop_32HLto64,IRExpr_Const(IRConst_U32(0)),{inputs[invar]});\n"
    # apply body
    s += bodyLowest if component==0 else bodyNonLowest
    for outvar in outputs:
      if fpsize==4: # extract the 32 bit
        s += f"  {outputs[outvar]} = IRExpr_Unop(Iop_64to32,{outputs[outvar]});\n"
      s += f"  {outputs[outvar]}_arr[{component}] = {outputs[outvar]};\n" 
    s += "}\n"
  for outvar in outputs:
    s += f"IRExpr* {outvar} = assembleSIMDVector({outputs[outvar]}_arr, {fpsize}, {simdsize}, diffenv);\n"
  return s

def createBarCode(op, inputs, floatinputs, partials, value, fpsize,simdsize,llo):
  """
    Create C code that records an operation on the tape for every component of a SIMD vector,
    given the partial derivatives.

    A partial derivative is specified as C code for an IRExpr* of type F64. The C code may involve
    - the IRExpr* arg1part of type I64 for the respective component of arg1, with the higher 4 bytes
      being zero if fpsize==4,
    - the IRExpr* arg1part_f of type F64 for the floating-point value. In the case fpsize==4, the lower
      4 bytes of arg1part were reinterpreted as F32 and then converted to F64. You need to add the index 1
      to floatinputs.
    - The same constructs for arg2, arg3, arg4.

    @param op - IROp whose barcode we currently define (necessary to infer the output type).
    @param inputs - List of argument indices (1...4) on which the result depends in a differentiable way.
    @param floatinputs - List of argument indices (1...4) for which the component is needed as F64 arg<i>_part_f.
    @param partials - List of partial derivatives w.r.t. the arguments in inputs. 
    @param value - Value of the result.
    @param fpsize - Size of component, either 4 or 8 (bytes)
    @param simdsize - Number of components, 1, 2, 4 or 8.
    @param llo - Whether it is a lowest-lane-only operation, boolean.
  """
  # createBarCode calls applyComponentwisely with the proper input and output vectors and type conversions.
  assert(len(inputs) in [1,2,3])
  input_names = {}
  for i in inputs:
    input_names[f"arg{i}"] = f"arg{i}_part"
    input_names[f"i{i}Lo"] = f"i{i}Lo_part"
    input_names[f"i{i}Hi"] = f"i{i}Hi_part"
  output_names = {"indexIntLo":"indexIntLo_part", "indexIntHi":"indexIntHi_part"}
  # the body computes IRExpr* indexIntLo_part, IRExpr* indexIntHi_part of type I64 with the upper and lower
  # layer of the index in the respective lower 4 bytes, and zero in the upper four bytes.
  bodyLowest = ""
  for i in floatinputs:
    if fpsize==4:
      bodyLowest += f'IRExpr* arg{i}_part_f = IRExpr_Unop(Iop_F32toF64,IRExpr_Unop(Iop_ReinterpI32asF32,IRExpr_Unop(Iop_64to32,arg{i}_part)));\n'
    else:
      bodyLowest += f'IRExpr* arg{i}_part_f = IRExpr_Unop(Iop_ReinterpI64asF64,arg{i}_part);'
  # add statement to push to tape
  if len(inputs)==1: # use index 0 to indicate missing input
    bodyLowest += f'  IRExpr** indexIntHiLo_part = dg_bar_writeToTape(diffenv,i{inputs[0]}Lo_part,i{inputs[0]}Hi_part,IRExpr_Const(IRConst_U64(0)),IRExpr_Const(IRConst_U64(0)), {partials[0]}, IRExpr_Const(IRConst_F64(0.)), {value});\n  IRExpr* indexIntLo_part = indexIntHiLo_part[0];\n  IRExpr* indexIntHi_part = indexIntHiLo_part[1];\n'
  elif len(inputs)==2:
    bodyLowest += f'  IRExpr** indexIntHiLo_part = dg_bar_writeToTape(diffenv,i{inputs[0]}Lo_part,i{inputs[0]}Hi_part,i{inputs[1]}Lo_part,i{inputs[1]}Hi_part, {partials[0]}, {partials[1]}, {value});\n  IRExpr* indexIntLo_part = indexIntHiLo_part[0];\n  IRExpr* indexIntHi_part = indexIntHiLo_part[1];\n'
  elif len(inputs)==3: # add two tape entries to combine three inputs
    bodyLowest += f'  IRExpr** indexIntermediateIntHiLo_part = dg_bar_writeToTape(diffenv,i{inputs[0]}Lo_part,i{inputs[0]}Hi_part,i{inputs[1]}Lo_part,i{inputs[1]}Hi_part, {partials[0]}, {partials[1]}, IRExpr_Const(IRConst_F64(0.)));\n  IRExpr* indexIntermediateIntLo_part = indexIntermediateIntHiLo_part[0];\n  IRExpr* indexIntermediateIntHi_part = indexIntermediateIntHiLo_part[1];\n'
    bodyLowest += f'  IRExpr** indexIntHiLo_part = dg_bar_writeToTape(diffenv,indexIntermediateIntLo_part,indexIntermediateIntHi_part,i{inputs[2]}Lo_part,i{inputs[2]}Hi_part, IRExpr_Const(IRConst_F64(1.)), {partials[2]}, {value});\n  IRExpr* indexIntLo_part = indexIntHiLo_part[0];\n  IRExpr* indexIntHi_part = indexIntHiLo_part[1];\n'
  if llo:
    bodyNonLowest = f'  IRExpr* indexIntLo_part = i{inputs[0]}Lo_part;\n  IRExpr* indexIntHi_part = i{inputs[0]}Hi_part;\n'
  else:
    bodyNonLowest = bodyLowest
  barcode = applyComponentwisely(input_names, output_names, fpsize, simdsize, bodyLowest, bodyNonLowest)
  # cast to the type of the result of the operation
  barcode += f"IRExpr* indexLo = reinterpretType(diffenv,indexIntLo, typeOfIRExpr(diffenv->sb_out->tyenv,{op.apply()}));\n"
  barcode += f"IRExpr* indexHi = reinterpretType(diffenv,indexIntHi, typeOfIRExpr(diffenv->sb_out->tyenv,{op.apply()}));\n"
  return barcode


def createTrickCode(op, activityinputs, floatinputs, resultIsDiscrete, fpsize,simdsize,llo):
  """
    Create C code that propagates bit-trick-finding flags separately 
    for every component of a SIMD vector.
    
    @param op - IROp whose trickcode we currently define (necessary to infer the output type).
    @param activityinputs - List of argument indices (1...4) that affect the activity flags of the result.
    @param floatinputs - List of argument indices (1...4) for which warnings are printed if they are active and discrete.
    @param resultIsDiscrete - Whether or not the result is discrete.
    @param fpsize - Size of component, either 4 or 8 (bytes)
    @param simdsize - Number of components, 1, 2, 4 or 8.
    @param llo - Whether it is a lowest-lane-only operation, boolean.
  """
  input_names = {}
  for i in activityinputs:
    input_names[f"arg{i}"] = f"arg{i}_part"
    input_names[f"f{i}Lo"] = f"f{i}Lo_part"
    input_names[f"f{i}Hi"] = f"f{i}Hi_part"
  output_names = {"flagsIntLo":"flagsIntLo_part", "flagsIntHi":"flagsIntHi_part"}
  bodyLowest = "" 
  # Check floating-point operands for activity and discreteness
  for i in floatinputs:
    bodyLowest += f'dg_trick_warn{fpsize}(diffenv, f{i}Lo_part, f{i}Hi_part);\n'
  # Propagation of activity flag: One non-zero bit in any input 
  bodyLowest += f"IRExpr* flagsIntLo_allzero = IRExpr_Const(IRConst_U1(True));\n" 
  for i in activityinputs:
    bodyLowest += f"flagsIntLo_allzero = IRExpr_Binop(Iop_And1, flagsIntLo_allzero, isZero(f{i}Lo_part,Ity_I64));\n"
  bodyLowest += f"IRExpr* flagsIntLo_part = IRExpr_ITE(flagsIntLo_allzero, mkIRConst_zero(Ity_I64), mkIRConst_ones(Ity_I64));\n"
  # Determine discreteness of the result
  if resultIsDiscrete:
    bodyLowest += f"IRExpr* flagsIntHi_part = mkIRConst_ones(Ity_I64);\n"
  else:
    bodyLowest += f"IRExpr* flagsIntHi_part = mkIRConst_zero(Ity_I64);\n"
  # For llo operations, the higher-lane body is trivial
  if llo:
    bodyNonLowest = f'  IRExpr* flagsIntLo_part = f{activityinputs[0]}Lo_part;\n  IRExpr* flagsIntHi_part = f{activityinputs[0]}Hi_part;\n'
  else:
    bodyNonLowest = bodyLowest
  trickcode = applyComponentwisely(input_names, output_names, fpsize, simdsize, bodyLowest, bodyNonLowest)
  # cast to the type of the result of the operation
  trickcode += f"IRExpr* flagsLo = reinterpretType(diffenv,flagsIntLo, typeOfIRExpr(diffenv->sb_out->tyenv,{op.apply()}));\n"
  trickcode += f"IRExpr* flagsHi = reinterpretType(diffenv,flagsIntHi, typeOfIRExpr(diffenv->sb_out->tyenv,{op.apply()}));\n"
  return trickcode



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
  # additionally, Sqrt64Fx4 and Sqrt32Fx8 have no rounding mode
  sqrt_noroundingmode = llo or fpsize*simdsize==32
  if sqrt_noroundingmode:
    sqrt_arg1 = None; sqrt_arg2 = "arg1"; sqrt_d2 = "d1";
    rounding_mode = None if llo else "dg_rounding_mode" # rounding mode for div and mul
  else:
    sqrt_arg1 = "arg1"; sqrt_arg2 = "arg2"; sqrt_d2 = "d2";
    rounding_mode = "arg1"

  add = IROp_Info(f"Iop_Add{suffix}",3-llo,[2-llo,3-llo],fpsize,simdsize,True)
  sub = IROp_Info(f"Iop_Sub{suffix}",3-llo,[2-llo,3-llo],fpsize,simdsize,True)
  mul = IROp_Info(f"Iop_Mul{suffix}",3-llo,[2-llo,3-llo],fpsize,simdsize,True)
  div = IROp_Info(f"Iop_Div{suffix}",3-llo,[2-llo,3-llo],fpsize,simdsize,True)
  sqrt = IROp_Info(f"Iop_Sqrt{suffix}",2-sqrt_noroundingmode,[2-sqrt_noroundingmode],fpsize,simdsize,True)

  add.dotcode = dv(add.apply(arg1,d2,d3))
  sub.dotcode = dv(sub.apply(arg1,d2,d3))
  mul.dotcode = dv(add.apply(arg1,mul.apply(arg1,d2,arg3),mul.apply(arg1,d3,arg2)))
  div.dotcode = dv(div.apply(arg1,sub.apply(arg1,mul.apply(arg1,d2,arg3),mul.apply(arg1,arg2,d3)),mul.apply(arg1,arg3,arg3)))
  sqrt.dotcode = dv(div.apply(rounding_mode,sqrt_d2,mul.apply(rounding_mode,f"mkIRConst_fptwo({fpsize},{simdsize})",sqrt.apply(sqrt_arg1,sqrt_arg2))))

  add.barcode = createBarCode(add, [2-llo,3-llo], [2-llo,3-llo], ["IRExpr_Const(IRConst_F64(1.))", "IRExpr_Const(IRConst_F64(1.))"], f"IRExpr_Triop(Iop_AddF64,dg_rounding_mode,{arg2}_part_f,{arg3}_part_f)", fpsize, simdsize, llo)
  sub.barcode = createBarCode(sub, [2-llo,3-llo], [2-llo,3-llo], ["IRExpr_Const(IRConst_F64(1.))", "IRExpr_Const(IRConst_F64(-1.))"], f"IRExpr_Triop(Iop_SubF64,dg_rounding_mode,{arg2}_part_f,{arg3}_part_f)", fpsize, simdsize, llo)
  mul.barcode = createBarCode(mul, [2-llo,3-llo], [2-llo, 3-llo], [f"{arg3}_part_f", f"{arg2}_part_f"], f"IRExpr_Triop(Iop_MulF64,dg_rounding_mode,{arg2}_part_f,{arg3}_part_f)", fpsize, simdsize, llo)
  div.barcode = createBarCode(div, [2-llo,3-llo], [2-llo, 3-llo], [f"IRExpr_Triop(Iop_DivF64,dg_rounding_mode,IRExpr_Const(IRConst_F64(1.)),{arg3}_part_f)", f"IRExpr_Triop(Iop_DivF64,dg_rounding_mode,IRExpr_Triop(Iop_MulF64,dg_rounding_mode,IRExpr_Const(IRConst_F64(-1.)), {arg2}_part_f),IRExpr_Triop(Iop_MulF64,dg_rounding_mode,{arg3}_part_f,{arg3}_part_f))"],f"IRExpr_Triop(Iop_DivF64,dg_rounding_mode,{arg2}_part_f,{arg3}_part_f)", fpsize, simdsize, llo)
  sqrt.barcode = createBarCode(sqrt, [2-sqrt_noroundingmode], [2-sqrt_noroundingmode], [f"IRExpr_Triop(Iop_DivF64,dg_rounding_mode,IRExpr_Const(IRConst_F64(0.5)), IRExpr_Binop(Iop_SqrtF64,dg_rounding_mode,{sqrt_arg2}_part_f))"], f"IRExpr_Binop(Iop_SqrtF64,dg_rounding_mode,{sqrt_arg2}_part_f)", fpsize, simdsize, llo)

  add.trickcode = createTrickCode(add, [2-llo,3-llo], [2-llo,3-llo], False, fpsize, simdsize, llo)
  sub.trickcode = createTrickCode(sub, [2-llo,3-llo], [2-llo,3-llo], False, fpsize, simdsize, llo)
  mul.trickcode = createTrickCode(mul, [2-llo,3-llo], [2-llo,3-llo], False, fpsize, simdsize, llo)
  div.trickcode = createTrickCode(div, [2-llo,3-llo], [2-llo,3-llo], False, fpsize, simdsize, llo)
  sqrt.trickcode = createTrickCode(sqrt, [2-sqrt_noroundingmode], [2-sqrt_noroundingmode], False, fpsize, simdsize, llo)

  IROp_Infos += [add,sub,mul,div,sqrt]

# Neg - different set of SIMD setups, no rounding mode
for suffix,fpsize,simdsize in [("F64",8,1),("F32",4,1),("64Fx2",8,2),("32Fx2",4,2),("32Fx4",4,4)]:
  neg = IROp_Info(f"Iop_Neg{suffix}", 1,[1],fpsize,simdsize,True)
  neg.dotcode = dv(neg.apply("d1"))
  neg.barcode = createBarCode(neg, [1], [1], ["IRExpr_Const(IRConst_F64(-1.))"], neg.apply("arg1_part_f"), fpsize, simdsize, False)
  neg.trickcode = createTrickCode(neg, [1], [1], False, fpsize, simdsize, False)
  IROp_Infos += [ neg ]
# Abs 
for suffix,fpsize,simdsize,llo in [pF64,pF32]: # p64Fx2, p32Fx2, p32Fx4 exist, but AD logic is different
  abs_ = IROp_Info(f"Iop_Abs{suffix}", 1, [1],fpsize,simdsize,True)
  abs_.dotcode = dv(f"IRExpr_ITE(IRExpr_Unop(Iop_32to1,IRExpr_Binop(Iop_Cmp{suffix}, arg1, IRExpr_Const(IRConst_{suffix}i(0)))), IRExpr_Unop(Iop_Neg{suffix},d1), d1)")
  abs_.barcode = createBarCode(abs_, [1], [1], [f"IRExpr_ITE( IRExpr_Unop(Iop_32to1,IRExpr_Binop(Iop_CmpF64, arg1_part_f, IRExpr_Const(IRConst_{suffix}i(0)))) , IRExpr_Const(IRConst_F64(-1.)), IRExpr_Const(IRConst_F64(1.)))"], abs_.apply("arg1_part_f"), fpsize, simdsize, llo)
  abs_.trickcode = createTrickCode(abs_, [1], [1], False, fpsize, simdsize, llo)
  IROp_Infos += [ abs_ ]
# Min, Max
for Op, op in [("Min", "min"), ("Max", "max")]:
  for suffix,fpsize,simdsize,llo in [p32Fx2,p32Fx4,p32F0x4,p64Fx2,p64F0x2,p32Fx8,p64Fx4]:
    the_op = IROp_Info(f"Iop_{Op}{suffix}", 2, [1,2],fpsize,simdsize,True)
    the_op.dotcode = applyComponentwisely({"arg1":"arg1_part","d1":"d1_part","arg2":"arg2_part","d2":"d2_part"}, {"dotvalue":"dotvalue_part"}, fpsize, simdsize, f'IRExpr* dotvalue_part = mkIRExprCCall(Ity_I64,0,"dg_dot_arithmetic_{op}{fpsize*8}", &dg_dot_arithmetic_{op}{fpsize*8}, mkIRExprVec_4(arg1_part, d1_part, arg2_part, d2_part));') 
    the_op.barcode = createBarCode(the_op, [1,2], [1,2], [f"IRExpr_ITE(IRExpr_Unop(Iop_32to1,IRExpr_Binop(Iop_CmpF64,arg1_part_f,arg2_part_f)),  IRExpr_Const(IRConst_F64({'1.' if op=='min' else '0.'})),  IRExpr_Const(IRConst_F64({'0.' if op=='min' else '1.'})) )",     f"IRExpr_ITE(IRExpr_Unop(Iop_32to1,IRExpr_Binop(Iop_CmpF64,arg1_part_f,arg2_part_f)),  IRExpr_Const(IRConst_F64({'0.' if op=='min' else '1.'})),  IRExpr_Const(IRConst_F64({'1.' if op=='min' else '0.'})) )"], f"IRExpr_ITE(IRExpr_Unop(Iop_32to1,IRExpr_Binop(Iop_CmpF64, arg1_part_f, arg2_part_f)), {'arg1_part_f' if Op=='Min' else 'arg2_part_f'}, {'arg2_part_f' if Op=='Min' else 'arg1_part_f'})", fpsize, simdsize,llo)
    the_op.trickcode = createTrickCode(the_op, [1,2], [1,2], False, fpsize, simdsize, llo) # TODO more precise
    IROp_Infos += [ the_op ]
# fused multiply-add
for Op in ["Add", "Sub"]:
  for suffix,fpsize,simdsize,llo in [pF64,pF32]:
    the_op = IROp_Info(f"Iop_M{Op}{suffix}", 4, [2,3,4],fpsize,simdsize,True)
    # perform calculations with F64 operations even in the F32 case, otherwise there is an ISEL error...
    if fpsize==8:
      arg2 = "arg2"
      arg3 = "arg3"
      d2 = "d2"
      d3 = "d3"
      d4 = "d4"
    else:
      arg2 = "IRExpr_Unop(Iop_F32toF64,arg2)"
      arg3 = "IRExpr_Unop(Iop_F32toF64,arg3)"
      d2 = "IRExpr_Unop(Iop_F32toF64,d2)"
      d3 = "IRExpr_Unop(Iop_F32toF64,d3)"
      d4 = "IRExpr_Unop(Iop_F32toF64,d4)"
    res = f"IRExpr_Triop(Iop_AddF64, arg1, IRExpr_Triop(Iop_AddF64, arg1, IRExpr_Triop(Iop_MulF64,arg1,{d2},{arg3}), IRExpr_Triop(Iop_MulF64,arg1,{arg2},{d3})), {d4})"
    if fpsize==4:
      res = f"IRExpr_Binop(Iop_F64toF32,arg1,{res})"
    the_op.dotcode = dv(res)
    the_op.barcode = createBarCode(the_op, [2,3,4], [2,3,4], ["arg3_part_f", "arg2_part_f", f"IRExpr_Const(IRConst_F64({'1.' if Op=='Add' else '-1.'}))"], the_op.apply("arg1", "arg2_part_f", "arg3_part_f", "arg4_part_f"), fpsize, simdsize,llo)
    the_op.trickcode = createTrickCode(the_op, [2,3,4], [2,3,4], False, fpsize, simdsize,llo)
    IROp_Infos += [ the_op ]

# Miscellaneous
scalef64 = IROp_Info("Iop_ScaleF64",  3, [2],8,1,True)
scalef64.dotcode = dv(scalef64.apply("arg1","d2","arg3"))
scalef64.barcode = createBarCode(scalef64, [2], [], ["IRExpr_Triop(Iop_ScaleF64,arg1,IRExpr_Const(IRConst_F64(1.)),arg3)"], scalef64.apply(), 8, 1, False)
scalef64.trickcode = createTrickCode(scalef64, [2], [2], False, 8, 1, False)
yl2xf64 = IROp_Info("Iop_Yl2xF64", 3, [2,3],8,1,True)
yl2xf64.dotcode = dv("IRExpr_Triop(Iop_AddF64,arg1,IRExpr_Triop(Iop_Yl2xF64,arg1,d2,arg3),IRExpr_Triop(Iop_DivF64,arg1,IRExpr_Triop(Iop_MulF64,arg1,arg2,d3),IRExpr_Triop(Iop_MulF64,arg1,IRExpr_Const(IRConst_F64(0.6931471805599453094172321214581)),arg3)))")
yl2xf64.barcode = createBarCode(yl2xf64, [2,3], [], ["IRExpr_Triop(Iop_Yl2xF64,arg1,IRExpr_Const(IRConst_F64(1.)),arg3)",  "IRExpr_Triop(Iop_DivF64,arg1,arg2,IRExpr_Triop(Iop_MulF64,arg1,IRExpr_Const(IRConst_F64(0.6931471805599453094172321214581)), arg3))"], yl2xf64.apply(), 8, 1, False)
yl2xf64.trickcode = createTrickCode(yl2xf64, [2,3], [2,3], False, 8, 1, False)
yl2xp1f64 = IROp_Info("Iop_Yl2xp1F64", 3, [2,3],8,1,True)
yl2xp1f64.dotcode = dv("IRExpr_Triop(Iop_AddF64,arg1,IRExpr_Triop(Iop_Yl2xp1F64,arg1,d2,arg3),IRExpr_Triop(Iop_DivF64,arg1,IRExpr_Triop(Iop_MulF64,arg1,arg2,d3),IRExpr_Triop(Iop_MulF64,arg1,IRExpr_Const(IRConst_F64(0.6931471805599453094172321214581)),IRExpr_Triop(Iop_AddF64, arg1, arg3, IRExpr_Const(IRConst_F64(1.))))))")
yl2xp1f64.barcode = createBarCode(yl2xp1f64, [2,3], [], ["IRExpr_Triop(Iop_Yl2xp1F64,arg1,IRExpr_Const(IRConst_F64(1.)),arg3)",  "IRExpr_Triop(Iop_DivF64,arg1,arg2,IRExpr_Triop(Iop_MulF64,arg1,IRExpr_Const(IRConst_F64(0.6931471805599453094172321214581)),IRExpr_Triop(Iop_AddF64, arg1, arg3, IRExpr_Const(IRConst_F64(1.)))))"], yl2xp1f64.apply(), 8, 1, False)
yl2xp1f64.trickcode = createTrickCode(yl2xp1f64, [2,3], [2,3], False, 8, 1, False)
IROp_Infos += [ scalef64, yl2xf64, yl2xp1f64 ]

### Bitwise logical instructions. ###

for Op, op in [("And","and"), ("Or","or"), ("Xor","xor")]:
  # the dirty calls handling 8-byte blocks also consider the case of 2 x 4 bytes
  for (simdsize,fpsize) in [(1,4),(1,8),(2,8),(4,8)]:
    size = simdsize*fpsize*8
    the_op = IROp_Info(f"Iop_{Op}{'V' if size>=128 else ''}{size}", 2, [1,2],fpsize,simdsize,False)
    the_op.dotcode = applyComponentwisely({"arg1":"arg1_part","d1":"d1_part","arg2":"arg2_part","d2":"d2_part"}, {"dotvalue":"dotvalue_part"}, fpsize, simdsize, f'IRExpr* dotvalue_part = mkIRExprCCall(Ity_I64,0,"dg_dot_bitwise_{op}64", &dg_dot_bitwise_{op}64, mkIRExprVec_4(arg1_part, d1_part, arg2_part, d2_part));') 
    the_op.barcode = applyComponentwisely({"arg1":"arg1_part","i1Lo":"i1Lo_part","i1Hi":"i1Hi_part","arg2":"arg2_part","i2Lo":"i2Lo_part","i2Hi":"i2Hi_part"}, {"indexLo":"indexLo_part","indexHi":"indexHi_part"}, fpsize, simdsize, f'IRDirty* di = unsafeIRDirty_0_N( 0, "dg_bar_bitwise_{op}64", &dg_bar_bitwise_{op}64, mkIRExprVec_6(arg1_part, i1Lo_part, i1Hi_part, arg2_part, i2Lo_part, i2Hi_part));  \n addStmtToIRSB(diffenv->sb_out, IRStmt_Dirty(di));\n IRTemp iLo = newIRTemp(diffenv->sb_out->tyenv, Ity_I64), iHi = newIRTemp(diffenv->sb_out->tyenv, Ity_I64);\n   IRDirty* diLo = unsafeIRDirty_1_N( iLo, 0, "dg_bar_bitwise_get_lower", &dg_bar_bitwise_get_lower, mkIRExprVec_0());\naddStmtToIRSB(diffenv->sb_out, IRStmt_Dirty(diLo));  IRDirty* diHi = unsafeIRDirty_1_N( iHi, 0, "dg_bar_bitwise_get_higher", &dg_bar_bitwise_get_higher, mkIRExprVec_0());\naddStmtToIRSB(diffenv->sb_out, IRStmt_Dirty(diHi));\n   IRExpr* indexLo_part = IRExpr_RdTmp(iLo);\n IRExpr* indexHi_part = IRExpr_RdTmp(iHi); ') 
    the_op.trickcode = applyComponentwisely({"arg1":"arg1_part","f1Lo":"f1Lo_part","f1Hi":"f1Hi_part","arg2":"arg2_part","f2Lo":"f2Lo_part","f2Hi":"f2Hi_part"}, {"flagsLo":"flagsLo_part","flagsHi":"flagsHi_part"}, fpsize, simdsize, f'IRDirty* di = unsafeIRDirty_0_N( 0, "dg_trick_bitwise_{op}64", &dg_trick_bitwise_{op}64, mkIRExprVec_6(arg1_part, f1Lo_part, f1Hi_part, arg2_part, f2Lo_part, f2Hi_part));  \n addStmtToIRSB(diffenv->sb_out, IRStmt_Dirty(di));\n IRTemp fLo = newIRTemp(diffenv->sb_out->tyenv, Ity_I64), fHi = newIRTemp(diffenv->sb_out->tyenv, Ity_I64);\n   IRDirty* diLo = unsafeIRDirty_1_N( fLo, 0, "dg_trick_bitwise_get_lower", &dg_trick_bitwise_get_lower, mkIRExprVec_0());\naddStmtToIRSB(diffenv->sb_out, IRStmt_Dirty(diLo));  IRDirty* diHi = unsafeIRDirty_1_N( fHi, 0, "dg_trick_bitwise_get_higher", &dg_trick_bitwise_get_higher, mkIRExprVec_0());\naddStmtToIRSB(diffenv->sb_out, IRStmt_Dirty(diHi));\n   IRExpr* flagsLo_part = IRExpr_RdTmp(fLo);\n IRExpr* flagsHi_part = IRExpr_RdTmp(fHi); ') 
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
  the_op = IROp_Info(f"Iop_{op}",1,[1],0,1,False)
  the_op.dotcode = dv(f"IRExpr_Unop(Iop_{op},d1)")
  the_op.barcode = f"IRExpr* indexLo = IRExpr_Unop(Iop_{op},i1Lo);\nIRExpr* indexHi = IRExpr_Unop(Iop_{op},i1Hi);"
  the_op.trickcode = f"IRExpr* flagsLo = IRExpr_Unop(Iop_{op},f1Lo);\nIRExpr* flagsHi = IRExpr_Unop(Iop_{op},f1Hi);"
  IROp_Infos += [the_op]

# Conversion F32 -> F64: Apply analogously to dot value, and add zero bytes to index.
the_op = IROp_Info("Iop_F32toF64",1,[1],8,1,True)
the_op.dotcode = dv("IRExpr_Unop(Iop_F32toF64,d1)")
the_op.barcode = "\n".join([f"IRExpr* index{HiLo} = IRExpr_Unop(Iop_ReinterpI64asF64,IRExpr_Binop(Iop_32HLto64,IRExpr_Const(IRConst_U32(0)),IRExpr_Unop(Iop_ReinterpF32asI32,i1{HiLo})));" for HiLo in ["Lo","Hi"]])
the_op.trickcode = f"dg_trick_warn4(diffenv, IRExpr_Binop(Iop_32HLto64, IRExpr_Unop(Iop_ReinterpF32asI32,f1Lo),IRExpr_Const(IRConst_U32(0))), IRExpr_Binop(Iop_32HLto64, IRExpr_Unop(Iop_ReinterpF32asI32,f1Hi), IRExpr_Const(IRConst_U32(0)))); IRExpr* flagsLo = IRExpr_ITE(isZero(f1Lo, Ity_F32), mkIRConst_zero(Ity_F64), mkIRConst_ones(Ity_F64));\n IRExpr* flagsHi = mkIRConst_zero(Ity_F64);\n"
IROp_Infos += [the_op]


# Zero-derivative unary operations
for op in ["I32StoF64", "I32UtoF64"]:
  the_op = IROp_Info(f"Iop_{op}",1,[],8,1,True)
  the_op.dotcode = dv("IRExpr_Const(IRConst_F64i(0))")
  the_op.barcode = "\n".join([f"IRExpr* index{HiLo} = mkIRConst_zero(Ity_F64);" for HiLo in ["Lo","Hi"]])
  the_op.trickcode = f"IRExpr* flagsLo = IRExpr_ITE(isZero(f1Lo,Ity_I32), mkIRConst_zero(Ity_F64), mkIRConst_ones(Ity_F64));\n IRExpr* flagsHi = mkIRConst_zero(Ity_F64);"
  IROp_Infos += [the_op]

# Binary operations that move data, apply analogously to dot values and indices.
ops = []
ops += [f"Iop_{n}HLto{2*n}" for n in [8,16,32,64]]
ops += ["Iop_64HLtoV128", "Iop_V128HLtoV256"]
ops += ["Iop_SetV128lo32", "Iop_SetV128lo64"]
ops += [f"Iop_Interleave{hilo}{n}x{128//n}" for hilo in ["HI","LO"] for n in [8,16,32,64]]
for op in ops:
  the_op = IROp_Info(op,2,[1,2],0,1,False)
  the_op.dotcode = dv(f"IRExpr_Binop({op},d1,d2)")
  the_op.barcode = "\n".join([f"IRExpr* index{HiLo} = IRExpr_Binop({op},i1{HiLo},i2{HiLo});" for HiLo in ["Lo","Hi"]])
  the_op.trickcode = "\n".join([f"IRExpr* flags{HiLo} = IRExpr_Binop({op},f1{HiLo},f2{HiLo});" for HiLo in ["Lo","Hi"]])
  IROp_Infos += [the_op]

# Bitshifts, as a special case of data move operations
for direction in ["Shr","Shl"]:
  for size in [8,16,32,64]:
    the_op = IROp_Info(f"Iop_{direction}{size}", 2, [1], 0,1,False);
    the_op.dotcode = dv(f"IRExpr_Binop(Iop_{direction}{size},d1,arg2)")
    the_op.barcode = "\n".join([f"IRExpr* index{HiLo} = IRExpr_Binop(Iop_{direction}{size},i1{HiLo},arg2);" for HiLo in ["Lo","Hi"]])
    the_op.trickcode = f"IRExpr* flagsLo = IRExpr_Binop(Iop_{direction}{size}, f1Lo, arg2);\n IRExpr* flagsHi = IRExpr_ITE(isZero(arg2,Ity_I8), IRExpr_Binop(Iop_{direction}{size}, f1Hi, arg2), mkIRConst_ones(typeOfIRExpr(diffenv->sb_out->tyenv,{the_op.apply()}))); " # set discreteness flag for non-trivial shift
    IROp_Infos += [the_op]

# Conversion F64 -> F32: Apply analogously to dot value, and cut bytes from index.
f64tof32 = IROp_Info("Iop_F64toF32",2,[2],4,1,True)
f64tof32.dotcode = dv(f64tof32.apply("arg1","d2"))
f64tof32.barcode = "\n".join([f"IRExpr* index{HiLo} = IRExpr_Unop(Iop_ReinterpI32asF32,IRExpr_Unop(Iop_64to32,IRExpr_Unop(Iop_ReinterpF64asI64,i2{HiLo})));" for HiLo in ["Lo","Hi"]])
f64tof32.trickcode = f"dg_trick_warn8(diffenv, IRExpr_Unop(Iop_ReinterpF64asI64,f2Lo), IRExpr_Unop(Iop_ReinterpF64asI64,f2Hi)); IRExpr* flagsLo = IRExpr_ITE(isZero(f2Lo, Ity_F64), mkIRConst_zero(Ity_F32), mkIRConst_ones(Ity_F32));\n IRExpr* flagsHi = mkIRConst_zero(Ity_F32); "
IROp_Infos += [f64tof32]

# Zero-derivative binary operations
for op in ["I64StoF64","I64UtoF64","RoundF64toInt"]:
  the_op = IROp_Info(f"Iop_{op}",2,[],8,1,True)
  the_op.dotcode = dv("IRExpr_Const(IRConst_F64i(0))")
  the_op.barcode = "\n".join([f"IRExpr* index{HiLo} = IRExpr_Unop(Iop_ReinterpI64asF64,IRExpr_Const(IRConst_U64(0)));" for HiLo in ["Lo","Hi"]])
  # Difficult to see what the proper bit-trick-finding instrumentation is. Either you suspect a bit-trick with these operations, and warn whenever they have an active operand, or at least set the discreteness flags of the result. Or you consider them ok, handling activity bits in an infectious way and not setting discreteness bits. We go for the latter option.
  the_op.trickcode = "IRExpr* flagsLo = IRExpr_ITE(isZero(f2Lo,typeOfIRExpr(diffenv->sb_out->tyenv,f2Lo)), mkIRConst_zero(Ity_F64), mkIRConst_ones(Ity_F64));\n IRExpr* flagsHi = mkIRConst_zero(Ity_F64);"
  if op=="RoundF64toInt":
    the_op.trickcode = "dg_trick_warn8(diffenv, IRExpr_Unop(Iop_ReinterpF64asI64,f2Lo), IRExpr_Unop(Iop_ReinterpF64asI64,f2Hi)); " + the_op.trickcode
  IROp_Infos += [the_op]
for op in ["I64StoF32","I64UtoF32","I32StoF32","I32UtoF32"]:
  the_op = IROp_Info(f"Iop_{op}",2,[],4,1,True)
  the_op.dotcode = dv("IRExpr_Const(IRConst_F32i(0))")
  the_op.barcode = "\n".join([f"IRExpr* index{HiLo} = IRExpr_Unop(Iop_ReinterpI32asF32,IRExpr_Const(IRConst_U32(0)));" for HiLo in ["Lo","Hi"]])
  # See the above discussion of the 64-bit case. 
  the_op.trickcode = "IRExpr* flagsLo = IRExpr_ITE(isZero(f2Lo,typeOfIRExpr(diffenv->sb_out->tyenv,f2Lo)), mkIRConst_zero(Ity_F32), mkIRConst_ones(Ity_F32));\n IRExpr* flagsHi = mkIRConst_zero(Ity_F32);"
  IROp_Infos += [the_op]

# Quaternary operation that moves data, apply analogously to dot values and indices.
i64x4tov256 = IROp_Info("Iop_64x4toV256",4,[1,2,3,4],0,1,False)
i64x4tov256.dotcode = dv("IRExpr_Qop(Iop_64x4toV256,d1,d2,d3,d4)")
i64x4tov256.barcode = "\n".join([f"IRExpr* index{HiLo} = IRExpr_Qop(Iop_64x4toV256,i1{HiLo},i2{HiLo},i3{HiLo},i4{HiLo});" for HiLo in ["Lo","Hi"]])
i64x4tov256.trickcode = "\n".join([f"IRExpr* flags{HiLo} = IRExpr_Qop(Iop_64x4toV256,f1{HiLo},f2{HiLo},f3{HiLo},f4{HiLo});" for HiLo in ["Lo","Hi"]])
IROp_Infos += [i64x4tov256]




### Produce code. ###
# Some operations created above do actually not exist in VEX, remove them.
IROp_Missing = ["Iop_Div32Fx2","Iop_Sqrt32Fx2"]
IROp_Infos = [irop_info for irop_info in IROp_Infos if irop_info.name not in IROp_Missing]

import sys
mode = 'dot'
if(len(sys.argv)>=2):
  mode = sys.argv[1]

print("""
/*
   This file is part of Derivgrind, an automatic differentiation
   tool applicable to compiled programs.

   Copyright (C) 2022, Chair for Scientific Computing, TU Kaiserslautern
   Copyright (C) since 2023, Chair for Scientific Computing, University of Kaiserslautern-Landau
   Homepage: https://www.scicomp.uni-kl.de
   Contact: Prof. Nicolas R. Gauger (derivgrind@projects.rptu.de)

   Lead developer: Max Aehle

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, see <http://www.gnu.org/licenses/>.

   The GNU General Public License is contained in the file COPYING.
*/

// ----------------------------------------------------
// WARNING: This file has been generated by 
// gen_operationhandling_code.py. Do not manually edit it.
// ----------------------------------------------------
""")

for irop_info in IROp_Infos:
  if mode=='dot':
    print(irop_info.makeCaseDot(False))
  elif mode=='dot-dqd':
    print(irop_info.makeCaseDot(True))
  elif mode=='bar':
    print(irop_info.makeCaseBar())
  elif mode=='trick':
    print(irop_info.makeCaseTrick())
  else:
    print(f"Error: Bad mode '{mode}'.",file=sys.stderr)
    exit(1)


