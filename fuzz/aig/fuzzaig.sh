#!/bin/sh
cd `dirname $0`
if [ ! -f aiger/aigfuzz ]
then
  make -C aiger || exit 1
fi
if [ $# -gt 0 ]
then
  case "$1" in 
    *.aig) suffix=aig;;
    *) suffix=aag;;
  esac
else
  suffix=aag
fi
tmp=/tmp/fuzzaig-$$
original=${tmp}-original.$suffix
anded=${tmp}-anded.$suffix
trap "rm -f $tmp*" 2 9 15
./aiger/aigfuzz -m -c -1 -s -o $original
./aiger/aigand $original $anded
./aiger/aigstrip $anded
if [ $# -gt 0 ]
then
  cp $anded $1
else
  cat $anded
fi
rm -f $tmp*
