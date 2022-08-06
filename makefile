#
# Makefile for wordle
#

#DEBUG    := -g -DDEBUG
DEFINES  := -D_POSIX_SOURCE -D_POSIX_C_SOURCE=200809L
WARNINGS := -Wall -Wextra -pedantic
SERVER_LIB := -lmicrohttpd
#OPTIMIZE := -O3

export CFLAGS := -std=c11 $(DEBUG) $(DEFINES) $(WARNINGS) $(OPTIMIZE)
export CC := gcc

all: wordle server

server.o:   server.c

wordle.o:   wordle.c wordle.h wstats.h wdict.h wsolve.h

wstats.o:   wstats.c wordle.h wstats.h wdict.h

wdict.o:    wdict.c wordle.h wdict.h

wpos.o:     wpos.c wordle.h wdict.h wpos.h

wsolve.o:   wsolve.c wordle.h wdict.h wpos.h wsolve.h

wordle:  wordle.o wstats.o wdict.o wpos.o wsolve.o
	    $(CC) $(CFLAGS) -o $@ $^

server.o: server.c wordle.h wstats.h wdict.h wsolve.h

server:  server.o wstats.o wdict.o wpos.o wsolve.o
	    $(CC) $(CFLAGS) -o $@ $^ $(SERVER_LIB)

.PHONY: clean
clean:	  
	  rm *.[o] wordle server

