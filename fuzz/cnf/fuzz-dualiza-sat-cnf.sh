#!/bin/sh
make || exit 1
cd `dirname $0`
case "$0" in
  *count-cnf.sh) mode=count;;
  *) mode=sat;;
esac
cmp=./cmp-dualiza-${mode}-cnf.sh
if [ ! -f cnfuzz ]
then
  make || exit 1
fi
i=0
tmp=/tmp/dualiza-cnf-fuzz${mode}-$$
trap "rm -f $tmp*; exit 1" 2 9 15
while true
do
  cnf=$tmp.cnf
  ./cnfuzz --tiny > $cnf
  seed="`awk '/^c seed/{print $3}' $cnf`"
  i=`expr $i + 1`
  echo -n "\r$i $seed         "
  if $cmp $cnf > /dev/null; then continue; fi
  echo
  bug=bug-sat-$seed.cnf
  red=red-sat-$seed.cnf
  cp $cnf $bug
  echo "$bug `grep 'p cnf' $bug`"
  ./cnfdd $bug $red $cmp >/dev/null 2>/dev/null
  echo "$red `grep 'p cnf' $red`"
done
