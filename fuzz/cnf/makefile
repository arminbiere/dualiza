all: cnfdd cnfuzz
cnfdd: cnfdd.c
	gcc -o cnfdd cnfdd.c
cnfuzz: cnfuzz.c
	gcc -o cnfuzz cnfuzz.c
clean:
	-killall -9 -q fuzz-dualiza-sat-cnf.sh
	-killall -9 -q fuzz-dualiza-count-cnf.sh
	-killall -9 -q cnfuzz
	-killall -9 -q cmp-dualiza-sat-cnf.sh
	-killall -9 -q cmp-dualiza-count-cnf.sh
	-killall -9 -q cnfdd
	rm -f bug-*.cnf red-*.cnf cnfuzz cnfdd
