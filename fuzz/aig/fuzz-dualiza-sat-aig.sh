#!/bin/sh
cd `dirname $0`
case "$0" in
  *count-aig.sh) mode=count;;
  *) mode=sat;;
esac
cmp=./cmp-dualiza-${mode}-aig.sh
if [ ! -f aiger/aigfuzz ]
then
  make -C aiger || exit 1
fi
i=0
suffix=aag
tmp=/tmp/dualiza-aig-fuzz${mode}-$$
child=0
trap "[ $child ] || kill -9 $child; rm -f $tmp*; exit 1" 2 9 15
while true
do
  aig=$tmp.$suffix
  ./fuzzaig.sh $aig
  seed="`./aiger/aiginfo $aig|awk '/^seed/{print $2}'`"
  i=`expr $i + 1`
  echo -n "\r$i $seed         "
  $cmp $aig > /dev/null &
  child=$!
  wait $child && continue
  echo
  bug=bug-${mode}-$seed.$suffix
  red=red-${mode}-$seed.$suffix
  cp $aig $bug
  echo "$bug `head -1 $bug 2>/dev/null`"
  ./aiger/aigdd $bug $red $cmp >/dev/null 2>/dev/null
  echo "$red `head -1 $red 2>/dev/null`"
  if [ $suffix = aag ]; then suffix=aig; else suffix=aag; fi
done
