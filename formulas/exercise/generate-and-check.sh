#!/bin/sh

make || exit 1

#./generate > /dev/null

echo -n "generated "
ls ??.cnf|wc -l

echo -n "different "
for cnf in ??.cnf
do
  grep -v '^c' $cnf|md5sum
done | sort | uniq | wc -l

for cnf in ??.cnf
do
  name=`basename $cnf .cnf`
  last=`expr $name : '.\(.\)'`
  if [ $last -lt  5 ]
  then
    expected=`expr $last + 10`
  else
    expected=$last
  fi
  solutions=`dualiza $cnf|tail -1`
  #echo $cnf $name $last $expected $solutions
  [ $expected = $solutions ] && continue
  echo "*** $cnf has $solutions solutions but expected $expected"
  exit 1
done
echo "all CNF files have expected number of solutions"

for cnf in ??.cnf
do
  name=`basename $cnf .cnf`
  dualiza -f $cnf | \
  sed \
  -e 's,x1,a,g' \
  -e 's,x2,b,g' \
  -e 's,x3,c,g' \
  -e 's,x4,d,g' \
  -e 's,x5,e,g' | \
  sed -e 's,& ,&@,g' | \
  tr @ \\n > $name.limboole
done

for formula in ??.limboole
do
  name=`basename $formula .limboole`
  last=`expr $name : '.\(.\)'`
  if [ $last -lt  5 ]
  then
    expected=`expr $last + 10`
  else
    expected=$last
  fi
  solutions=`dualiza $formula|tail -1`
  #echo $cnf $name $last $expected $solutions
  [ $expected = $solutions ] && continue
  echo "*** $formula has $solutions solutions but expected $expected"
  exit 1
done
echo "all formulas have expected number of solutions"
