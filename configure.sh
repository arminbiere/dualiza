#!/bin/sh
debug=no
check=undefined
log=undefined
gmp=no
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
--gmp         use GMP code
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
    --gmp) gmp=yes;;
    *) die "invalid option '$1' (try '-h')";;
  esac
  shift
done
if [ $gmp = yes ]
then
#tmp=/tmp/dualize-configure-$$
tmp=/tmp/dualize-configure
cat << EOF > $tmp.c
#include <gmp.h>
#include <stdio.h>
int main () {
  mpz_t n;
  mpz_init (n);
  mpz_add_ui (n, n, 40);
  mpz_add_ui (n, n, 2);
  mpz_out_str (stdout, 10, n);
  mpz_clear (n);
  fputc ('\n', stdout);
}
EOF
gcc -o $tmp $tmp.c -lgmp 1>/dev/null 2>/dev/null || \
  die "failed to compile 'gmp' test program"
res="`$tmp`"
[ x"$res" = x42 ] || \
  die "'gmp' produces unexpected output"
LIBS="-lgmp"
fi
CC=gcc
CFLAGS=-Wall
[ $check = undefined ] && check=$debug
[ $log = undefined ] && log=$debug
if [ $debug = yes ]
then
  CFLAGS="$CFLAGS -g3"
else
  CFLAGS="$CFLAGS -O3"
fi
[ $check = no ] && CFLAGS="$CFLAGS -DNDEBUG"
[ $log = no ] && CFLAGS="$CFLAGS -DNLOG"
[ $gmp = yes ] && CFLAGS="$CFLAGS -DGMP"
echo "$CC $CFLAGS $LIBS"
rm -f makefile
sed \
  -e "s,@CC@,$CC," \
  -e "s,@CFLAGS@,$CFLAGS," \
  -e "s,@LIBS@,$LIBS," \
makefile.in > makefile
