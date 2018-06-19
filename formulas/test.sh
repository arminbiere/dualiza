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
    cnf="$tmp.cnf"
    if filter $dualiza -d $1 -o $cnf --no-polarity
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
  execute $dualiza -s --dual $1
  if [ ! "$last" = "$firstline" ]
  then
    error \
"counting mismatch with '--dual' configuration: '$last' and '$firstline'"
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
  execute $dualiza -t --dual $1
  if [ ! "$last" = "$firstline" ]
  then
    error \
"counting mismatch with '--dual' configuration: '$last' and '$firstline'"
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
  case `basename $1 .form` in
    0000|0011);; # sharpSAT gives wrong solution '1'
    *)
      if [ "$sharpsat" ]
      then
	cnf="$tmp.cnf"
	if filter $dualiza -d $1 -o $cnf --no-polarity
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

for i in $dir/*.form
do
  run $i
done

erase
rm -f $tmp*
exit 0
