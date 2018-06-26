all:
	make -C aiger all
clean:
	-killall -9 -q fuzzsat.sh fuzzcount.sh
	-killall -9 -q fuzzaig.sh aigfuzz
	-killall -9 -q cmpsat.sh cmpcount.sh
	-killall -9 -q aigdd
	rm -f bug-*.* red-*.*
	make -C aiger clean
