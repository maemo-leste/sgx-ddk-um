.POSIX:

all: pvrsrvinit

pvrsrvinit: pvrsrvinit.c
	gcc -Wall -Wextra -std=c99 -ldl $< -o $@

install:
	mkdir -p $(DESTDIR)/etc/init.d
	cp -f scripts/powervr.init $(DESTDIR)/etc/init.d/powervr
	chmod 755 $(DESTDIR)/etc/init.d/powervr

clean:
	rm -f pvrsrvinit

.PHONY: all install
