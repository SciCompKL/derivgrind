# -------------------------------------------------------------------- #
# --- Build and execute unit tests for Derivgrind.    TestCase .py --- #
# -------------------------------------------------------------------- #

#
#  This file is part of Derivgrind, a tool performing forward-mode
#  algorithmic differentiation of compiled programs, implemented
#  in the Valgrind framework.
#
#  Copyright (C) 2022 Chair for Scientific Computing (SciComp), TU Kaiserslautern
#  Homepage: https://www.scicomp.uni-kl.de
#  Contact: Prof. Nicolas R. Gauger (derivgrind@scicomp.uni-kl.de)
#
#  Lead developer: Max Aehle (SciComp, TU Kaiserslautern)
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

import subprocess
import re
import time
import os
import stat

# Type information: 
# ctype / ftype: Keyword in the C/C++/Fortran source
# gdbptype: Regex to recognize pointer to variable in GDB output
# size: size in bytes
# tol: tolerance of the tests
# get / set: Derivgrind monitor commands
# format: C printf format specifier

# C/C++ types
TYPE_DOUBLE = {"ctype":"double", "gdbptype":"double \*", "size":8, "tol":1e-8, "get":"get", "set":"set","format":"%.16lf"}
TYPE_FLOAT = {"ctype":"float", "gdbptype":"float \*", "size":4, "tol":1e-4, "get":"fget", "set":"fset","format":"%.9f"}
TYPE_LONG_DOUBLE = {"ctype":"long double", "gdbptype":"long double \*", "size":"sizeof(long double)", "tol":1e-8, "get":"lget", "set":"lset","format":"%.16Lf"}
# Fortran types
TYPE_REAL4 = {"ftype":"real", "gdbptype":"PTR TO -> \( real\(kind=4\) \)", "size":4, "tol":1e-4, "get":"fget", "set":"fset","format":"%.9f"}
TYPE_REAL8 = {"ftype":"double precision", "gdbptype":"PTR TO -> \( real\(kind=8\) \)", "size":8, "tol":1e-8, "get":"get", "set":"set","format":"%.9f"}
# Python types, not suitable for interactive testcases
TYPE_PYTHONFLOAT = {"tol":1e-8, "pytype":"float"}
TYPE_NUMPYFLOAT64 = {"tol":1e-8, "pytype":"np.float64"}
TYPE_NUMPYFLOAT32 = {"tol":1e-4, "pytype":"np.float32"}

def str_fortran(d):
  """Convert d to Fortran double precision literal."""
  s = str(d)
  if "e" in s:
    return s.replace("e","d")
  else:
    return s+"d0"

class TestCase:
  """Basic data for a Derivgrind test case."""
  def __init__(self, name):
    self.name = name # Name of TestCase
    self.mode = 'd' # 'd': dot/forward, 'b': bar/recording
    self.stmtd = None # Code to be run in C/C++ main function for double test
    self.stmtf = None # Code to be run in C/C++ main function for float test
    self.stmtl = None # Code to be run in C/C++ main function for long double test
    self.stmtr4 = None # Code to be run in Fortran program for real*4 test
    self.stmtr8 = None # Code to be run in Fortran program for real*8 test
    self.stmtp = None # Code to be run in Python program
    self.stmt = None # One out of stmtd, stmtf, stmtl will be copied here
    # if a testcase is meant only for a subset of the three datatypes only,
    # set the others stmtX to None
    self.include = "" # Code pasted above main function
    self.vals = {} # Assigns values to input variables used by stmt
    self.dots = {} # Assigns dot values to input variables used by stmt
    self.bars = {} # Assigns bar values to output variables used by stmt
    self.test_vals = {} # Expected values of output variables computed by stmt
    self.test_dots = {} # Expected dot values of output variables computed by stmt
    self.test_bars = {} # Expected bar values of input variables computed by stmt
    self.timeout = 10 # Timeout for execution in Valgrind, in seconds
    self.cflags = "" # Additional flags for the C compiler
    self.cflags_clang = None # Additional flags for the C compiler, if clang is used
    self.fflags = "" # Additional flags for the Fortran compiler
    self.ldflags = "" # Additional flags for the linker, e.g. "-lm"
    self.type = TYPE_DOUBLE # TYPE_DOUBLE, TYPE_FLOAT, TYPE_LONG_DOUBLE (for C/C++), TYPE_REAL4, TYPE_REAL8 (for Fortran)
    self.arch = 32 # 32 bit (x86) or 64 bit (amd64)
    self.disable = lambda mode, arch, language, typename : False # if True, test will not be run
    self.compiler = "gcc" # gcc, g++, gfortran, python

