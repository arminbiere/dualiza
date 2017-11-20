#!/bin/sh
make || exit 1
i=0
while true
do
  cnf=/tmp/dualiza-fuzzsat.cnf
  ./cnfuzz --tiny > $cnf
  seed="`awk '/^c seed/{print $3}' $cnf`"
  i=`expr $i + 1`
  echo -n "\r$i $seed         "
  if ./cmpsat.sh $cnf > /dev/null; then continue; fi
  echo
  bug=bug-sat-$seed.cnf
  reduced=reduced-sat-$seed.cnf
  cp $cnf $bug
  echo "$bug `grep 'p cnf' $bug`"
  ./cnfdd $bug $reduced ./cmpsat.sh >/dev/null 2>/dev/null
  echo "$reduced `grep 'p cnf' $reduced`"
done
