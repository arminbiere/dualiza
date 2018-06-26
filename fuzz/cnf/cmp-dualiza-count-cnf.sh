#!/bin/sh
tmp=/tmp/cmpsat-$$
satlog=${tmp}-sat.log
bddlog=${tmp}-bdd.log
trap "rm -f $tmp*" 2 9 15
../../dualiza --print=0 $1 2>/dev/null >$satlog
sat="`tail -1 $satlog`"
../../dualiza --print=0 -b $1 2>/dev/null >$bddlog
bdd="`tail -1 $bddlog`"
rm -f $tmp*
echo "sat '$sat'"
echo "bdd '$bdd'"
[ x"$sat" = x ] && exit 1
exec test "$sat = $bdd"
