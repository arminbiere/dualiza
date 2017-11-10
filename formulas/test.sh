#!/bin/sh
tmp="/tmp/dualiza-formulas-test-$$"
trap "rm -f $tmp" 2
die () {
  echo "*** $0: $*" 1>&2
  rm -f $tmp
  exit 1
}
cd `dirname $0`
dualiza=../dualiza
[ -f $dualiza ] || die "can not find '$dualiza'"
firsterase=yes
erase () {
  if [ $firsterase = yes ]
  then
    firsterase=no
  elif [ -t 1 ]
  then
    printf '\r%70s\r' ""
  fi
}
execute () {
  erase
  echo -n "$*"
  $* > $tmp
  firstline="`head -1 $tmp`"
  lastline="`tail -1 $tmp`"
  if [ ! -t 1 ]
  then
    echo "$firstline"
    echo "$lastline"
  fi
}
sat () {
  execute $dualiza -s $1
  last="$firstline"
  execute $dualiza -s $1 -b
  if [ ! "$last" = "$firstline" ]
  then
    echo
    die \
"sat checking mismatch between SAT and BDD engine: '$last' and '$firstline'"
  fi
}
tautology () {
  execute $dualiza -t $1
  last="$firstline"
  execute $dualiza -t $1 -b
  if [ ! "$last" = "$firstline" ]
  then
    echo
    die \
"tautology checking mismatch between SAT and BDD engine: '$last' and '$firstline'"
  fi
}
count () {
  execute $dualiza $1
  last="$lastline"
  execute $dualiza $1 -b
  if [ ! "$last" = "$lastline" ]
  then
    echo
    die \
"counting mismatch between SAT and BDD engine: '$last' and '$lastline'"
  fi
}
run () {
  sat $1
  tautology $1
  count $1
}
for i in *.form
do
  run $i
done
erase
rm -f $tmp
