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

# Copy original libm.so.
cp_libm = subprocess.run(["cp", libmso_path, "libmoriginal.so"],universal_newlines=True)

# Produce source file of the library.
with open("math.c","w") as f:
  f.write("""
    #include "valgrind/derivgrind.h" // for client requests
    #include <dlfcn.h> // for dynamic loading
    #include <stdio.h> // for printing error messages
    #define LIBM(function) DG_MATH_ORIGINAL_##function
    void* libmoriginal; // handle to original libm
  """)
  # declare pointers to functions from the original libm
  for function in functions:
    f.write(function.declaration_original_pointer())
  # when the library is loaded, dynamically load 
  # the original libm and initialize these pointers
  f.write("""
  __attribute__((constructor)) void DG_MATH_init(void){
    void* libmoriginal = dlopen("libmoriginal.so",RTLD_LAZY);
    if(!libmoriginal) printf("Cannot load libmoriginal.so\\n");
  """)
  for function in functions:
    f.write(f"""LIBM({function.name}) = dlsym(libmoriginal, "{function.name}");\n""")
  f.write("}\n")
  # when the library is unloaded, don't unload the original libm
  # as this causes a segmentation fault 
  f.write("""
  __attribute__((destructor)) void DG_MATH_fini(void){
  }
  """)
  # define wrapped math functions   
  for function in functions:
    f.write(function.c_code())

# Compile wrapper libm.so.
compile_libm = subprocess.run(["gcc","math.c", "-o",libmso_name,"-shared","-fPIC","-Wl,--version-script","version-script","-m32","-I../../install/include","-ldl"], universal_newlines=True)

