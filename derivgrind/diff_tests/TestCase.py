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
import json
import numpy as np

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

install_dir = "" # Valgrind installation directory
temp_dir = "" # directory for temporary files produced by tests
codi_dir = "" # CoDiPack include directory for validation in performance tests

class TestCase:
  """Basic data for a Derivgrind regression test case."""
  def __init__(self, name):
    self.name = name # Name of TestCase
    self.mode = 'd' # 'd': dot/forward, 'b': bar/recording
    self.stmtd = None # Code to be run in C/C++ main function for double regression test
    self.stmtf = None # Code to be run in C/C++ main function for float regression test
    self.stmtl = None # Code to be run in C/C++ main function for long double regression test
    self.stmtr4 = None # Code to be run in Fortran program for real*4 regression test
    self.stmtr8 = None # Code to be run in Fortran program for real*8 regression test
    self.stmtp = None # Code to be run in Python regression test
    self.stmt = None # One out of stmtd, stmtf, stmtl will be copied here
    # if a testcase is meant only for a subset of the three datatypes only,
    # set the others stmtX to None
    self.benchmark = None # C++ file to be run for performance test
    self.benchmarkargs = "" # Arguments to C++ program for performance test
    self.benchmarkreps = 0 # Number of repetitions for performance test
    self.include = "" # Code pasted above main function
    self.vals = {} # Assigns values to input variables used by stmt
    self.dots = {} # Assigns dot values to input variables used by stmt
    self.bars = {} # Assigns bar values to output variables used by stmt
    self.test_vals = {} # Expected values of output variables computed by stmt
    self.test_dots = {} # Expected dot values of output variables computed by stmt
    self.test_bars = {} # Expected bar values of input variables computed by stmt
    self.cflags = "" # Additional flags for the C compiler
    self.cflags_clang = None # Additional flags for the C compiler, if clang is used
    self.fflags = "" # Additional flags for the Fortran compiler
    self.ldflags = "" # Additional flags for the linker, e.g. "-lm"
    self.type = TYPE_DOUBLE # TYPE_DOUBLE, TYPE_FLOAT, TYPE_LONG_DOUBLE (for C/C++), TYPE_REAL4, TYPE_REAL8 (for Fortran)
    self.arch = 32 # 32 bit (x86) or 64 bit (amd64)
    self.disable = lambda mode, arch, language, typename : False # if True, test will not be run
    self.compiler = "gcc" # gcc, g++, gfortran, python
    self.install_dir = install_dir # Valgrind installation directory
    self.temp_dir = temp_dir # directory of temporary files produced by tests
    self.codi_dir = codi_dir # CoDiPack include directory for validation in performance tests

