SHELL=/bin/bash
CC=gcc
CFLAGS=-Wall -g
LIBS=-lpthread

as1: $(patsubst as1/%.c, as1/%.o, $(shell find as1 -type f))
	${CC} ${CFLAGS} ${LIBS} -o $@/main $<

as2: $(patsubst as2/%.c, as2/%.o, $(shell find as2 -type f))
	${CC} ${CFLAGS} ${LIBS} -o $@/main $<

clean:
	rm -f as1/*.o as2/*.o as1/main as2/main

*.o:
	${CC} ${CFLAGS} -c $<

.PHONY: clean