# $Id: Makefile 7 2015-04-05 07:40:16Z dima $


DEST ?=/usr/local

.PHONY: all doc clean install

all:
	make -C src

doc:
	doxygen

clean:
	make -C src clean
	make -C examples clean
	rm -Rf doc
	cd tests/data.out ; ./clean.sh
	cd tests/ ; rm -f *.log

install:
	DEST=$(DEST) make -C src install
