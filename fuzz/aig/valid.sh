#!/bin/sh
[ -f "$1" ] || exit 1
read type M I L O A < $1
case $type in
  aig|aag);;
  *) exit1;;
esac
[ $L -gt 0 ] && exit 1
[ $O = 1 ] || exit 1
exit 0
