CC=gcc
CFLAGS=-I. -W -Wall -Wextra -Wuninitialized -Wstrict-aliasing
DEPS=util.h
OBJ=util.o main.o
LDLIBS=-lpthread -lnuma -lm

.PHONY: all
all: cxltest

%.o: %.c $(DEPS)
        $(CC) -c -o $@ $< $(CFLAGS)

cxltest: $(OBJ)
        $(CC) -o $@ $^ $(CFLAGS) $(LDLIBS)

.PHONY: clean
clean:
        $(RM) *~ *.o cxltest

debug: CFLAGS+=-g
debug: cxltest
~                             