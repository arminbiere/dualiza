#!/bin/sh
die () {
  echo "***fuzaig.sh: $*" 1>&2
  exit 1
}
[ "`which aigfuzz`" = "" ] && \
die "could not find 'aigfuzz' (install 'aiger' and put 'aigfuzz' in your PATH)"
[ "`which aigand`" = "" ] && \
die "could not find 'aigand' (install 'aiger' and put 'aigand' in your PATH)"
aigfuzz -m -c -a -1 | aigand | aigstrip