class InteractiveTestCase(TestCase):
  """Methods to run a Derivgrind regression test case interactively in VGDB."""
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
      with open(self.temp_dir+"/"+self.source_filename, "w") as f:
        f.write(self.code)
      compile_process = subprocess.run(['gcc', "-g", "-O0", self.temp_dir+"/"+self.source_filename, "-o", self.temp_dir+"/TestCase_exec"] + (["-m32"] if self.arch==32 else []) + ( self.cflags_clang.split() if self.cflags_clang!=None and self.compiler=='clang' else self.cflags.split() ) + self.ldflags.split(),universal_newlines=True)
    elif self.compiler in ['g++','clang++']:
      self.source_filename = "TestCase_src.cpp"
      with open(self.temp_dir+"/"+self.source_filename, "w") as f:
        f.write(self.code)
      compile_process = subprocess.run(['g++', "-g", "-O0", self.temp_dir+"/"+self.source_filename, "-o", self.temp_dir+"/TestCase_exec"] + (["-m32"] if self.arch==32 else []) + ( self.cflags_clang.split() if self.cflags_clang!=None and self.compiler=='clang++' else self.cflags.split() ) + self.ldflags.split(),universal_newlines=True)
    elif self.compiler=='gfortran':
      self.source_filename = "TestCase_src.f90"
      with open(self.temp_dir+"/"+self.source_filename, "w") as f:
        f.write(self.code)
      compile_process = subprocess.run(['gfortran', "-g", "-O0", self.temp_dir+"/"+self.source_filename, "-o", self.temp_dir+"/TestCase_exec"] + (["-m32"] if self.arch==32 else []) + self.fflags.split() + self.ldflags.split(),universal_newlines=True)

    if compile_process.returncode!=0:
      self.errmsg += "COMPILATION FAILED:\n"+compile_process.stdout.decode("utf-8")

  def run_code_in_vgdb(self):
    # in reverse mode, clear index and adjoints files
    if self.mode=='b':
      try:
        os.remove(self.temp_dir+"/dg-input-indices")
      except OSError:
        pass
      try:
        os.remove(self.temp_dir+"/dg-input-adjoints")
      except OSError:
        pass
      try:
        os.remove(self.temp_dir+"/dg-output-indices")
      except OSError:
        pass
      try:
        os.remove(self.temp_dir+"/dg-output-adjoints")
      except OSError:
        pass
    # logs are shown in case of failure
    self.valgrind_log = ""
    self.gdb_log = ""
    # start Valgrind and extract "target remote" line
    maybereverse = ["--record="+self.temp_dir] if self.mode=='b' else []
    valgrind = subprocess.Popen([self.install_dir+"/bin/valgrind", "--tool=derivgrind", "--vgdb-error=0"]+maybereverse+[self.temp_dir+"/TestCase_exec"], stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,universal_newlines=True,bufsize=0)
    while True:
      line = valgrind.stdout.readline()
      self.valgrind_log += line
      r = re.search(r"==\d+==\s*(target remote \| .*)$", line.strip())
      if r:
        target_remote_command = r.group(1)
        break
    # start GDB and connect to Valgrind
    gdb = subprocess.Popen(["gdb", self.temp_dir+"/TestCase_exec"],stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,universal_newlines=True,bufsize=0)
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
        with open(self.temp_dir+"/dg-input-indices","a") as f:
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
        with open(self.temp_dir+"/dg-output-indices","a") as f:
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
      with open(self.temp_dir+"/dg-output-adjoints","w") as outputadjoints:
        for var in self.bars:
          outputadjoints.writelines([str(self.bars[var])+"\n"])
      tape_evaluation = subprocess.run([self.install_dir+"/bin/tape-evaluation", self.temp_dir])
      with open(self.temp_dir+"/dg-input-adjoints","r") as inputadjoints:
        for var in self.test_bars:
          bar = float(inputadjoints.readline())
          if bar < self.test_bars[var]-self.type["tol"] or bar > self.test_bars[var]+self.type["tol"]:
            self.errmsg += f"BAR VALUES DISAGREE: {var} stored={self.test_bars[var]} computed={bar}\n"


  def run(self):
    print("##### Running interactive regression test '"+self.name+"'... #####", flush=True)
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
  """Methods to run a Derivgrind regression test case using Valgrind client requests."""
  def __init__(self,name):
    super().__init__(name)

  def produce_code(self):
    # Insert testcase data into C, Fortran or Python code template. 
    if self.compiler in ['gcc','g++','clang','clang++']:
      c = (self.compiler in ['gcc','clang'])
      self.code = "#include <stdio.h>\n" if c else "#include <iostream>\n"
      self.code += "#include <valgrind/derivgrind.h>\n"
      self.code += self.include + "\n"
      self.code += "int main(){\n  int ret=0;\n"
      self.code += "".join([f"  {self.type['ctype']} {var} = {self.vals[var]};\n" for var in self.vals]) 
      self.code += "  {\n"
      if self.mode=='d':
        self.code += "".join([f"    {self.type['ctype']} _derivative_of_{var} = {self.dots[var]}; DG_SET_DOTVALUE(&{var},&_derivative_of_{var},{self.type['size']});\n" for var in self.dots])
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
            DG_GET_DOTVALUE(&{var},&_derivative_of_{var},{self.type['size']});
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
      for var in self.dots:
        if self.mode=='d':
          self.code += f"""
            block
            {self.type['ftype']}, target :: derivative_of_{var} = {str_fortran(self.dots[var])}
            call dg_set_dotvalue(c_loc({var}), c_loc(derivative_of_{var}), {self.type['size']})
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
            call dg_get_dotvalue(c_loc({var}), c_loc(derivative_of_{var}), {self.type['size']})
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
      self.code = "import numpy as np\nimport derivgrind as dg\nret = 0\n"
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
          self.code += f"{var} = dg.set_dotvalue({var}, {self_dots[var]})\n"
        elif self.mode=='b':
          self.code += f"{var} = dg.inputf({var})\n"
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
          self.code += f'derivative_of_{var} = dg.get_dotvalue({var})\n'
          self.code += f'if derivative_of_{var} < {self_test_dots[var]-self.type["tol"]} or derivative_of_{var} > {self_test_dots[var]+self.type["tol"]}:\n'
          self.code += f'  print("DOT VALUES DISAGREE: {var} stored=", {self_test_dots[var]}, "computed=", derivative_of_{var})\n'
          self.code +=  '  ret = 1\n' 
      elif self.mode=='b':
        for var in self_test_dots:
          self.code += f'dg.outputf({var})\n'
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
    with open(self.temp_dir+"/"+self.source_filename, "w") as f:
      f.write(self.code)
    if self.compiler in ['gcc','g++','clang','clang++']:
      compile_process = subprocess.run([self.compiler, "-O3", self.temp_dir+"/"+self.source_filename, "-o", self.temp_dir+"/TestCase_exec", f"-I{self.install_dir}/include"] + (["-m32"] if self.arch==32 else []) + ( self.cflags_clang.split() if self.cflags_clang!=None and self.compiler in ['clang','clang++'] else self.cflags.split() ) + self.ldflags.split(),universal_newlines=True)
    elif self.compiler=='gfortran':
      compile_process = subprocess.run([self.compiler, "-O3", self.temp_dir+"/"+self.source_filename, "-o", self.temp_dir+"/TestCase_exec", f"-I{self.install_dir}/include/valgrind", f"-L{self.install_dir}/lib/valgrind", f"-lderivgrind_clientrequests-{'x86' if self.arch==32 else 'amd64'}"] + (["-m32"] if self.arch==32 else []) + self.fflags.split() ,universal_newlines=True)
    elif self.compiler=='python':
      pass

    if self.compiler!='python' and compile_process.returncode!=0:
      self.errmsg += "COMPILATION FAILED:\n"+compile_process.stdout.decode("utf-8")

  def run_code(self):
    self.valgrind_log = ""
    environ = os.environ.copy()
    if self.compiler=='python':
      commands = ["python3", self.temp_dir+"/TestCase_src.py"]
      if "PYTHONPATH" not in environ:
        environ["PYTHONPATH"]=""
      environ["PYTHONPATH"] += ":"+self.install_dir+"/lib/python3/site-packages"
    else:
      commands = [self.temp_dir+"/TestCase_exec"]
    maybereverse = ["--record="+self.temp_dir] if self.mode=='b' else []
    valgrind = subprocess.run([self.install_dir+"/bin/valgrind", "--tool=derivgrind"]+maybereverse+commands,stdout=subprocess.PIPE,stderr=subprocess.PIPE,universal_newlines=True,env=environ)
    if valgrind.returncode!=0:
      self.errmsg +="VALGRIND STDOUT:\n"+valgrind.stdout.decode("utf-8")+"\n\nVALGRIND STDERR:\n"+valgrind.stderr.decode("utf-8")+"\n\n"
    if self.mode=='b': # evaluate tape
      with open(self.temp_dir+"/dg-output-adjoints","w") as outputadjoints:
        # NumPy testcases are repeated 16 times
        repetitions = 16 if self.compiler=='python' and self.type["pytype"] in ["np.float32","np.float64"] else 1
        for var in self.bars: # same order as in the client code
          for i in range(repetitions):
            print(str(self.bars[var]), file=outputadjoints)
      tape_evaluation = subprocess.run([self.install_dir+"/bin/tape-evaluation",self.temp_dir],env=environ)
      with open(self.temp_dir+"/dg-input-adjoints","r") as inputadjoints:
        for var in self.test_bars: # same order as in the client code
          for i in range(repetitions):
            bar = float(inputadjoints.readline())
            if bar < self.test_bars[var]-self.type["tol"] or bar > self.test_bars[var]+self.type["tol"]:
              self.errmsg += f"BAR VALUES DISAGREE: {var} stored={self.test_bars[var]} computed={bar}\n"
    

  def run(self):
    print("##### Running client request regression test '"+self.name+"'... #####", flush=True)
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




class PerformanceTestCase(TestCase):
  """Methods to run a Derivgrind performance test case, using Valgrind client requests."""
  def __init__(self,name):
    super().__init__(name)

  def runCoDi(self):
    """Build with CoDiPack types and run."""
    if self.codi_dir==None:
      self.errmsg += "NO CODIPACK INCLUDE PATH SUPPLIED\n"
      return 
    comp = subprocess.run(["g++", self.benchmark, "-o", f"{self.temp_dir}/main_codi", "-DCODI_DOT" if self.mode=='d' else "-DCODI_BAR"]+self.cflags.split()+[f"-I{self.codi_dir}"] + (["-m32"] if self.arch==32 else []), capture_output=True)
    if comp.returncode!=0:
      self.errmsg += "COMPILATION WITH CODIPACK FAILED:\n" + comp.stdout.decode('utf-8')+ comp.stderr.decode('utf-8')
    exe = subprocess.run([f"{self.temp_dir}/main_codi",f"{self.temp_dir}/dg-performance-result-codi.json"]+self.benchmarkargs.split(), capture_output=True)
    if exe.returncode!=0:
      self.errmsg += "EXECUTION WITH CODIPACK FAILED:\n" + "STDOUT:\n" + exe.stdout.decode("utf-8") + "\nSTDERR:\n" + exe.stderr.decode("utf-8")
    with open(self.temp_dir+"/dg-performance-result-codi.json") as f:
      self.result_codi = json.load(f)

  def runNoAD(self, nrep):
    """Build without AD and run repeatedly."""
    comp = subprocess.run(["g++", self.benchmark, "-o", f"{self.temp_dir}/main_noad"]+self.cflags.split()+(["-m32"] if self.arch==32 else []), capture_output=True)
    if comp.returncode!=0:
      self.errmsg += "COMPILATION WITHOUT AD FAILED:\n" + comp.stdout.decode('utf-8') + comp.stderr.decode('utf-8')
    self.results_noad = []
    for irep in range(nrep):
      exe = subprocess.run(["/usr/bin/time", "-f", "time_output %e %M", f"{self.temp_dir}/main_noad", f"{self.temp_dir}/dg-performance-result-noad.json"]+self.benchmarkargs.split(), capture_output=True)
      if exe.returncode!=0:
        self.errmsg += "EXECUTION WITHOUT AD FAILED:\n" + "STDOUT:\n" + exe.stdout.decode('utf-8') + "\nSTDERR:\n" + exe.stderr.decode('utf-8')
      with open(self.temp_dir+"/dg-performance-result-noad.json") as f:
        result = json.load(f)
      for line in exe.stderr.decode('utf-8').split("\n"):
        line = line.strip().split()
        if len(line)>=3 and line[0]=="time_output":
          result["forward_outer_time_in_s"] = float(line[1])
          result["forward_outer_maxrss_in_kb"] = float(line[2])
          break
      else:
        self.errmsg += "EXECUTION WITHOUT AD FAILED: NO GNU TIME OUTPUT\n"
      self.results_noad.append(result)

  def runDG(self, nrep):
    """Build with Derivgrind client request types and run repeatedly."""
    comp = subprocess.run(["g++", self.benchmark, "-o", f"{self.temp_dir}/main_dg", f"-I{self.install_dir}/include", "-DDG_DOT" if self.mode=='d' else "-DDG_BAR"]+self.cflags.split() + (["-m32"] if self.arch==32 else []), capture_output=True)
    if comp.returncode!=0:
      self.errmsg += "COMPILATION WITH DERIVGRIND FAILED:\n" + comp.stderr.decode('utf-8')
    self.results_dg = []
    for irep in range(nrep):
      maybereverse = ["--record="+self.temp_dir] if self.mode=='b' else []
      exe = subprocess.run(["/usr/bin/time", "-f", "time_output %e %M", self.install_dir+"/bin/valgrind", "--tool=derivgrind"]+maybereverse+[f"{self.temp_dir}/main_dg", f"{self.temp_dir}/dg-performance-result-dg.json"]+self.benchmarkargs.split(), capture_output=True)
      if exe.returncode!=0:
        self.errmsg += "EXECUTION WITH DERIVGRIND FAILED:\n" + "STDOUT:\n" + exe.stdout.decode('utf-8') + "\nSTDERR:\n" + exe.stderr.decode('utf-8')
      with open(self.temp_dir+"/dg-performance-result-dg.json") as f:
        result = json.load(f)
      for line in exe.stderr.decode('utf-8').split("\n"):
        line = line.strip().split()
        if len(line)>=3 and line[0]=="time_output":
          result["forward_outer_time_in_s"] = float(line[1])
          result["forward_outer_maxrss_in_kb"] = float(line[2])
          break
      else:
        self.errmsg += "EXECUTION WITH DERIVGRIND FAILED: NO GNU TIME OUTPUT\n"
      if self.mode=='b':
        with open(self.temp_dir+"/dg-output-indices", "r") as f:
          number_of_outputs = len(f.readlines())
        with open(self.temp_dir+"/dg-output-adjoints", "w") as f:
          f.writelines(["1.0\n"]*number_of_outputs)
        eva = subprocess.run([self.install_dir+"/bin/tape-evaluation", self.temp_dir], capture_output=True)
        if eva.returncode!=0:
          self.errmsg += "EVALUATION OF DERIVGRIND TAPE FAILED:\n" + "STDOUT:\n" + eva.stdout.decode('utf-8') + "\nSTDERR:\n" + eva.stderr.decode('utf-8')
        result["input_bar"] = [float(adj) for adj in np.loadtxt(self.temp_dir+"/dg-input-adjoints")]
        # run tape-evaluation another time for statistics
        eva = subprocess.run([self.install_dir+"/bin/tape-evaluation", self.temp_dir, "--stats"], capture_output=True)
        if eva.returncode!=0:
          self.errmsg += "EVALUATION OF DERIVGRIND TAPE STATS FAILED:\n" + "STDOUT:\n" + eva.stdout.decode('utf-8') + "\nSTDERR:\n" + eva.stderr.decode('utf-8')
        nZero,nOne,nTwo = [int(n) for n in eva.stdout.decode('utf-8').strip().split()]
        result["number_of_jacobians"] = nOne + 2*nTwo
      self.results_dg.append(result)

  def verifyGradient(self):
    """Check Derivgrind results against CoDiPack result."""
    correct = True
    for result_dg in self.results_dg:
      if self.mode=='b':
        input_bar_dg = np.array(result_dg["input_bar"])
        input_bar_codi = np.array(self.result_codi["input_bar"])
        err = np.linalg.norm( input_bar_dg - input_bar_codi ) / ( np.linalg.norm(input_bar_codi) * np.sqrt(len(input_bar_codi)) )
      else:
        output_dot_dg = np.array(result_dg["output_dot"])
        output_dot_codi = np.array(self.result_codi["output_dot"])
        err = np.linalg.norm( output_dot_dg - output_dot_codi) / ( np.linalg.norm(output_dot_codi) * np.sqrt(len(output_dot_codi)) )
      correct = correct and err < self.type["tol"]
    return correct  

  def averagePerformance(self):
    """Average no-AD and Derivgrind runtime and memory performances."""
    noad_forward_time_in_s = np.mean([res["forward_time_in_s"] for res in self.results_noad])
    noad_forward_vmhwm_in_kb = np.mean([res["forward_vmhwm_in_kb"] for res in self.results_noad])
    noad_forward_outer_time_in_s = np.mean([res["forward_outer_time_in_s"] for res in self.results_noad])
    noad_forward_outer_maxrss_in_kb = np.mean([res["forward_outer_maxrss_in_kb"] for res in self.results_noad])
    dg_forward_time_in_s = np.mean([res["forward_time_in_s"] for res in self.results_dg])
    dg_forward_vmhwm_in_kb = np.mean([res["forward_vmhwm_in_kb"] for res in self.results_dg])
    dg_forward_outer_time_in_s = np.mean([res["forward_outer_time_in_s"] for res in self.results_dg])
    dg_forward_outer_maxrss_in_kb = np.mean([res["forward_outer_maxrss_in_kb"] for res in self.results_dg])
    if self.mode=='b':
      codi_number_of_jacobians = self.result_codi["number_of_jacobians"]
      dg_number_of_jacobians = np.mean([res["number_of_jacobians"] for res in self.results_dg])
    else:
      codi_number_of_jacobians = 0
      dg_number_of_jacobians = 0
    # Choose which output you prefer
    #return f"{noad_forward_time_in_s} {noad_forward_vmhwm_in_kb} {dg_forward_time_in_s} {dg_forward_vmhwm_in_kb}"
    #return f"{int(dg_forward_time_in_s / noad_forward_time_in_s)}x"
    return f"{int(dg_forward_time_in_s / noad_forward_time_in_s)} {noad_forward_time_in_s} {noad_forward_vmhwm_in_kb} {dg_forward_time_in_s} {dg_forward_vmhwm_in_kb} {codi_number_of_jacobians} {dg_number_of_jacobians} {noad_forward_outer_time_in_s} {noad_forward_outer_maxrss_in_kb} {dg_forward_outer_time_in_s} {dg_forward_outer_maxrss_in_kb}"

  def run(self):
    print("##### Running performance test '"+self.name+"'... #####", flush=True)
    self.errmsg = ""
    if self.errmsg=="":
      self.runCoDi()
    if self.errmsg=="":
      self.runNoAD(self.benchmarkreps)
    if self.errmsg=="":
      self.runDG(self.benchmarkreps)
    if self.errmsg=="":
      if not self.verifyGradient():
        self.errmsg="DERIVATIVES DISAGREE\n"
    if self.errmsg=="":
      print("OK.", self.averagePerformance())
      return True
    else:
      print("FAIL:")
      print(self.errmsg)
      return False

