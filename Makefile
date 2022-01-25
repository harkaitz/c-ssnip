## Configuration
DESTDIR    =
PREFIX     =/usr/local
AR         =ar
CC         =gcc
CFLAGS     =-Wall -g
CPPFLAGS   =
LIBS       =-lutil
CFLAGS_ALL =$(LDFLAGS) $(CFLAGS) $(CPPFLAGS) -DSSNIP_TEMPLATE_DIR='"$(PREFIX)/share/ssnip"'
PROGRAMS   =./ssnip
MAN1       =./ssnip.1

## Help string.
all:
help:
	@echo "all     : Build everything."
	@echo "clean   : Clean files."
	@echo "install : Install all produced files."

## Programs.
./ssnip: main.c ssnip.c ssnip.h
	$(CC) -o $@ main.c ssnip.c $(CFLAGS_ALL) $(LIBS)

## install and clean.
all: $(PROGRAMS)
install: $(PROGRAMS)
	install -d                $(DESTDIR)$(PREFIX)/bin
	install -m755 $(PROGRAMS) $(DESTDIR)$(PREFIX)/bin
	install -d                $(DESTDIR)$(PREFIX)/share/man/man1
	install -m644 $(MAN1)     $(DESTDIR)$(PREFIX)/share/man/man1
clean:
	rm -f $(PROGRAMS)
