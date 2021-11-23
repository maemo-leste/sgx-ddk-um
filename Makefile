.POSIX:

all: pvrsrvinit

pvrsrvinit: pvrsrvinit.c
	gcc -Wall -Wextra -std=c99 -ldl $< -o $@

install: all
	mkdir -p $(DESTDIR)/etc/init.d
	mkdir -p $(DESTDIR)/usr/bin
	cp -f scripts/powervr.init $(DESTDIR)/etc/init.d/powervr
	cp -f pvrsrvinit $(DESTDIR)/usr/bin
	chmod 755 $(DESTDIR)/etc/init.d/powervr
	chmod 755 $(DESTDIR)/usr/bin/pvrsrvinit

clean:
	rm -f pvrsrvinit

.PHONY: all install
