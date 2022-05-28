import subprocess
import re

# Sample program to find libm.so and GLIBC versions.
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

# Parse math.c for all function names.
glibc_versions = {} # store them as keys here
with open('math.c','r') as f:
  for line in f.readlines():
    r = re.search(r"^DERIVGRIND_MATH_FUNCTION[2ip]*\(([a-zA-Z0-9_]+),", line.strip())
    if r:
      glibc_versions[r.group(1)] = None

# Obtain GLIBC versions for all of these functions.
nm = subprocess.run(["nm","-D","--with-symbol-versions",libmso_path],stdout=subprocess.PIPE,universal_newlines=True)
for line in nm.stdout.split("\n"):
  for function in glibc_versions:
    r = re.search("\s"+function+"@@+(GLIBC_[0-9\._]*\s*$)", line.strip())
    if r:
      glibc_versions[function] = r.group(1)

# Produce version script.
glibc_versions_inv = {}
for function,glibc_version in glibc_versions.items():
  if glibc_version not in glibc_versions_inv:
    glibc_versions_inv[glibc_version] = []
  glibc_versions_inv[glibc_version].append(function)
version_script = ""
for glibc_version in glibc_versions_inv:
  version_script += glibc_version + " { " + "".join([function + "; " for function in glibc_versions_inv]) + " } ; \n"
with open("version-script", "w") as f:
  f.write(version_script)

# Compile wrapper libm.so.
compile_libm = subprocess.run(["gcc","math.c",libmso_path.replace(libmso_name,"libm.a"), "-o",libmso_name,"-shared","-fPIC","-Wl,--version-script","version-script","-m32","-I../../install/include"], universal_newlines=True)

