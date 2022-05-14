from TestCase import TestCase

testlist = []

addition = TestCase("addition")
addition.stmt = "double c = a+b;"
addition.vals = {'a':1.0,'b':2.0}
addition.grads = {'a':3.0,'b':4.0}
addition.test_vals = {'c':3.0}
addition.test_grads = {'c':7.0}
testlist.append(addition)

addition_const_l = TestCase("addition_const_l")
addition_const_l.stmt = "double c = 0.3 + a;"
addition_const_l.vals = {'a':1.0}
addition_const_l.grads = {'a':2.0}
addition_const_l.test_vals = {'c':1.3}
addition_const_l.test_grads = {'c':2.0}
testlist.append(addition_const_l)

addition_const_r = TestCase("addition_const_r")
addition_const_r.stmt = "double c = a + 0.3;"
addition_const_r.vals = {'a':1.0}
addition_const_r.grads = {'a':2.0}
addition_const_r.test_vals = {'c':1.3}
addition_const_r.test_grads = {'c':2.0}
testlist.append(addition_const_r)

subtraction = TestCase("subtraction")
subtraction.stmt = "double c = a-b;"
subtraction.vals = {'a':1.0,'b':2.0}
subtraction.grads = {'a':3.0,'b':4.0}
subtraction.test_vals = {'c':-1.0}
subtraction.test_grads = {'c':-1.0}
testlist.append(subtraction)

subtraction_const_l = TestCase("subtraction_const_l")
subtraction_const_l.stmt = "double c = 0.3 - a;"
subtraction_const_l.vals = {'a':1.0}
subtraction_const_l.grads = {'a':2.0}
subtraction_const_l.test_vals = {'c':-0.7}
subtraction_const_l.test_grads = {'c':-2.0}
testlist.append(subtraction_const_l)

subtraction_const_r = TestCase("subtraction_const_r")
subtraction_const_r.stmt = "double c = a - 0.5;"
subtraction_const_r.vals = {'a':1.0}
subtraction_const_r.grads = {'a':2.0}
subtraction_const_r.test_vals = {'c':0.5}
subtraction_const_r.test_grads = {'c':2.0}
testlist.append(subtraction_const_r)

multiplication = TestCase("multiplication")
multiplication.stmt = "double c = a*b;"
multiplication.vals = {'a':1.0,'b':2.0}
multiplication.grads = {'a':3.0,'b':4.0}
multiplication.test_vals = {'c':2.0}
multiplication.test_grads = {'c':10.0}
testlist.append(multiplication)

multiplication_const_l = TestCase("multiplication_const_l")
multiplication_const_l.stmt = "double c = 0.3 * a;"
multiplication_const_l.vals = {'a':2.0}
multiplication_const_l.grads = {'a':3.0}
multiplication_const_l.test_vals = {'c':0.6}
multiplication_const_l.test_grads = {'c':0.9}
testlist.append(multiplication_const_l)

multiplication_const_r = TestCase("multiplication_const_r")
multiplication_const_r.stmt = "double c = a * 0.5;"
multiplication_const_r.vals = {'a':2.0}
multiplication_const_r.grads = {'a':3.0}
multiplication_const_r.test_vals = {'c':1.0}
multiplication_const_r.test_grads = {'c':1.5}
testlist.append(multiplication_const_r)

division = TestCase("division")
division.stmt = "double c = a/b;"
division.vals = {'a':1.0,'b':2.0}
division.grads = {'a':5.0,'b':4.0}
division.test_vals = {'c':0.5}
division.test_grads = {'c':1.5}
testlist.append(division)

division_const_l = TestCase("division_const_l")
division_const_l.stmt = "double c = 0.3 / a;"
division_const_l.vals = {'a':2.0}
division_const_l.grads = {'a':3.0}
division_const_l.test_vals = {'c':0.15}
division_const_l.test_grads = {'c':-9/40}
testlist.append(division_const_l)

division_const_r = TestCase("division_const_r")
division_const_r.stmt = "double c = a / 0.5;"
division_const_r.vals = {'a':2.0}
division_const_r.grads = {'a':3.0}
division_const_r.test_vals = {'c':4.0}
division_const_r.test_grads = {'c':6.0}
testlist.append(division_const_r)

outcomes = []
for test in testlist:
  outcomes.append(test.run())

print("Summary:")
there_are_failed_tests = False
for i in range(len(testlist)):
  if outcomes[i]:
    print("  "+testlist[i].name+": PASSED")
  else:
    print("* "+testlist[i].name+": FAILED")
    there_are_failed_tests = True

if there_are_failed_tests:
  exit(1)
else:
  exit(0)

  
