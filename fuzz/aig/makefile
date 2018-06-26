all:
	make -C aiger all
clean:
	-killall -q fuzzsat.sh fuzzcount.sh
	-killall -q fuzzaig.sh aigfuzz
	-killall -q cmpsat.sh cmpcount.sh
	rm -f bug-*.* reduced-*.*
	make -C aiger clean
