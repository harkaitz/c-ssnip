PROJECT    =c-ssnip
VERSION    =1.0.0
DESTDIR    =
PREFIX     =/usr/local
CC         =gcc -pedantic-errors -std=c99 -Wall
LIBS       =-lutil
PROGRAMS   =./ssnip$(EXE)
##
CFLAGS_ALL =$(LDFLAGS) $(CFLAGS) $(CPPFLAGS) -DSSNIP_TEMPLATE_DIR='"$(PREFIX)/share/ssnip"'
##
all: $(PROGRAMS)
install: $(PROGRAMS)
	@install -d                $(DESTDIR)$(PREFIX)/bin
	install -m755 $(PROGRAMS) $(DESTDIR)$(PREFIX)/bin
clean:
	rm -f $(PROGRAMS)
./ssnip$(EXE): main.c ssnip.c ssnip.h
	$(CC) -o $@ main.c ssnip.c $(CFLAGS_ALL) $(LIBS)
## -- BLOCK:license --
install: install-license
install-license: 
	@mkdir -p $(DESTDIR)$(PREFIX)/share/doc/$(PROJECT)
	cp LICENSE  $(DESTDIR)$(PREFIX)/share/doc/$(PROJECT)
## -- BLOCK:license --
## -- BLOCK:man --
install: install-man
install-man:
	@mkdir -p $(DESTDIR)$(PREFIX)/share/man/man1
	cp ./ssnip.1 $(DESTDIR)$(PREFIX)/share/man/man1
## -- BLOCK:man --
