#$Id: Makefile 347 2017-05-14 09:32:29Z dima $

CC=gcc

HFILE_PATH=../src

#DBGFLAG   ?= -O3 -march=native -mtune=native -funroll-loops
DBGFLAG = -ggdb3

CFLAGS= -std=gnu11 -D_GNU_SOURCE -D_REENTRANT  $(DBGFLAG) -fPIC -Wall -Wno-parentheses -Wno-switch -Wno-pointer-sign -Wno-trampolines -Wno-unused-result -I $(HFILE_PATH)/

LDFLAGS= -lmicrohttpd -lcrypto -lcmph -luuid -lmagic -lm -lrt

SRC= $(wildcard *.c)
OBJS= $(SRC:.c=.o) 

LOBJS= $(filter-out main.o,$(SRC:.c=.o))

DFILES= $(SRC:.c=.d)
HFILES= $(wildcard *.h)

PROG1=http
PROG2=http_cache


GOALS=$(PROG1)

.PHONY: all test doc docs clean dist install

all:	$(DFILES) $(GOALS)

doc docs:
	rm -Rf docs
	doxygen


$(DFILES): $(HFILES)


$(PROG1): http.c $(HFILE_PATH)/libhfile.a
	$(CC) $(CFLAGS) $^ $(LDFLAGS) -o $@

$(PROG2): http_cache.c $(HFILE_PATH)/libhfile.a
	$(CC) $(CFLAGS) $^ -lcurl $(LDFLAGS) -o $@


%.d:	%.c
	$(CC) -MM -MG $(CFLAGS) $< > $@

install: /usr/local/lib/$(PROG1) /usr/local/lib/$(PROG2)

/usr/local/lib/$(PROG1): $(PROG1)
	cp $^ $@

/usr/local/lib/$(PROG2): $(PROG2)
	cp $^ $@

dist clean:
	rm -fR $(OBJS) $(DFILES) $(PROG1) $(PROG2) semantic.cache* *.tmp *.tmp~ docs *.inc


ifeq (,$(findstring $(MAKECMDGOALS),dist clean depend doc docs))
-include $(DFILES)
endif
