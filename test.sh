#!/bin/sh
for tests in \
  ./cnfs/test.sh \
  ./formulas/test.sh \
  ./aigs/test.sh
do
  echo "$0: running '$tests'"
  $tests || exit 1
done
