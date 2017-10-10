#!/bin/sh
debug=no
die () {
  echo "*** configure.sh: $*" 1>&2
  exit 1
}
usage () {
cat <<EOF
usage: configure.sh [-h | --help] [-g | --debug]
EOF
exit 0
}
while [ $# -gt 0 ]
do
  case "$1" in
    -g | --debug) debug=yes;;
    -h | --help) usage;;
    *) die "invalid option '$1' (try '-h')";;
  esac
  shift
done
CC=gcc
CFLAGS=-Wall
if [ $debug = yes ]
then
  CFLAGS="$CFLAGS -g"
else
  CFLAGS="$CFLAGS -O3 -DNDEBUG"
fi
echo "$CC $CFLAGS"
rm -f makefile
sed -e "s,@CC@,$CC," -e "s,@CFLAGS@,$CFLAGS," makefile.in > makefile
