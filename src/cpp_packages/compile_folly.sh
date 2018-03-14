# !/bin/bash

# pass a .cc file for simple compilation.

# check if a file is passed

if [ $# -eq 0 ]; then
  echo "No arguments supplied"
fi

output=$(echo $1|cut -d "." -f 1)

g++ -std=c++14 "$@" -lssl -lcrypto -lrt -lfolly -lpthread -lglog -levent -ldl -ldouble-conversion -o $output
