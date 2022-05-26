import subprocess
import re

# Sample program to find libm.so and GLIBC versions.
compile_sample = subprocess.run(["gcc", "sample.c", "-o", "sample", "-m32", "-lm"],universal_newlines=True)
if compile_sample.returncode!=0:
  raise Error("Couldn't compile sample C program.")
# ldd: path and filename of libm.so.N
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
  raise Error("Couldn't find libm.so with ldd.")
# nm: GLIBC version
nm_sample = subprocess.run(["nm", "./sample"], stdout=subprocess.PIPE,universal_newlines=True)
glibc_version = None
for line in nm_sample.stdout.split("\n"):
  r = re.search(r"^U sin@(GLIBC_.*)$", line.strip())
  if r:
    glibc_version = r.group(1)
if glibc_version==None:
  raise Error("Couldn'T find GLIBC version with nm.")

# Produce version script.
version_script_template = None
with open("version-script-template", "r") as f:
  version_script_template = f.read()
version_script = version_script_template.replace("GLIBC_VERSION", glibc_version)
with open("version-script", "w") as f:
  f.write(version_script)

# Compile wrapper libm.so
compile_libm = subprocess.run(["gcc","math.c",libmso_path.replace(libmso_name,"libm.a"), "-o",libmso_name,"-shared","-fPIC","-Wl,--version-script","version-script","-m32","-I../../install/include"], universal_newlines=True)

