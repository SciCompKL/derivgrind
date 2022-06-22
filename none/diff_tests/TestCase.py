import subprocess
import re
import time
import os

# Type information: 
# ctype / ftype: Keyword in the C/C++/Fortran source
# gdbptype: Regex to recognize pointer to variable in GDB output
# size: size in bytes
# tol: tolerance of the tests
# get / set: DerivGrind monitor commands
# format: C printf format specifier

# C/C++ types
TYPE_DOUBLE = {"ctype":"double", "gdbptype":"double \*", "size":8, "tol":1e-8, "get":"get", "set":"set","format":"%.16lf"}
TYPE_FLOAT = {"ctype":"float", "gdbptype":"float \*", "size":4, "tol":1e-4, "get":"fget", "set":"fset","format":"%.9f"}
TYPE_LONG_DOUBLE = {"ctype":"long double", "gdbptype":"long double \*", "size":"sizeof(long double)", "tol":1e-8, "get":"lget", "set":"lset","format":"%.16Lf"}
# Fortran types
TYPE_REAL4 = {"ftype":"real", "gdbptype":"PTR TO -> \( real\(kind=4\) \)", "size":4, "tol":1e-4, "get":"fget", "set":"fset","format":"%.9f"}
TYPE_REAL8 = {"ftype":"double precision", "gdbptype":"PTR TO -> \( real\(kind=8\) \)", "size":8, "tol":1e-8, "get":"get", "set":"set","format":"%.9f"}

class TestCase:
  """Basic data for a DerivGrind test case."""
  def __init__(self, name):
    self.name = name # Name of TestCase
    self.stmtd = None # Code to be run in C/C++ main function for double test
    self.stmtf = None # Code to be run in C/C++ main function for float test
    self.stmtl = None # Code to be run in C/C++ main function for long double test
    self.stmtr4 = None # Code to be run in Fortran program for real*4 test
    self.stmtr8 = None # Code to be run in Fortran program for real*8 test
    self.stmt = None # One out of stmtd, stmtf, stmtl will be copied here
    # if a testcase is meant only for a subset of the three datatypes only,
    # set the others stmtX to None
    self.include = "" # Code pasted above main function
    self.vals = {} # Assigns values to input variables used by stmt
    self.grads = {} # Assigns gradients to input variables used by stmt
    self.test_vals = {} # Expected values of output variables computed by stmt
    self.test_grads = {} # Expected gradients of output variables computed by stmt
    self.timeout = 10 # Timeout for execution in Valgrind, in seconds
    self.cflags = "" # Additional flags for the C compiler
    self.fflags = "" # Additional flags for the Fortran compiler
    self.ldflags = "" # Additional flags for the linker, e.g. "-lm"
    self.type = TYPE_DOUBLE # TYPE_DOUBLE, TYPE_FLOAT, TYPE_LONG_DOUBLE (for C/C++), TYPE_REAL4, TYPE_REAL8 (for Fortran)
    self.arch = 32 # 32 bit (x86) or 64 bit (amd64)
    self.disabled = False # if True, test will not be run
    self.compiler = "gcc" # gcc, g++ or gfortran
    self.only_language = None # define if test works for one particular language only

