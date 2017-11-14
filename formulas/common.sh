dir=`pwd|xargs basename`
cd ..
[ -t 1 ] && echo "$0: write to file or pipe into less for more details"
tmp="/tmp/dualiza-formulas-test-$$"
trap "rm -f $tmp*" 2
die () {
  echo "*** $0: $*" 1>&2
  rm -f $tmp*
  exit 1
}

dualiza="`pwd`/dualiza"
if [ x"`which dualiza`" = x"$dualiza" ]
then
  echo "$0: using '$dualiza'"
  dualiza=dualiza
elif [ x"`which dualiza|xargs readlink`" = x"$dualiza" ]
then
  echo "$0: using '$dualiza'"
  dualiza=dualiza
else
  dualiza=./dualiza
  [ -f "$dualiza" ] || die "can not find '$dualiza'"
  echo "$0: using '$dualiza'"
fi

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
  picosat=picosat
else
  echo "$0: no 'picosat' found to test against"
fi

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
  sharpsat=sharpSAT
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
run () {
  [ -t 1 ] || ( echo; echo )
  sat $1
  tautology $1
  count $1
}
