.phony all:
all: schdlr

schdlr: schdlr.c
	gcc -pthread schdlr.c -o schdlr

.PHONY clean:
clean:
	-rm -rf *.o *.exe
