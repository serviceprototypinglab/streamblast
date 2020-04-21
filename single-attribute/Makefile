all: streamblast-s test

streamblast-s: cextract.c
	gcc -O2 -o $@ $<

test:
	./streamblast-s ../data/fwsyslog-twolines src: plain