class InteractiveTestCase(TestCase):
  """Methods to run a Derivgrind test case interactively in VGDB."""
  def __init__(self,name):
    super().__init__(name)

  def produce_code(self):
    # Insert testcase data into C or Fortran code template. 
    # The template has two labels around the statement, in order 
    # to find the lines where breakpoints should be added.
    self.line_before_stmt = 0
    self.line_after_stmt = 0
    if self.compiler in ['gcc','g++','clang','clang++']:
      self.code = f"""
        {self.include}
        int main(){{
          { " ".join([self.type["ctype"]+" "+var+" = "+str(self.vals[var])+";" for var in self.vals]) }
          /*_testcase_before_stmt*/ __asm__("nop");
          {self.stmt};
          /*_testcase_after_stmt*/ __asm__("nop");
        }}
      """
      for i, line in enumerate(self.code.split("\n")):
        if line.strip().startswith("/*_testcase_before_stmt*/"):
          self.line_before_stmt = i+1
        if line.strip().startswith("/*_testcase_after_stmt*/"):
          self.line_after_stmt = i+1
    elif self.compiler=='gfortran':
      self.code = f"""
        program main
        implicit none
        { " ".join([self.type["ftype"]+" :: "+var+" = "+str(self.vals[var])+";" for var in self.vals]) }
        ! _testcase_before_stmt
        do while(0==1) ; end do
        block
        { self.stmt }
        ! _testcase_after_stmt
        do while(0==1) ; end do
        end block
        end program main
        """
      for i, line in enumerate(self.code.split("\n")):
        if line.strip().startswith("! _testcase_before_stmt"):
          self.line_before_stmt = i+2
        if line.strip().startswith("! _testcase_after_stmt"):
          self.line_after_stmt = i+2

    assert self.line_before_stmt != 0
    assert self.line_after_stmt != 0

  def compile_code(self):
    # write C or Fortran code into file
    if self.compiler in ['gcc','clang']:
      self.source_filename = "TestCase_src.c"
      with open(self.source_filename, "w") as f:
        f.write(self.code)
      compile_process = subprocess.run(['gcc', "-g", "-O0", self.source_filename, "-o", "TestCase_exec"] + (["-m32"] if self.arch==32 else []) + ( self.cflags_clang.split() if self.cflags_clang!=None and self.compiler=='clang' else self.cflags.split() ) + self.ldflags.split(),universal_newlines=True)
    elif self.compiler in ['g++','clang++']:
      self.source_filename = "TestCase_src.cpp"
      with open(self.source_filename, "w") as f:
        f.write(self.code)
      compile_process = subprocess.run(['g++', "-g", "-O0", self.source_filename, "-o", "TestCase_exec"] + (["-m32"] if self.arch==32 else []) + ( self.cflags_clang.split() if self.cflags_clang!=None and self.compiler=='clang++' else self.cflags.split() ) + self.ldflags.split(),universal_newlines=True)
    elif self.compiler=='gfortran':
      self.source_filename = "TestCase_src.f90"
      with open(self.source_filename, "w") as f:
        f.write(self.code)
      compile_process = subprocess.run(['gfortran', "-g", "-O0", self.source_filename, "-o", "TestCase_exec"] + (["-m32"] if self.arch==32 else []) + self.fflags.split() + self.ldflags.split(),universal_newlines=True)

    if compile_process.returncode!=0:
      self.errmsg += "COMPILATION FAILED:\n"+compile_process.stdout

  def run_code_in_vgdb(self):
    # in reverse mode, clear index and adjoints files
    if self.mode=='b':
      try:
        os.remove("dg-input-indices")
      except OSError:
        pass
      try:
        os.remove("dg-input-adjoints")
      except OSError:
        pass
      try:
        os.remove("dg-output-indices")
      except OSError:
        pass
      try:
        os.remove("dg-output-adjoints")
      except OSError:
        pass
    # logs are shown in case of failure
    self.valgrind_log = ""
    self.gdb_log = ""
    # start Valgrind and extract "target remote" line
    maybereverse = ["--record=rec"] if self.mode=='b' else []
    valgrind = subprocess.Popen(["../../install/bin/valgrind", "--tool=derivgrind", "--vgdb-error=0"]+maybereverse+["./TestCase_exec"], stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,universal_newlines=True,bufsize=0)
    while True:
      line = valgrind.stdout.readline()
      self.valgrind_log += line
      r = re.search(r"==\d+==\s*(target remote \| .*)$", line.strip())
      if r:
        target_remote_command = r.group(1)
        break
    # start GDB and connect to Valgrind
    gdb = subprocess.Popen(["gdb", "./TestCase_exec"],stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,universal_newlines=True,bufsize=0)
    gdb.stdin.write(target_remote_command+"\n")
    # set breakpoints and continue
    gdb.stdin.write("break "+self.source_filename+":"+str(self.line_before_stmt)+"\n")
    gdb.stdin.write("break "+self.source_filename+":"+str(self.line_after_stmt)+"\n")
    gdb.stdin.write("continue\n")
    # "mark" inputs with monitor commands
    if self.mode=='d': # forward mode, set dot values
      for var in self.dots:
        gdb.stdin.write("print &"+var+"\n")
        while True:
          line = gdb.stdout.readline()
          self.gdb_log += line
          r = re.search(r"\$\d+ = \("+self.type["gdbptype"]+"\) (0x[0-9a-f]+)\s?", line.strip())
          if r:
            pointer = r.group(1)
            gdb.stdin.write("monitor "+self.type["set"]+" "+pointer+" "+str(self.dots[var])+"\n")
            break
    elif self.mode=='b': # reverse mode, assign and record indices
      for var in self.test_bars:
        gdb.stdin.write("print &"+var+"\n")
        while True:
          line = gdb.stdout.readline()
          self.gdb_log += line
          r = re.search(r"\$\d+ = \("+self.type["gdbptype"]+"\) (0x[0-9a-f]+)\s?", line.strip())
          if r:
            pointer = r.group(1)
            break
        gdb.stdin.write("monitor mark "+pointer+"\n")
        while True:
          line = gdb.stdout.readline()
          self.gdb_log += line
          r = re.search(r"index: (\d+)", line.strip())
          if r:
            index = int(r.group(1))
            break
        with open("dg-input-indices","a") as f:
          f.writelines([str(index)+"\n"])
    # execute statement
    gdb.stdin.write("continue\n")
    # check values
    for var in self.test_vals:
      gdb.stdin.write("print "+var+"\n")
      while True:
        line = gdb.stdout.readline()
        self.gdb_log += line
        r = re.search(r"\$\d+ = ([0-9\.\-]+)$", line.strip())
        if r:
          computed_value = float(r.group(1))
          if abs(computed_value-self.test_vals[var]) > self.type["tol"]:
            self.errmsg += "VALUES DISAGREE: "+var+" stored="+str(self.test_vals[var])+" computed="+str(computed_value)+"\n"
          break
    # handle outputs with monitor commands
    if self.mode=='d': # forward mode, check dot values
      for var in self.test_dots:
        gdb.stdin.write("print &"+var+"\n")
        while True:
          line = gdb.stdout.readline()
          self.gdb_log += line
          r = re.search(r"\$\d+ = \("+self.type["gdbptype"]+"\) (0x[0-9a-f]+)\s?", line.strip())
          if r:
            pointer = r.group(1)
            gdb.stdin.write("monitor "+self.type["get"]+" "+pointer+"\n")
            break
        while True:
          line = gdb.stdout.readline()
          self.gdb_log += line
          r = re.search("dot value: ([0-9.\-]+)$", line.strip())
          if r:
            computed_dot = float(r.group(1))
            if abs(computed_dot-self.test_dots[var]) > self.type["tol"]:
              self.errmsg += "DOT VALUES DISAGREE: "+var+" stored="+str(self.test_dots[var])+" computed="+str(computed_dot)+"\n"
            break
    elif self.mode=='b': # reverse mode, record indices
      for var in self.bars:
        gdb.stdin.write("print &"+var+"\n")
        while True:
          line = gdb.stdout.readline()
          self.gdb_log += line
          r = re.search(r"\$\d+ = \("+self.type["gdbptype"]+"\) (0x[0-9a-f]+)\s?", line.strip())
          if r:
            pointer = r.group(1)
            break
        gdb.stdin.write("monitor index "+pointer+"\n")
        while True:
          line = gdb.stdout.readline()
          self.gdb_log += line
          r = re.search(r"index: (\d+)", line.strip())
          if r:
            index = int(r.group(1))
            break
        with open("dg-output-indices","a") as f:
          f.writelines([str(index)+"\n"])
    # finish execution of client program under Valgrind
    gdb.stdin.write("continue\n")
    gdb.stdin.write("quit\n")
    (stdout_data, stderr_data) = valgrind.communicate()
    self.valgrind_log += stdout_data
    (stdout_data, stderr_data) = gdb.communicate()
    self.gdb_log += stdout_data
    # for reverse mode, evaluate tape
    if self.mode=='b':
      with open("dg-output-adjoints","w") as outputadjoints:
        for var in self.bars:
          outputadjoints.writelines([str(self.bars[var])+"\n"])
      tape_evaluation = subprocess.run(["../../install/bin/tape-evaluation", "rec"])
      with open("dg-input-adjoints","r") as inputadjoints:
        for var in self.test_bars:
          bar = float(inputadjoints.readline())
          if bar < self.test_bars[var]-self.type["tol"] or bar > self.test_bars[var]+self.type["tol"]:
            self.errmsg += f"BAR VALUES DISAGREE: {var} stored={self.test_bars[var]} computed={bar}\n"


  def run(self):
    print("##### Running interactive test '"+self.name+"'... #####", flush=True)
    self.errmsg = ""
    if self.errmsg=="":
      self.produce_code()
    if self.errmsg=="":
      self.compile_code()
    if self.errmsg=="":
      self.run_code_in_vgdb()
    if self.errmsg=="":
      print("OK.\n")
      return True
    else:
      print("FAIL:")
      print(self.errmsg)
      print("VALGRIND LOG:")
      print(self.valgrind_log)
      print("GDB LOG:")
      print(self.gdb_log)
      return False
    

