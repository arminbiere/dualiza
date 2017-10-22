#!/bin/sh
debug=no
check=undefined
log=undefined
testing=no
die () {
  echo "*** configure.sh: $*" 1>&2
  exit 1
}
usage () {
cat <<EOF
usage: configure.sh [ <option> ]

where '<option>' is one of the following

-h | --help   print this command line option summary 
-g | --debug  compile with debugging information
-c | --check  include assertion checking code (default for '-g')
-l | --log    include logging code (default for '-g')
-t | --test   include testing code
EOF
exit 0
}
while [ $# -gt 0 ]
do
  case "$1" in
    -h | --help) usage;;
    -g | --debug) debug=yes;;
    -l | --log | --logging) log=yes;;
    -c | --chk | --check | --checking) check=yes;;
    -t | --test | --testing) testing=yes;;
    *) die "invalid option '$1' (try '-h')";;
  esac
  shift
done
CC=gcc
CFLAGS=-Wall
[ $check = undefined ] && check=$debug
[ $log = undefined ] && log=$debug
if [ $debug = yes ]
then
  CFLAGS="$CFLAGS -g"
else
  CFLAGS="$CFLAGS -O3"
fi
[ $check = no ] && CFLAGS="$CFLAGS -DNDEBUG"
[ $log = no ] && CFLAGS="$CFLAGS -DNLOG"
[ $testing = yes ] && CFLAGS="$CFLAGS -DTEST"
echo "$CC $CFLAGS"
rm -f makefile
sed -e "s,@CC@,$CC," -e "s,@CFLAGS@,$CFLAGS," makefile.in > makefile
