#!/bin/sh
case "$1" in
  [0-9]*) n="$1";;
  *) n=1;;
esac
[ $n -lt 1 ] && n=1
i=1
while [ $i -le $n ]
do
  [ $i -gt 1 ] && echo -n " | "
  echo -n x$i
  i=`expr $i + 1`
done
echo
