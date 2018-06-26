#!/bin/sh
tmp=/tmp/cmpsat-$$
satlog=${tmp}-sat.log
bddlog=${tmp}-bdd.log
trap "rm -f $tmp*" 2 9 15
../../dualiza --print=0 -s $1 2>/dev/null >$satlog
sat="`head -1 $satlog`"
../../dualiza --print=0 -s -b $1 2>/dev/null >$bddlog
bdd="`head -1 $bddlog`"
rm -f $tmp*
echo "sat '$sat'"
echo "bdd '$bdd'"
[ x"$sat" = x ] && exit 1
exec test "$sat = $bdd"
