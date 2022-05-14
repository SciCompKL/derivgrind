from TestCase import TestCase

summing = TestCase("summing")
summing.stmt = "double c = a+b;"
summing.vals = {'a':1.0,'b':2.0}
summing.grads = {'a':3.0,'b':4.0}
summing.test_vals = {'c':3.0}
summing.test_grads = {'c':7.0}

summing.run()
