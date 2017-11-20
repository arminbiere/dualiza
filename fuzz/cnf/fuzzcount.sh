#!/bin/sh
make || exit 1
i=0
while true
do
  cnf=/tmp/dualiza-fuzzcount.cnf
  ./cnfuzz --tiny > $cnf
  seed="`awk '/^c seed/{print $3}' $cnf`"
  i=`expr $i + 1`
  echo -n "\r$i $seed         "
  if ./cmpcount.sh $cnf > /dev/null; then continue; fi
  echo
  bug=bug-count-$seed.cnf
  reduced=reduced-count-$seed.cnf
  cp $cnf $bug
  echo "$bug `grep 'p cnf' $bug`"
  ./cnfdd $bug $reduced ./cmpcount.sh > /dev/null 2>/dev/null
  echo "$reduced `grep 'p cnf' $reduced`"
done
