import subprocess
import re
from overloaded_functions import functions

# Sample program to find libm.so.
compile_sample = subprocess.run(["gcc", "sample.c", "-o", "sample", "-m32", "-lm"],universal_newlines=True)
if compile_sample.returncode!=0:
  raise Exception("Couldn't compile sample C program.")

# Use ldd to find path and filename of libm.so.N.
ldd_sample = subprocess.run(["ldd", "sample"], stdout=subprocess.PIPE,universal_newlines=True)
libmso_name = None
libmso_path = None
for line in ldd_sample.stdout.split("\n"):
  r = re.search(r"^(libm\.so\S*)\s*=>\s*(\S*)", line.strip())
  if r:
    libmso_name = r.group(1)
    libmso_path = r.group(2)
    break
if libmso_name==None or libmso_path==None:
  raise Exception("Couldn't find libm.so with ldd.")

# Obtain GLIBC versions for all of these functions.
nm = subprocess.run(["nm","-D","--with-symbol-versions",libmso_path],stdout=subprocess.PIPE,universal_newlines=True)
for line in nm.stdout.split("\n"):
  for function in functions:
    r = re.search("\s"+function.name+"@@+(GLIBC_[0-9\._]*\s*$)", line.strip())
    if r:
      function.glibc_version = r.group(1)

# Produce version script.
glibc_versions_inv = {}
for function in functions:
  glibc_version = function.glibc_version
  if glibc_version not in glibc_versions_inv:
    glibc_versions_inv[glibc_version] = []
  glibc_versions_inv[glibc_version].append(function.name)
version_script = ""
for glibc_version in glibc_versions_inv:
  version_script += glibc_version + " { " + "".join([function_name + "; " for function_name in glibc_versions_inv]) + " } ; \n"
with open("version-script", "w") as f:
  f.write(version_script)

# Produce list of symbols that need to be renamed.
with open("redefine-syms","w") as f:
  for function in functions:
    f.write(function.name+" LIBM_ORIGINAL_"+function.name+"\n")

# Copy libm.so and rename certain symbols.
adapt_libm = subprocess.run(["objcopy", "--redefine-syms", "redefine-syms", libmso_path, "libmoriginal.so"],universal_newlines=True)

# Produce header file declaring the renamed symbols.
with open("mathoriginal.h","w") as f:
  f.write("#define LIBM(name) LIBM_ORIGINAL_##name \n")
  for function in functions:
    f.write(function.declaration_of_original())

# Produce source file defining the wrapped math symbols.
with open("math.c","w") as f:
  f.write('#include "mathoriginal.h"\n')
  f.write('#include "valgrind/derivgrind.h"\n')
  for function in functions:
    f.write(function.c_code())

# Compile wrapper libm.so.
compile_libm = subprocess.run(["gcc","math.c", "-o",libmso_name,"-L.","-lmoriginal","-shared","-fPIC","-Wl,--version-script","version-script","-m32","-I../../install/include"], universal_newlines=True)

