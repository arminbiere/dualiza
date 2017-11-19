all: cnfdd cnfuzz
cnfdd: cnfdd.c
	gcc -o cnfdd cnfdd.c
cnfuzz: cnfuzz.c
	gcc -o cnfuzz cnfuzz.c
clean:
	-killall -q fuzzsat.sh fuzzcount.sh
	-killall -q cnfdd cnfuzz
	-killall -q cmpsat.sh cmpcount.sh
	rm -f bug-*.cnf reduced-*.cnf cnfuzz cnfdd
