set shell := ["powershell"]

c: configure
configure:
  cmake -S . -B . -G Ninja

b: build
build:
  cmake --build .

t: test
test:
  ctest
