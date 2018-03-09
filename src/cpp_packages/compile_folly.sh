# !/bin/bash

# pass a .cc file for simple compilation.

# check if a file is passed

if [ $# -eq 0 ]; then
  echo "No arguments supplied"
fi


g++ -std=c++14 "$@" -lfolly -lpthread -lglog -levent -ldl -ldouble-conversion
