all:
	make -C aiger all
clean:
	-killall -9 -q cmp-dualiza-sat-aig.sh
	-killall -9 -q cmp-dualiza-count-aig.sh
	-killall -9 -q aigdd
	-killall -9 -q fuzz-dualiza-sat-aig.sh
	-killall -9 -q fuzz-dualiza-count-aig.sh
	-killall -9 -q aigfuzz fuzzaig.sh
	rm -f bug-*.* red-*.*
	make -C aiger clean
