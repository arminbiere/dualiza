#!/bin/sh
cd `dirname $0`
tmp="/tmp/dualiza-formulas-test-$$"
trap "rm -f $tmp" 2
die () {
  echo "*** $0: $*" 1>&2
  rm -f $tmp
  exit 1
}

picosat=""
if which picosat >/dev/null 2>/dev/null
then
  path="`which picosat`"
  if [ "`echo 'p cnf 0 0'|"$path"|head -1`" = "s SATISFIABLE" ]
  then
    picosat="$path"
  fi
fi
if [ "$picosat" ]
then
  echo "$0: using '$picosat'"
else
  echo "$0: no 'picosat' found"
fi

dualiza=../dualiza
[ -f $dualiza ] || die "can not find '$dualiza'"

sharpsat=""
if which sharpSAT >/dev/null 2>/dev/null
then
  path="`which sharpSAT`"
  echo 'p cnf 0 0' > $tmp
  if [ "`$path $tmp|sed -e '1,/# solutions/d' -e '/# END/,$d'`" = 1 ]
  then
    sharpsat="$path"
  fi
fi
if [ "$sharpsat" ]
then
  echo "$0: using '$sharpsat'"
else
  echo "$0: no 'sharpSAT' found"
fi

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
    echo
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
exit 0
