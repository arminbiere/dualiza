#!/bin/sh
if [ -f valid.sh ]
then 
  ./valid.sh $1 || exit 2
fi
name="`basename $0 .sh`"
echo "name '$name'"
case "$name" in
  *-sat-*) mode=sat;;
  *) mode=count;;
esac
echo "mode '$mode'"
if [ $mode = sat ]
then
  options="-s"
  grep="head -1"
else
  options=""
  grep="tail -1"
fi
echo "options '$options'"
tmp=/tmp/${name}-$$
satlog=${tmp}-sat.log
bddlog=${tmp}-bdd.log
trap "rm -f $tmp*" 2 9 15
../../dualiza --print=0 $options $1 2>/dev/null >$satlog
sat="`$grep $satlog`"
../../dualiza --print=0 $options -b $1 2>/dev/null >$bddlog
bdd="`$grep $bddlog`"
rm -f $tmp*
echo "sat '$sat'"
echo "bdd '$bdd'"
[ x"$sat" = x ] && exit 1
exec test "$sat = $bdd"
