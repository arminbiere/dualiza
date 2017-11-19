#!/bin/sh
while true
do
  cnf=/tmp/dualiza-fuzzcount.cnf
  cnfuzz --tiny > $cnf
  if ./cmpcount.sh $cnf > /dev/null; then continue; fi
  seed="`awk '/^c seed/{print $3}' $cnf`"
  bug=bug-count-$seed.cnf
  reduced=reduced-count-$seed.cnf
  cp $cnf $bug
  echo "$bug `grep 'p cnf' $bug`"
  cnfdd $bug $reduced ./cmpcount.sh
  echo "$reduced `grep 'p cnf' $reduced`"
  sleep 10
done
