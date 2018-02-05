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
  if [ "$picosat" -a "$aigtocnf" ]
  then
    cnf="$tmp.cnf"
    if filter $aigtocnf $1 $cnf
    then
      execute "$picosat" $cnf
      if [ ! "$firstline" = "s $last" ]
      then
	error \
  "sat checking mismatch between SAT engine and PicoSAT: '$last' and '$firstline'"
      fi
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
  case `basename $1|sed -e 's,.a[ai]g$,,'` in
    false) ;; # sharpSAT gives wrong answer # TODO not adapted to aigs yet
    *)
      if [ "$sharpsat" -a "$aigtocnf" ]
      then
	cnf="$tmp.cnf"
	if filter $aigtocnf $1 $cnf
	then
	  filter "$sharpsat" $cnf
	  res="`sed -e '1,/# solutions/d' -e '/# END/,$d' $tmp`"
	  [ -t 1 ] || echo $res
	  if [ ! "$res" = "$last" ]
	  then
	    error \
  "sat checking mismatch between SAT engine and sharpSAT: '$last' and '$res'"
	  fi
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

for i in $dir/*.aag
do
  run $i
done

erase
rm -f $tmp*
exit 0
