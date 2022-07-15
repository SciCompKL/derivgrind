if [[ ! -d "pybind11" ]]; then
  git clone https://github.com/pybind/pybind11.git
fi
python3 -c "import sys; exit(sys.maxsize>2**32)"
if [[ "$?" -eq 1 ]]; then # python3 is 64-bit executable
  export M32IF32=""
else # python3 is 32-bit executable
  export M32IF32="-m32"
fi
g++ -fvisibility=hidden -shared -fPIC -std=c++17 -Ipybind11/include `python3-config --includes` derivgrind_clientrequests.cpp -o derivgrind.so -I../../../install/include $M32IF32