class InteractiveTestCase(TestCase):
  """Methods to run a DerivGrind test case interactively in VGDB."""
  def __init__(self,name):
    super().__init__(name)

  def produce_code(self):
    # Insert testcase data into C or Fortran code template. 
    # The template has two labels around the statement, in order 
    # to find the lines where breakpoints should be added.
    self.line_before_stmt = 0
    self.line_after_stmt = 0
    if self.compiler=='gcc' or self.compiler=='g++':
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
    if self.compiler=='gcc':
      self.source_filename = "TestCase_src.c"
      with open(self.source_filename, "w") as f:
        f.write(self.code)
      compile_process = subprocess.run(['gcc', "-g", "-O0", self.source_filename, "-o", "TestCase_exec"] + (["-m32"] if self.arch==32 else []) + self.cflags.split() + self.ldflags.split(),universal_newlines=True)
    elif self.compiler=='g++':
      self.source_filename = "TestCase_src.cpp"
      with open(self.source_filename, "w") as f:
        f.write(self.code)
      compile_process = subprocess.run(['g++', "-g", "-O0", self.source_filename, "-o", "TestCase_exec"] + (["-m32"] if self.arch==32 else []) + self.cflags.split() + self.ldflags.split(),universal_newlines=True)
    elif self.compiler=='gfortran':
      self.source_filename = "TestCase_src.f90"
      with open(self.source_filename, "w") as f:
        f.write(self.code)
      compile_process = subprocess.run(['gfortran', "-g", "-O0", self.source_filename, "-o", "TestCase_exec"] + (["-m32"] if self.arch==32 else []) + self.fflags.split() + self.ldflags.split(),universal_newlines=True)

    if compile_process.returncode!=0:
      self.errmsg += "COMPILATION FAILED:\n"+compile_process.stdout

  def run_code_in_vgdb(self):
    self.valgrind_log = ""
    self.gdb_log = ""
    # start Valgrind and extract "target remote" line
    environ = os.environ.copy()
    if "LD_LIBRARY_PATH" not in environ:
      environ["LD_LIBRARY_PATH"]=""
    environ["LD_LIBRARY_PATH"] += ":"+environ["PWD"]+"/../libm-extension/lib"+str(self.arch)+"/"
    if "LD_PRELOAD" not in environ:
      environ["LD_PRELOAD"]=""
    environ["LD_PRELOAD"] += ":"+environ["PWD"]+"/../libm-extension/lib"+str(self.arch)+"/libmextension.so"
    valgrind = subprocess.Popen(["../../install/bin/valgrind", "--tool=none", "--vgdb-error=0", "./TestCase_exec"], stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,universal_newlines=True,bufsize=0,env=environ)
    while True:
      line = valgrind.stdout.readline()
      self.valgrind_log += line
      r = re.search(r"==\d+==\s*(target remote \| .*)$", line.strip())
      if r:
        target_remote_command = r.group(1)
        break
    # start GDB and connect to Valgrind
    # Don't use the above environment, because GDB should see the normal libm.
    gdb = subprocess.Popen(["gdb", "./TestCase_exec"],stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,universal_newlines=True,bufsize=0)
    gdb.stdin.write(target_remote_command+"\n")
    # set breakpoints and continue
    gdb.stdin.write("break "+self.source_filename+":"+str(self.line_before_stmt)+"\n")
    gdb.stdin.write("break "+self.source_filename+":"+str(self.line_after_stmt)+"\n")
    gdb.stdin.write("continue\n")
    # set gradients and continue
    for var in self.grads:
      gdb.stdin.write("print &"+var+"\n")
      while True:
        line = gdb.stdout.readline()
        self.gdb_log += line
        r = re.search(r"\$\d+ = \("+self.type["gdbptype"]+"\) (0x[0-9a-f]+)\s?", line.strip())
        if r:
          pointer = r.group(1)
          gdb.stdin.write("monitor "+self.type["set"]+" "+pointer+" "+str(self.grads[var])+"\n")
          break
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
    # check gradients
    for var in self.test_grads:
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
        r = re.search("Derivative: ([0-9.\-]+)$", line.strip())
        if r:
          computed_gradient = float(r.group(1))
          if abs(computed_gradient-self.test_grads[var]) > self.type["tol"]:
            self.errmsg += "GRADIENTS DISAGREE: "+var+" stored="+str(self.test_grads[var])+" computed="+str(computed_gradient)+"\n"
          break
    # finish
    gdb.stdin.write("continue\n")
    gdb.stdin.write("quit\n")
    (stdout_data, stderr_data) = valgrind.communicate()
    self.valgrind_log += stdout_data
    (stdout_data, stderr_data) = gdb.communicate()
    self.gdb_log += stdout_data

  def run(self):
    print("##### Running interactive test '"+self.name+"'... #####", flush=True)
    if self.disabled:
      print("DISABLED.\n")
      return True
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
  """Methods to run a DerivGrind test case using Valgrind client requests."""
  def __init__(self,name):
    super().__init__(name)

  def produce_code(self):
    # Insert testcase data into C or Fortran code template. 
    if self.compiler=='gcc' or self.compiler=='g++':
      gcc = self.compiler=='gcc'
      self.code = "#include <stdio.h>\n" if gcc else "#include <iostream>\n"
      self.code += "#include <valgrind/derivgrind.h>\n"
      self.code += self.include + "\n"
      self.code += "int main(){\n  int ret=0;\n"
      self.code += "".join([f"  {self.type['ctype']} {var} = {self.vals[var]};\n" for var in self.vals]) 
      self.code += "  {\n"
      self.code += "".join([f"    {self.type['ctype']} _derivative_of_{var} = {self.grads[var]}; VALGRIND_SET_DERIVATIVE(&{var},&_derivative_of_{var},{self.type['size']});\n" for var in self.grads])
      self.code += "  }\n"
      self.code += "  " + self.stmt + "\n"
      self.code += "  {\n"
      # check values
      for var in self.test_vals:
        self.code += f"""    
          if({var} < {self.test_vals[var]-self.type["tol"]} 
            || {var} > {self.test_vals[var]+self.type["tol"]}) {{
        """
        if gcc:
          self.code += f"""
            printf("VALUES DISAGREE: {var} stored={self.type["format"]} computed={self.type["format"]}\\n",({self.type["ctype"]}){self.test_vals[var]},{var}); ret = 1; 
          """
        else:
          self.code += f"""
            std::cout << "VALUES DISAGREE: {var} stored=" << (({self.type["ctype"]}) {self.test_vals[var]}) << " computed=" << {var} << "\\n"; ret = 1;
          """
        self.code += f""" }} """
      # check gradients
      for var in self.test_grads:
        self.code += f"""
          {self.type['ctype']} _derivative_of_{var} = 0.; 
          VALGRIND_GET_DERIVATIVE(&{var},&_derivative_of_{var},{self.type['size']});
          if(_derivative_of_{var} < {self.test_grads[var]-self.type["tol"]} 
              || _derivative_of_{var} > {self.test_grads[var]+self.type["tol"]}) {{
        """ 
        if gcc:
          self.code += f"""
            printf("GRADIENTS DISAGREE: {var} stored={self.type["format"]} computed={self.type["format"]}\\n",({self.type["ctype"]}){self.test_grads[var]}, _derivative_of_{var}); ret = 1; 
          """
        else:
          self.code += f"""
            std::cout << "GRADIENTS DISAGREE: {var} stored=" << (({self.type["ctype"]}) {self.test_grads[var]}) << " computed=" << _derivative_of_{var} << "\\n"; ret=1;
          """
        self.code += f""" }} """
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
        self.code += f"{self.type['ftype']}, target :: {var} = {self.vals[var]}\n"
      for var in self.grads:
        self.code += f"""
          block
          {self.type['ftype']}, target :: derivative_of_{var} = {self.grads[var]}
          call valgrind_set_derivative(c_loc({var}), c_loc(derivative_of_{var}), {self.type['size']})
          end block
        """
      self.code += "block\n"
      self.code += self.stmt + "\n"
      def str_fortran(d):
        """Convert d to Fortran double precision literal."""
        s = str(d)
        if "e" in s:
          return s.replace("e","d")
        else:
          return s+"d0"
      # check values
      for var in self.test_vals:
        self.code += f"""
          if({var} < {str_fortran(self.test_vals[var]-self.type["tol"])} .or. {var} > {str_fortran(self.test_vals[var]+self.type["tol"])}) then
            print *, "VALUES DISAGREE: {var} stored=", {str_fortran(self.test_vals[var])}, " computed=", {var}
            ret = 1
          end if
        """
      # check gradients
      for var in self.test_grads:
        self.code += f"""
          block
          {self.type['ftype']}, target :: derivative_of_{var} = 0
          call valgrind_get_derivative(c_loc({var}), c_loc(derivative_of_{var}), {self.type['size']})
          if(derivative_of_{var} < {str_fortran(self.test_grads[var]-self.type["tol"])} .or. derivative_of_{var} > {str_fortran(self.test_grads[var]+self.type["tol"])}) then
            print *, "GRADIENTS DISAGREE: {var} stored=", {str_fortran(self.test_grads[var])}, " computed=", derivative_of_{var}
            ret = 1
          end if
          end block
        """
      self.code += """
        end block
        call exit(ret)
        end program
      """

  def compile_code(self):
    # write C/C++ code into file
    if self.compiler=='gcc':
      self.source_filename = "TestCase_src.c"
    elif self.compiler=='g++':
      self.source_filename = "TestCase_src.cpp"
    elif self.compiler=='gfortran':
      self.source_filename = "TestCase_src.f90"
    with open(self.source_filename, "w") as f:
      f.write(self.code)
    if self.compiler=='gcc' or self.compiler=='g++':
      compile_process = subprocess.run([self.compiler, "-O3", self.source_filename, "-o", "TestCase_exec", "-I../../install/include"] + (["-m32"] if self.arch==32 else []) + self.cflags.split() + self.ldflags.split(),universal_newlines=True)
    elif self.compiler=='gfortran':
      compile_process = subprocess.run([self.compiler, "-O3", self.source_filename, "derivgrind_clientrequests.c", "-o", "TestCase_exec", "-I../../install/include"] + (["-m32"] if self.arch==32 else []) ,universal_newlines=True)

    if compile_process.returncode!=0:
      self.errmsg += "COMPILATION FAILED:\n"+compile_process.stdout

  def run_code(self):
    self.valgrind_log = ""
    environ = os.environ.copy()
    if "LD_LIBRARY_PATH" not in environ:
      environ["LD_LIBRARY_PATH"]=""
    environ["LD_LIBRARY_PATH"] += ":"+environ["PWD"]+"/../libm-extension/lib"+str(self.arch)+"/"
    if "LD_PRELOAD" not in environ:
      environ["LD_PRELOAD"]=""
    environ["LD_PRELOAD"] += ":"+environ["PWD"]+"/../libm-extension/lib"+str(self.arch)+"/libmextension.so"
    valgrind = subprocess.run(["../../install/bin/valgrind", "--tool=none", "./TestCase_exec"],stdout=subprocess.PIPE,stderr=subprocess.PIPE,universal_newlines=True,env=environ)
    if valgrind.returncode!=0:
      self.errmsg +="VALGRIND STDOUT:\n"+valgrind.stdout+"\n\nVALGRIND STDERR:\n"+valgrind.stderr+"\n\n"
    

  def run(self):
    print("##### Running client request test '"+self.name+"'... #####", flush=True)
    if self.disabled:
      print("DISABLED.\n")
      return True
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




    




