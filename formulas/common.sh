dir=`pwd|xargs basename`
cd ..
[ -t 1 ] && echo "$0: write to file or pipe into less for more details"
tmp="/tmp/dualiza-test-$$"
trap "rm -f $tmp*" 2
die () {
  echo "$0: $*"
  rm -f $tmp*
  exit 1
}

dualiza="`pwd`/dualiza"
if [ x"`which dualiza`" = x"$dualiza" ]
then
  echo "$0: using '$dualiza'"
  dualiza=dualiza
elif [ ! "`which dualiza`" = x -a x"`which dualiza|xargs readlink`" = x"$dualiza" ]
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

aigtocnf=""
if which aigtocnf >/dev/null 2>/dev/null
then
  path="`which aigtocnf 2>/dev/null`"
  if [ "`\"$path\" aigs/false.aig|fmt`" = "p cnf 0 1 0" ]
  then
    aigtocnf="$path"
  fi
fi
if [ "$aigtocnf" ]
then
  echo "$0: using '$aigtocnf'"
  aigtocnf=aigtocnf
else
  echo "$0: no 'aigtocnf' found to use for AIGER to CNF translation"
fi

firsterase=yes
erase () {
  if [ $firsterase = yes ]
  then
    firsterase=no
  elif [ -t 1 ]
  then
    printf '\r%78s\r' ""
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
error () {
  [ -t 1 ] && echo
  die "$*"
}
execute () {
  erase
  echo -n "$* "
  $* > $tmp 2>$tmp.err
  status=$?
  firstline="`head -1 $tmp|sed -e 's,,,g'`"
  lastline="`tail -1 $tmp|sed -e 's,,,g'`"
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
  case $status in
    0|10|20) ;;
    *)
      [ -t 1 ] && echo
      cat $tmp.err
      exit 1
      ;;
  esac
}
enumerate () {
  for configuration in $configurations
  do
    args=`echo $configuration | sed -e 's,_, ,g'`
    execute $dualiza -e $args $1
    if [ ! $status = 0 ]
    then 
      [ -t 1 ] && echo
      cat $tmp.err
      exit 1
    fi
  done
}
run () {
  [ -t 1 ] || ( echo; echo )
  sat $1
  tautology $1
  count $1
  enumerate $1
}
for dual in "--dual" "--no-dual"
do
for block in "--block" "--no-block"
do
for discount in "--discount" "--no-discount"
do
if [ $block = "--block" ]
then
  blocklimits="_--blocklimit=1 _--blocklimit=2 _--blocklimit=3"
else
  blocklimits=""
fi
for blocklimit in $blocklimits
do
for subsume in --subsume --no-subsume
do
for elim in --elim --no-elim
do
configurations="$configurations
${dual}_${block}_${discount}${blocklimit}"
done
done
done
done
done
done
