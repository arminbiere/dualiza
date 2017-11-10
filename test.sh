#!/bin/sh
for tests in ./formulas/test.sh
do
  echo "$tests"
  $tests || exit 1
done
