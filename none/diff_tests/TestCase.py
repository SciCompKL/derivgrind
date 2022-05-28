import subprocess
import re
import time
import os

TYPE_DOUBLE = {"ctype":"double", "size":8, "tol":1e-8, "get":"get", "set":"set","format":"%.16lf"}
TYPE_FLOAT = {"ctype":"float", "size":4, "tol":1e-4, "get":"fget", "set":"fset","format":"%.9f"}

class TestCase:
  """Basic data for a DerivGrind test case."""
  def __init__(self, name):
    self.name = name # Name of TestCase
    self.stmt = "" # Code to be run in main function
    self.include = "" # Code pasted above main function
    self.vals = {} # Assigns values to input variables used by stmt
    self.grads = {} # Assigns gradients to input variables used by stmt
    self.test_vals = {} # Expected values of output variables computed by stmt
    self.test_grads = {} # Expected gradients of output variables computed by stmt
    self.timeout = 10 # Timeout for execution in Valgrind, in seconds
    self.cflags = "" # Additional flags for the C compiler
    self.ldflags = "" # Additional flags for the linker, e.g. "-lm"
    self.type = TYPE_DOUBLE # TYPE_DOUBLE or TYPE_FLOAT

class InteractiveTestCase(TestCase):
  """Methods to run a DerivGrind test case interactively in VGDB."""
  def __init__(self,name):
    super().__init__(name)

  def produce_c_code(self):
    # Insert testcase data into C code template. 
    # The template has two labels around the statement, in order 
    # to find the lines where breakpoints should be added.
    self.code = f"""
      {self.include}
      int main(){{
        { " ".join([self.type["ctype"]+" "+var+" = "+str(self.vals[var])+";" for var in self.vals]) }
        /*_testcase_before_stmt*/ __asm__("nop");
        {self.stmt};
        /*_testcase_after_stmt*/ __asm__("nop");
      }}
    """
    self.line_before_stmt = 0
    self.line_after_stmt = 0
    for i, line in enumerate(self.code.split("\n")):
      if line.strip().startswith("/*_testcase_before_stmt*/"):
        self.line_before_stmt = i+1
      if line.strip().startswith("/*_testcase_after_stmt*/"):
        self.line_after_stmt = i+1
    assert self.line_before_stmt != 0
    assert self.line_after_stmt != 0

  def compile_c_code(self):
    # write C code into file
    with open("TestCase_src.c", "w") as f:
      f.write(self.code)
    # query GCC for all optimization options, and find flags to disable the enabled ones
    blacklist = ["stack-protector-strong","no-threadsafe-statics"] # these should not be disabled (as this would cause errors)
    options = []
    compile_flags_process = subprocess.run(["gcc", "-Q", "--help=optimizers", "-g", "-O0", "-m32", "TestCase_src.c", "-o", "TestCase_exec"] + self.cflags.split() + self.ldflags.split(),stdout=subprocess.PIPE,stderr=subprocess.STDOUT,universal_newlines=True)
    for line in compile_flags_process.stdout.split("\n"):
      r = re.search(r"^\s*-f(\S+)\s*\[enabled\]", line)
      if r:
        if r.group(1) not in blacklist:
          options.append("-fno-"+r.group(1))
    # compile with disabled options
    compile_process = subprocess.run(["gcc", "-g", "-O0", "-m32", "TestCase_src.c", "-o", "TestCase_exec"] + options + self.cflags.split() + self.ldflags.split(),universal_newlines=True)
    if compile_process.returncode!=0:
      self.errmsg += "COMPILATION FAILED:\n"+compile_process.stdout

  def run_c_code_in_vgdb(self):
    self.valgrind_log = ""
    self.gdb_log = ""
    # start Valgrind and extract "target remote" line
    environ = os.environ.copy()
    if "LD_LIBRARY_PATH" not in environ:
      environ["LD_LIBRARY_PATH"]=""
    environ["LD_LIBRARY_PATH"] += ":"+environ["PWD"]+"/../libm-replacement/"
    valgrind = subprocess.Popen(["../../install/bin/valgrind", "--tool=none", "--vgdb-error=0", "./TestCase_exec"], stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,universal_newlines=True,bufsize=0,env=environ)
    time.sleep(1)
    while True:
      line = valgrind.stdout.readline()
      self.valgrind_log += line
      r = re.search(r"==\d+==\s*(target remote \| .*)$", line.strip())
      if r:
        target_remote_command = r.group(1)
        break
    # start GDB and connect to Valgrind
    gdb = subprocess.Popen(["gdb", "./TestCase_exec"],stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,universal_newlines=True,bufsize=0,env=environ)
    gdb.stdin.write(target_remote_command+"\n")
    # set breakpoints and continue
    gdb.stdin.write("break TestCase_src.c:"+str(self.line_before_stmt)+"\n")
    gdb.stdin.write("break TestCase_src.c:"+str(self.line_after_stmt)+"\n")
    gdb.stdin.write("continue\n")
    # set gradients and continue
    for var in self.grads:
      gdb.stdin.write("print &"+var+"\n")
      while True:
        line = gdb.stdout.readline()
        self.gdb_log += line
        r = re.search(r"\$\d+ = \("+self.type["ctype"]+" \*\) (0x[0-9a-f]+)$", line.strip())
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
        r = re.search(r"\$\d+ = \("+self.type["ctype"]+" \*\) (0x[0-9a-f]+)$", line.strip())
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
    self.errmsg = ""
    if self.errmsg=="":
      self.produce_c_code()
    if self.errmsg=="":
      self.compile_c_code()
    if self.errmsg=="":
      self.run_c_code_in_vgdb()
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

  def produce_c_code(self):
    # Insert testcase data into C code template. 
    self.code = "#include <stdio.h>\n"
    self.code += "#include <valgrind/derivgrind.h>\n"
    self.code += self.include + "\n"
    self.code += "int main(){\n  int ret=0;\n"
    self.code += "".join([f"  {self.type['ctype']} {var} = {self.vals[var]};\n" for var in self.vals]) 
    self.code += "  {\n"
    self.code += "".join([f"    {self.type['ctype']} _derivative_of_{var} = {self.grads[var]}; VALGRIND_SET_DERIVATIVE(&{var},&_derivative_of_{var},{self.type['size']});\n" for var in self.grads])
    self.code += "  }\n"
    self.code += "  " + self.stmt + "\n"
    self.code += "  {\n"
    self.code += "".join([f'    if({var} < {self.test_vals[var]-self.type["tol"]} || {var} > {self.test_vals[var]+self.type["tol"]}) {{ printf("VALUES DISAGREE: {var} stored={self.type["format"]} computed={self.type["format"]}\\n",({self.type["ctype"]}){self.test_vals[var]},{var}); ret = 1; }}\n' for var in self.test_vals])
    self.code += "".join([f"    {self.type['ctype']} _derivative_of_{var} = 0.; VALGRIND_GET_DERIVATIVE(&{var},&_derivative_of_{var},{self.type['size']});\n" for var in self.test_grads])
    self.code += "".join([f'    if(_derivative_of_{var} < {self.test_grads[var]-self.type["tol"]} || _derivative_of_{var} > {self.test_grads[var]+self.type["tol"]}) {{ printf("GRADIENTS DISAGREE: {var} stored={self.type["format"]} computed={self.type["format"]}\\n",({self.type["ctype"]}){self.test_grads[var]}, _derivative_of_{var}); ret = 1; }}\n' for var in self.test_grads])
    self.code += "  }\n"
    self.code += "  return ret;\n}\n"

  def compile_c_code(self):
    # write C code into file
    with open("TestCase_src.c", "w") as f:
      f.write(self.code)
    # compile 
    compile_process = subprocess.run(["gcc", "-g", "-O0", "-m32", "TestCase_src.c", "-o", "TestCase_exec", "-I../../install/include"] + self.cflags.split() + self.ldflags.split(),universal_newlines=True)
    if compile_process.returncode!=0:
      self.errmsg += "COMPILATION FAILED:\n"+compile_process.stdout

  def run_c_code(self):
    self.valgrind_log = ""
    environ = os.environ.copy()
    if "LD_LIBRARY_PATH" not in environ:
      environ["LD_LIBRARY_PATH"]=""
    environ["LD_LIBRARY_PATH"] += ":"+environ["PWD"]+"/../libm-replacement/"
    valgrind = subprocess.run(["../../install/bin/valgrind", "--tool=none", "./TestCase_exec"],stdout=subprocess.PIPE,stderr=subprocess.PIPE,universal_newlines=True,env=environ)
    if valgrind.returncode!=0:
      self.errmsg +="VALGRIND STDOUT:\n"+valgrind.stdout+"\n\nVALGRIND STDERR:\n"+valgrind.stderr+"\n\n"
    

  def run(self):
    print("##### Running client request test '"+self.name+"'... #####", flush=True)
    self.errmsg = ""
    if self.errmsg=="":
      self.produce_c_code()
    if self.errmsg=="":
     self.compile_c_code()
    if self.errmsg=="":
      self.run_c_code()
    if self.errmsg=="":
      print("OK.\n")
      return True
    else:
      print("FAIL:")
      print(self.errmsg)
      return False




    




