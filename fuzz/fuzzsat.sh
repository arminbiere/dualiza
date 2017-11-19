#!/bin/sh
while true
do
  cnf=/tmp/dualiza-fuzzsat.cnf
  cnfuzz --tiny > $cnf
  if ./cmpsat.sh $cnf > /dev/null; then continue; fi
  seed="`awk '/^c seed/{print $3}' $cnf`"
  bug=bug-sat-$seed.cnf
  reduced=reduced-sat-$seed.cnf
  cp $cnf $bug
  echo "$bug `grep 'p cnf' $bug`"
  cnfdd $bug $reduced ./cmpsat.sh
  echo "$reduced `grep 'p cnf' $reduced`"
  sleep 10
done
