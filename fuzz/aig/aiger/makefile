CC=cc
CFLAGS=-O

all: aigand aigdd aigfuzz aiginfo aigstrip

aigand: aigand.o aiger.o makefile
	$(CC) $(CFLAGS) -o $@ aigand.o aiger.o
aigdd: aigdd.o aiger.o makefile
	$(CC) $(CFLAGS) -o $@ aigdd.o aiger.o
aigfuzz: aigfuzz.o aigfuzzlayers.o aiger.o makefile
	$(CC) $(CFLAGS) -o $@ aigfuzz.o aigfuzzlayers.o aiger.o
aiginfo: aiginfo.o aiger.o makefile
	$(CC) $(CFLAGS) -o $@ aiginfo.o aiger.o
aigstrip: aigstrip.o aiger.o makefile
	$(CC) $(CFLAGS) -o $@ aigstrip.o aiger.o

aiger.o: aiger.c aiger.h makefile
	$(CC) $(CFLAGS) -c $<
aigand.o: aigand.c aiger.h makefile
	$(CC) $(CFLAGS) -c $<
aigdd.o: aigdd.c aiger.h makefile
	$(CC) $(CFLAGS) -c $<
aigfuzz.o: aigfuzz.c aigfuzz.h aiger.h makefile
	$(CC) $(CFLAGS) -c $<
aigfuzzlayers.o: aigfuzzlayers.c aigfuzz.h aiger.h makefile
	$(CC) $(CFLAGS) -c $<
aiginfo.o: aiginfo.c aiger.h makefile
	$(CC) $(CFLAGS) -c $<
aigstrip.o: aigstrip.c aiger.h makefile
	$(CC) $(CFLAGS) -c $<

clean:
	rm -f *.o aigand aigdd aigfuzz aiginfo aigstrip

.PHONY: all clean
