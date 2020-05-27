all: streamblast-m regexps test testelastic
regexps: fwsyslog.re adminlog.re sfw.re switches.re fwx.re

streamblast-m: logxre.c
	gcc -O2 -o $@ $< -lpcre2-8

%.re: %.re.in
	cat $< | tr -d '\n' > $@

test:
	./streamblast-m ../data/fwsyslog-twolines fwsyslog.re
	./streamblast-m ../data/adminlog-twolines adminlog.re

testelastic:
	./streamblast-m ../data/fwsyslog-twolines fwsyslog.re elastic
	./streamblast-m ../data/adminlog-twolines adminlog.re elastic
