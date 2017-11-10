#!/bin/sh
cd `dirname $0`
[ -t 1 ] && echo "$0: write to file or pipe to less for details"
tmp="/tmp/dualiza-formulas-test-$$"
trap "rm -f $tmp*" 2
die () {
  echo "*** $0: $*" 1>&2
  rm -f $tmp*
  exit 1
}

picosat=""
if which picosat >/dev/null 2>/dev/null
then
  path="`which picosat 2>/dev/null`"
  if [ "`echo 'p cnf 0 0'|\"$path\"|head -1`" = "s SATISFIABLE" ]
  then
    picosat="$path"
  fi
fi
if [ "$picosat" ]
then
  echo "$0: using '$picosat'"
else
  echo "$0: no 'picosat' found to test against"
fi

dualiza=../dualiza
[ -f $dualiza ] || die "can not find '$dualiza'"

sharpsat=""
if which sharpSAT >/dev/null 2>/dev/null
then
  path="`which sharpSAT`"
  echo 'p cnf 0 0' > $tmp
  if [ "`\"$path\" \"$tmp\"|sed -e '1,/# solutions/d' -e '/# END/,$d'`" = 1 ]
  then
    sharpsat="$path"
  fi
fi
if [ "$sharpsat" ]
then
  echo "$0: using '$sharpsat'"
else
  echo "$0: no 'sharpSAT' found to test against"
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
filter () {
  erase
  echo -n "$*"
  $* > $tmp
  res=$?
  [ -t 1 ] || echo
  return $res
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
    lines="`wc -l $tmp|awk '{print $1}'`"
    case "$lines" in
      0);;
      1)
	echo "$firstline"
        ;;
      2)
	echo "$firstline"
	echo "$lastline"
	;;
      *)
	echo "$firstline"
	echo "..."
	echo "$lastline"
	;;
    esac
  fi
}
error () {
  [ -t 1 ] && echo
  die "$*"
}
sat () {
  execute $dualiza -s $1
  last="$firstline"
  execute $dualiza -s $1 -b
  if [ ! "$last" = "$firstline" ]
  then
    error \
"sat checking mismatch between SAT and BDD engine: '$last' and '$firstline'"
  fi
  if [ "$picosat" ]
  then
    cnf="$tmp.cnf"
    if filter $dualiza -d $1 -o $cnf
    then
      execute "$picosat" $cnf
      res=`echo $firstline|awk '{print $2}'`
      if [ ! "$res" = "$last" ]
      then
        error \
"sat checking mismatch between SAT engine and PicoSAT: '$last' and '$res'"
      fi
    else
      [ -t 1 ] && echo
      error "failed to generate primal CNF"
    fi
  fi
}
tautology () {
  execute $dualiza -t $1
  last="$firstline"
  execute $dualiza -t $1 -b
  if [ ! "$last" = "$firstline" ]
  then
    error \
"tautology checking mismatch between SAT and BDD engine: '$last' and '$firstline'"
  fi
}
count () {
  execute $dualiza $1
  last="$lastline"
  execute $dualiza $1 -b
  if [ ! "$last" = "$lastline" ]
  then
    error \
"counting mismatch between SAT and BDD engine: '$last' and '$lastline'"
  fi
  if [ "$sharpsat" ]
  then
    cnf="$tmp.cnf"
    if filter $dualiza -d $1 -o $cnf
    then
      filter "$sharpsat" $cnf
      res="`sed -e '1,/# solutions/d' -e '/# END/,$d' $tmp`"
      [ -t 1 ] || echo $res
      if [ ! "$res" = "$last" ]
      then
        error \
"sat checking mismatch between SAT engine and sharpSAT: '$last' and '$res'"
      fi
    else
      [ -t 1 ] && echo
      error "failed to generate primal CNF"
    fi
  fi
}
run () {
  [ -t 1 ] || ( echo; echo )
  sat $1
  tautology $1
  count $1
}
for i in *.form
do
  run $i
done
erase
rm -f $tmp*
exit 0
