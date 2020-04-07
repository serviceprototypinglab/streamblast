all: logxre fwsyslog.re test

logxre: logxre.c
	gcc -O2 -o logxre logxre.c -lpcre2-8

fwsyslog.re: fwsyslog.re.in
	cat fwsyslog.re.in | tr -d '\n' > fwsyslog.re

test:
	./logxre ../data/fwsyslog-twolines fwsyslog.re
