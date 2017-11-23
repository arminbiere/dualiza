#!/bin/sh
a="`../../dualiza --print=0 -s $1 |head -1`"
echo sat $a
b="`../../dualiza --print=0 -s $1 -b |head -1`"
echo bdd $b
exec test "$a = $b"
