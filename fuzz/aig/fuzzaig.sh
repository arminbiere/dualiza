#!/bin/sh
. `dirname $0`/fuzzaigcommon.sh
aigfuzz -m -c -a -1 -s | aigand | aigstrip
