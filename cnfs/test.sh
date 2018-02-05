#!/bin/sh

cd `dirname $0`
. ../formulas/common.sh

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
    execute "$picosat" $1
    if [ ! "$firstline" = "$last" ]
    then
      error \
"sat checking mismatch between SAT engine and PicoSAT: '$last' and '$firstline'"
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
  case `basename $1 .cnf` in
    0000) ;; # sharpSAT gives wrong answer
    *)
      if [ "$sharpsat" ]
      then
	filter "$sharpsat" $1
	res="`sed -e '1,/# solutions/d' -e '/# END/,$d' $tmp`"
	[ -t 1 ] || echo $res
	if [ ! "$res" = "$last" ]
	then
	  error \
"sat checking mismatch between SAT engine and sharpSAT: '$last' and '$res'"
	fi
      fi
      ;;
  esac
  for configuration in $configurations
  do
    args=`echo $configuration | sed -e 's,_, ,g'`
    execute $dualiza $args $1
    if [ ! "$last" = "$lastline" ]
    then
      error \
  "counting mismatch with '$args' configuration: '$last' and '$lastline'"
    fi
  done
}

for i in $dir/*.cnf
do
  run $i
done

erase
rm -f $tmp*
exit 0
