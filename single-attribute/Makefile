all: cextract test

cextract: cextract.c
	gcc -O2 -o cextract cextract.c

test:
	./cextract ../data/fwsyslog-twolines src: plain