class ClientRequestTestCase(TestCase):
  """Methods to run a Derivgrind test case using Valgrind client requests."""
  def __init__(self,name):
    super().__init__(name)

  def produce_code(self):
    # Insert testcase data into C, Fortran or Python code template. 
    if self.compiler in ['gcc','g++','clang','clang++']:
      c = (self.compiler in ['gcc','clang'])
      self.code = "#include <stdio.h>\n" if c else "#include <iostream>\n"
      self.code += "#include <valgrind/derivgrind.h>\n"
      if self.mode=='b':
        self.code += "#include <valgrind/derivgrind-recording.h>\n"
      self.code += self.include + "\n"
      self.code += "int main(){\n  int ret=0;\n"
      if self.mode=='b':
        self.code += "DG_CLEARF;\n"
      self.code += "".join([f"  {self.type['ctype']} {var} = {self.vals[var]};\n" for var in self.vals]) 
      self.code += "  {\n"
      if self.mode=='d':
        self.code += "".join([f"    {self.type['ctype']} _derivative_of_{var} = {self.dots[var]}; VALGRIND_SET_DERIVATIVE(&{var},&_derivative_of_{var},{self.type['size']});\n" for var in self.dots])
      elif self.mode=='b':
        self.code += "".join([f"    DG_INPUTF({var});\n" for var in self.test_bars])
      self.code += "  }\n"
      self.code += "  " + self.stmt + "\n"
      self.code += "  {\n"
      # check values
      for var in self.test_vals:
        self.code += f"""    
          if({var} < {self.test_vals[var]-self.type["tol"]} 
            || {var} > {self.test_vals[var]+self.type["tol"]}) {{
        """
        if c:
          self.code += f"""
            printf("VALUES DISAGREE: {var} stored={self.type["format"]} computed={self.type["format"]}\\n",({self.type["ctype"]}){self.test_vals[var]},{var}); ret = 1; 
          """
        else:
          self.code += f"""
            std::cout << "VALUES DISAGREE: {var} stored=" << (({self.type["ctype"]}) {self.test_vals[var]}) << " computed=" << {var} << "\\n"; ret = 1;
          """
        self.code += f""" }} """
      if self.mode=='d':
        # check dot values
        for var in self.test_dots:
          self.code += f"""
            {self.type['ctype']} _derivative_of_{var} = 0.; 
            VALGRIND_GET_DERIVATIVE(&{var},&_derivative_of_{var},{self.type['size']});
            if(_derivative_of_{var} < {self.test_dots[var]-self.type["tol"]} 
                || _derivative_of_{var} > {self.test_dots[var]+self.type["tol"]}) {{
          """ 
          if c:
            self.code += f"""
              printf("DOT VALUES DISAGREE: {var} stored={self.type["format"]} computed={self.type["format"]}\\n",({self.type["ctype"]}){self.test_dots[var]}, _derivative_of_{var}); ret = 1; 
            """
          else:
            self.code += f"""
              std::cout << "DOT VALUES DISAGREE: {var} stored=" << (({self.type["ctype"]}) {self.test_dots[var]}) << " computed=" << _derivative_of_{var} << "\\n"; ret=1;
            """
          self.code += f""" }} """
      elif self.mode=='b':
        # register output variables
        for var in self.bars:
          self.code += f"DG_OUTPUTF({var});\n"
      self.code += "  }\n"
      self.code += "  return ret;\n}\n"
    elif self.compiler=='gfortran':
      self.code = """
        program main
        use derivgrind_clientrequests
        use, intrinsic :: iso_c_binding
        implicit none
        integer :: ret = 0
      """
      for var in self.vals:
        self.code += f"{self.type['ftype']}, target :: {var} = {str_fortran(self.vals[var])}\n"
      if self.mode=='b':
        self.code += "call dg_clearf()\n"
      for var in self.dots:
        if self.mode=='d':
          self.code += f"""
            block
            {self.type['ftype']}, target :: derivative_of_{var} = {str_fortran(self.dots[var])}
            call valgrind_set_derivative(c_loc({var}), c_loc(derivative_of_{var}), {self.type['size']})
            end block
          """
        elif self.mode=='b':
          self.code += f"call dg_inputf(c_loc({var}))\n"
      self.code += "block\n"
      self.code += self.stmt + "\n"
      # check values
      for var in self.test_vals:
        self.code += f"""
          if({var} < {str_fortran(self.test_vals[var]-self.type["tol"])} .or. {var} > {str_fortran(self.test_vals[var]+self.type["tol"])}) then
            print *, "VALUES DISAGREE: {var} stored=", {str_fortran(self.test_vals[var])}, " computed=", {var}
            ret = 1
          end if
        """
      # forward mode: check dot values of outputs / recording mode: register outputs
      for var in self.test_dots:
        if self.mode=='d':
          self.code += f"""
            block
            {self.type['ftype']}, target :: derivative_of_{var} = 0
            call valgrind_get_derivative(c_loc({var}), c_loc(derivative_of_{var}), {self.type['size']})
            if(derivative_of_{var} < {str_fortran(self.test_dots[var]-self.type["tol"])} .or. derivative_of_{var} > {str_fortran(self.test_dots[var]+self.type["tol"])}) then
              print *, "DOT VALUES DISAGREE: {var} stored=", {str_fortran(self.test_dots[var])}, " computed=", derivative_of_{var}
              ret = 1
            end if
            end block
          """
        elif self.mode=='b':
          self.code += f"call dg_outputf(c_loc({var}))\n"
      self.code += """
        end block
        call exit(ret)
        end program
      """
    elif self.compiler == "python":
      self.code = "import numpy as np\nimport derivgrind\nret = 0\n"
      if self.mode=='b':
        self.code += "derivgrind.clearf()\n"
      # NumPy tests perform the same calculation 16 times
      if self.type['pytype']=='float':
        self_vals = self.vals
        self_dots = self.dots
        self_test_vals = self.test_vals
        self_test_dots = self.test_dots
      elif self.type['pytype'] in ['np.float64', 'np.float32']:
        self_vals = { (var+'['+str(i)+']'):self.vals[var] for var in self.vals for i in range(16) }
        self_dots = { (var+'['+str(i)+']'):self.dots[var] for var in self.dots for i in range(16) }
        self_test_vals = { (var+'['+str(i)+']'):self.test_vals[var] for var in self.test_vals for i in range(16) }
        self_test_dots = { (var+'['+str(i)+']'):self.test_dots[var] for var in self.test_dots for i in range(16) }
        for var in self.vals:
          self.code += f"{var} = np.empty(16,dtype={self.type['pytype']})\n"
      for var in self_vals:
        self.code += f"{var} = {self_vals[var]}\n"
      for var in self_dots:
        if self.mode=='d':
          self.code += f"{var} = derivgrind.set_derivative({var}, {self_dots[var]})\n"
        elif self.mode=='b':
          self.code += f"{var} = derivgrind.inputf({var})\n"
      self.code += self.stmt + "\n"
      if self.mode=='d' and self.type['pytype'] in ['np.float64','np.float32']:
        for var in self.test_dots:
          self.code += f"derivative_of_{var} = np.empty(16,dtype={self.type['pytype']})\n"
      for var in self_test_vals:
        self.code += f'if {var} < {self_test_vals[var]-self.type["tol"]} or {var} > {self_test_vals[var]+self.type["tol"]}:\n'
        self.code += f'  print("VALUES DISAGREE: {var} stored=", {self_test_vals[var]}, "computed=", {var})\n'
        self.code +=  '  ret = 1\n' 
      if self.mode=='d':
        for var in self_test_dots:
          self.code += f'derivative_of_{var} = derivgrind.get_derivative({var})\n'
          self.code += f'if derivative_of_{var} < {self_test_dots[var]-self.type["tol"]} or derivative_of_{var} > {self_test_dots[var]+self.type["tol"]}:\n'
          self.code += f'  print("DOT VALUES DISAGREE: {var} stored=", {self_test_dots[var]}, "computed=", derivative_of_{var})\n'
          self.code +=  '  ret = 1\n' 
      elif self.mode=='b':
        for var in self_test_dots:
          self.code += f'derivgrind.outputf({var})\n'
      self.code += "exit(ret)\n"



  def compile_code(self):
    # write C/C++ code into file
    if self.compiler in ['gcc','clang']:
      self.source_filename = "TestCase_src.c"
    elif self.compiler in ['g++','clang++']:
      self.source_filename = "TestCase_src.cpp"
    elif self.compiler=='gfortran':
      self.source_filename = "TestCase_src.f90"
    elif self.compiler=='python':
      self.source_filename = "TestCase_src.py"
    with open(self.source_filename, "w") as f:
      f.write(self.code)
    if self.compiler in ['gcc','g++','clang','clang++']:
      compile_process = subprocess.run([self.compiler, "-O3", self.source_filename, "-o", "TestCase_exec", "-I../../install/include"] + (["-m32"] if self.arch==32 else []) + ( self.cflags_clang.split() if self.cflags_clang!=None and self.compiler in ['clang','clang++'] else self.cflags.split() ) + self.ldflags.split(),universal_newlines=True)
    elif self.compiler=='gfortran':
      compile_process = subprocess.run([self.compiler, "-O3", self.source_filename, "fortran/derivgrind_clientrequests.c", "-o", "TestCase_exec", "-I../../install/include", "-Ifortran"] + (["-m32"] if self.arch==32 else []) + self.fflags.split() ,universal_newlines=True)
    elif self.compiler=='python':
      pass

    if self.compiler!='python' and compile_process.returncode!=0:
      self.errmsg += "COMPILATION FAILED:\n"+compile_process.stdout

  def run_code(self):
    self.valgrind_log = ""
    environ = os.environ.copy()
    if self.compiler=='python':
      commands = ['python3', 'TestCase_src.py']
      if "PYTHONPATH" not in environ:
        environ["PYTHONPATH"]=""
      environ["PYTHONPATH"] += ":"+environ["PWD"]+"/python"
    else:
      commands = ['./TestCase_exec']
    maybereverse = ["--record=rec"] if self.mode=='b' else []
    valgrind = subprocess.run(["../../install/bin/valgrind", "--tool=derivgrind"]+maybereverse+commands,stdout=subprocess.PIPE,stderr=subprocess.PIPE,universal_newlines=True,env=environ)
    if valgrind.returncode!=0:
      self.errmsg +="VALGRIND STDOUT:\n"+valgrind.stdout+"\n\nVALGRIND STDERR:\n"+valgrind.stderr+"\n\n"
    if self.mode=='b': # evaluate tape
      with open("dg-output-adjoints","w") as outputadjoints:
        # NumPy testcases are repeated 16 times
        repetitions = 16 if self.compiler=='python' and self.type["pytype"] in ["np.float32","np.float64"] else 1
        for var in self.bars: # same order as in the client code
          for i in range(repetitions):
            print(str(self.bars[var]), file=outputadjoints)
      tape_evaluation = subprocess.run(["../../install/bin/tape-evaluation","rec"],env=environ)
      with open("dg-input-adjoints","r") as inputadjoints:
        for var in self.test_bars: # same order as in the client code
          for i in range(repetitions):
            bar = float(inputadjoints.readline())
            if bar < self.test_bars[var]-self.type["tol"] or bar > self.test_bars[var]+self.type["tol"]:
              self.errmsg += f"BAR VALUES DISAGREE: {var} stored={self.test_bars[var]} computed={bar}\n"
    

  def run(self):
    print("##### Running client request test '"+self.name+"'... #####", flush=True)
    self.errmsg = ""
    if self.errmsg=="":
      self.produce_code()
    if self.errmsg=="":
     self.compile_code()
    if self.errmsg=="":
      self.run_code()
    if self.errmsg=="":
      print("OK.\n")
      return True
    else:
      print("FAIL:")
      print(self.errmsg)
      return False




    




