#!/bin/sh
LANG=C
LC_TIME=C
export LANG
export LC_TIME
[ -f makefile ] && make clean
VERSION="`cat VERSION`"
GITID="`git show|awk '{print $2;exit}'|sed -e 's,^\(.......\).*,\1,'`"
BASE=`pwd|xargs basename`
NAME=$BASE-$VERSION-$GITID
DIR=/tmp/$NAME
rm -rf $DIR || exit 1
mkdir $DIR || exit 1
cp -p `ls *.[ch]|grep -v version.c` $DIR
cp -p makefile.in configure.sh test.sh VERSION README LICENSE $DIR
RELEASED="`git log -n 1|sed -e '/^Date:/!d' -e 's,^Date: *,,'`"
sed \
  -e "s,^VERSION=.*,VERSION=$VERSION," \
  -e "s,^GITID=.*,GITID=$GITID," \
  -e "s,^RELEASED=.*,RELEASED=\"$RELEASED\"," \
version.sh > $DIR/version.sh
chmod 755 $DIR/version.sh
cp -pr formulas cnfs aigs fuzz $DIR/
cd /tmp
TAR=/tmp/$NAME.tar.xz
tar cfJ $TAR $NAME
ls -l $TAR
