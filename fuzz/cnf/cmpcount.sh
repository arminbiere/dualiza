#!/bin/sh
a=`../../dualiza $1 |tail -1`
echo sat $a
b=`../../dualiza $1 -b |tail -1`
echo bdd $b
exec test $a = $b
