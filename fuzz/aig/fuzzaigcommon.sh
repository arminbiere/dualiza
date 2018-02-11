#!/bin/sh
die () {
  echo "*** `basename $0`: $*" 1>&2
  exit 1
}
[ "`which aigfuzz`" = "" ] && \
die "could not find 'aigfuzz' (install 'aiger' and put 'aigfuzz' in your PATH)"
[ "`which aigand`" = "" ] && \
die "could not find 'aigand' (install 'aiger' and put 'aigand' in your PATH)"
[ "`which aigstrip`" = "" ] && \
die "could not find 'aigstrip' (install 'aiger' and put 'aigstrip' in your PATH)"
