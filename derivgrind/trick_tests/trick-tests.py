import sys
import subprocess
import fnmatch
import tempfile

install_dir = "../../install"
temp_dir = None
selected_testcase = "*" # The user might specify a pattern in order to run only a subset of all bit-trick tests.
no_external_dependencies = False # suppress testcases that require external dependencies
for arg in sys.argv[1:]:
  if arg.startswith("--prefix="):
    install_dir = arg[len("--prefix="):]
  elif arg.startswith('--tempdir='):
    temp_dir = arg[len('--tempdir='):]
  elif arg.startswith('--no-external-dependencies'):
    no_external_dependencies = True
  elif arg in ['-h', '--help']:
    print(
"""Test script to run testcases of the "tricky program" under Derivgrind's
forward-mode and bit-trick-finder instrumentation.
Arguments: 
  --prefix=path    .. Path of Derivgrind installation.
  --tempdir=path   .. Temporary directory used by this script.
  --no-external-dependencies  
    .. If certain libraries required for some testcases (libgcrypt,libz) are 
       missing, do not run these.
  testcase_name    .. Run only a single or few testcases; "*" is a wildcard.
""")
    exit(0)


  else:
    selected_testcase = arg
if temp_dir == None:
  t = tempfile.TemporaryDirectory()
  temp_dir = t.name

subprocess.run(["g++"] + (["-DTRICKTEST_NO_EXTERNAL_DEPENDENCIES"] if no_external_dependencies else []) +["tricky-program.cpp", "-o", temp_dir+"/tricky-program", "-O0", "-mfpmath=sse", "-g", "-I"+install_dir+"/include"] + ([] if no_external_dependencies else ["-lgcrypt","-lz"]) )

# Map testcase name to a tuple of booleans.
# The first element indicates whether external dependencies are required.
# The secomd element indicates whether a wrong derivative is reported as an error. 
# As we mostly test unsupported bit-tricks here, this will usually be False.
# The third element indicates whether a wrong derivative without bit-trick finder
# warning is reported as an error. This is usually True, except for known false negatives.
tests = {
  "integer_addition_to_exponent_double": (False,False,True),
  "integer_addition_to_exponent_float": (False,False,True),
  "incomplete_masking_to_perform_frexp_double": (False,False,True),
  "incomplete_masking_to_perform_frexp_float": (False,False,True),
  "exploiting_imprecisions_for_rounding_double": (False,False,False),
  "exploiting_imprecisions_for_rounding_float": (False,False,False),
  "encrypt_decrypt": (True,False,True),
  "compress_inflate": (True,True,True),
}
fail = False
for test in tests:
  if fnmatch.fnmatchcase(test,selected_testcase):
    print(test+":")
    # If test requires external dependencies but they are disabled, skip.
    if tests[test][0] and no_external_dependencies:
      print("  ext deps disabled  - SKIP")
      continue

    # Test correctness of forward-mode derivatives.
    forwardrun = subprocess.run([install_dir+"/bin/valgrind", "--tool=derivgrind", temp_dir+"/tricky-program", test], capture_output=True)
    forward_correct = (forwardrun.stdout.decode("utf-8").find("WRONG FORWARD-MODE DERIVATIVE")==-1)
    if forward_correct:
      forward_output = "correct derivative - OK  "
    elif not tests[test][1]:
      forward_output = "wrong derivative   - OK  "
    else:
      forward_output = "wrong derivative   - FAIL"
      fail = True
    print("  "+forward_output)

    # Test whether the bit-trick finding instrumentation produces a warning.
    trickrun = subprocess.run([install_dir+"/bin/valgrind", "--tool=derivgrind", "--trick=yes", temp_dir+"/tricky-program", test], capture_output=True)
    trick_found = (trickrun.stderr.decode("utf-8").find("Active discrete data used as floating-point operand.")!=-1)
    if trick_found:
      trick_output = "trick found        - OK  "
    elif not tests[test][2]:
      trick_output = "trick not found    - OK  "
    else:
      trick_output = "trick not found    - FAIL"
      fail = True
    print("  "+trick_output) 

exit(fail)


  
