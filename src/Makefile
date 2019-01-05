#$Id: Makefile 347 2017-05-14 09:32:29Z dima $

CC=gcc

#DBGFLAG   ?= -O3 -march=native -mtune=native -funroll-loops
DBGFLAG = -ggdb3

CFLAGS= -std=gnu11 -D_GNU_SOURCE -D_REENTRANT  $(DBGFLAG) -fPIC -Wall -Wno-parentheses -Wno-switch -Wno-pointer-sign -Wno-trampolines -Wno-unused-result

LDFLAGS= -lcrypto -lcmph -luuid -lm -lrt

SRC= $(wildcard *.c)
OBJS= $(SRC:.c=.o) 

LOBJS= $(filter-out main.o,$(SRC:.c=.o))

DFILES= $(SRC:.c=.d)
HFILES= $(wildcard *.h)

LIBNAME=libhfile.a
SONAME=libhfile.so

PROG=hugefile

GOALS=$(SONAME) $(LIBNAME) $(PROG)
# $(M4DST) $(M4DH)

.PHONY: all test lib doc docs clean dist install

all:	$(DFILES) $(GOALS)

lib: $(LIBNAME)

doc docs:
	rm -Rf docs
	doxygen


$(DFILES): $(HFILES)


$(PROG): $(OBJS)
	$(CC) $^ $(LDFLAGS) -o $@ $(POSTLD)


$(LIBNAME): $(LOBJS)
	ar rcsu $@ $^

$(SONAME): $(LOBJS)
	gcc -shared $^ -o $@

%.d:	%.c
	$(CC) -MM -MG $(CFLAGS) $< > $@

install: $(SONAME) $(LIBNAME) $(PROG)
	cp $(SONAME) /usr/local/lib
	cp $(LIBNAME) /usr/local/lib
	cp $(PROG) /usr/local/bin

dist clean:
	rm -fR $(OBJS) $(DFILES) $(LIBNAME) $(SONAME) $(PROG) semantic.cache* *.tmp *.tmp~ docs *.inc


ifeq (,$(findstring $(MAKECMDGOALS),dist clean depend doc docs))
-include $(DFILES)
endif


