import sys
import subprocess
import fnmatch

install_dir = "../../install"
selected_testcase = "*"
for arg in sys.argv[1:]:
  if arg.startswith("--prefix="):
    install_dir = arg[len("--prefix="):]
  else:
    selected_testcase = arg

subprocess.run(["g++", "tricky-program.cpp", "-o", "tricky-program", "-O0", "-mfpmath=sse", "-g", "-I"+install_dir+"/include"])

# Map testcase name to a pair of booleans.
# The first element indicates whether a wrong derivative is reported as an error. 
# As we mostly test unsupported bit-tricks here, this will usually be False.
# The second element indicates whether a wrong derivative without bit-trick finder
# warning is reported as an error. This is usually True, except for known false negatives.
tests = {
  "integer_addition_to_exponent_double": (False,True),
  "integer_addition_to_exponent_float": (False,True),
  "incomplete_masking_to_perform_frexp_double": (False,True),
  "incomplete_masking_to_perform_frexp_float": (False,True),
  "exploiting_imprecisions_for_rounding_double": (False,False),
  "exploiting_imprecisions_for_rounding_float": (False,False),
}
fail = False
for test in tests:
  if fnmatch.fnmatchcase(test,selected_testcase):
    print(test+":")
    forwardrun = subprocess.run([install_dir+"/bin/valgrind", "--tool=derivgrind", "./tricky-program", test], capture_output=True)
    forward_correct = (forwardrun.stdout.decode("utf-8").find("WRONG FORWARD-MODE DERIVATIVE")==-1)
    if forward_correct:
      forward_output = "correct derivative - OK  "
    elif not tests[test][0]:
      forward_output = "wrong derivative   - OK  "
    else:
      forward_output = "wrong derivative   - FAIL"
      fail = True
    print("  "+forward_output)

    trickrun = subprocess.run([install_dir+"/bin/valgrind", "--tool=derivgrind", "--trick=yes", "./tricky-program", test], capture_output=True)
    trick_found = (trickrun.stderr.decode("utf-8").find("Active discrete data used as floating-point operand.")!=-1)
    if trick_found:
      trick_output = "trick found        - OK  "
    elif not tests[test][1]:
      trick_output = "trick not found    - OK  "
    else:
      trick_output = "trick not found    - FAIL"
      fail = True
    print("  "+trick_output) 

exit(fail)


  
