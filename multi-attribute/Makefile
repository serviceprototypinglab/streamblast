all: streamblast-m fwsyslog.re test

streamblast-m: logxre.c
	gcc -O2 -o $@ $< -lpcre2-8

fwsyslog.re: fwsyslog.re.in
	cat fwsyslog.re.in | tr -d '\n' > fwsyslog.re

test:
	./streamblast-m ../data/fwsyslog-twolines fwsyslog.re
