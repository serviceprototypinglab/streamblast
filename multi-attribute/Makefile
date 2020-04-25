all: streamblast-m regexps test
regexps: fwsyslog.re adminlog.re sfw.re switches.re fwx.re

streamblast-m: logxre.c
	gcc -O2 -o $@ $< -lpcre2-8

%.re: %.re.in
	cat $< | tr -d '\n' > $@

test:
	./streamblast-m ../data/fwsyslog-twolines fwsyslog.re
