if [[ ! -d "pybind11" ]]; then
  git clone https://github.com/pybind/pybind11.git
fi
g++ -fvisibility=hidden -shared -fPIC -std=c++17 -Ipybind11/include `python3-config --includes` derivgrind_clientrequests.cpp -o derivgrind.so -I../../../install/include
