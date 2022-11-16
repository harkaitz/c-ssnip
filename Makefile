DESTDIR    =
PREFIX     =/usr/local
AR         =ar
CC         =gcc
CFLAGS     =-Wall -g
CPPFLAGS   =
LIBS       =-lutil
CFLAGS_ALL =$(LDFLAGS) $(CFLAGS) $(CPPFLAGS) -DSSNIP_TEMPLATE_DIR='"$(PREFIX)/share/ssnip"'
PROGRAMS   =./ssnip$(EXE)


all: $(PROGRAMS)
install: $(PROGRAMS)
	@echo 'I bin/ $(PROGRAMS)'
	@install -d                $(DESTDIR)$(PREFIX)/bin
	@install -m755 $(PROGRAMS) $(DESTDIR)$(PREFIX)/bin
clean:
	@echo "D $(PROGRAMS)"
	@rm -f $(PROGRAMS)

./ssnip$(EXE): main.c ssnip.c ssnip.h
	@echo "B $@ $^"
	@$(CC) -o $@ main.c ssnip.c $(CFLAGS_ALL) $(LIBS)


## -- manpages --
install: install-man1
install-man1:
	@echo 'I share/man/man1/'
	@mkdir -p $(DESTDIR)$(PREFIX)/share/man/man1
	@echo 'I share/man/man1/ssnip.1'
	@cp ./ssnip.1 $(DESTDIR)$(PREFIX)/share/man/man1
## -- manpages --
## -- license --
install: install-license
install-license: LICENSE
	@echo 'I share/doc/c-ssnip/LICENSE'
	@mkdir -p $(DESTDIR)$(PREFIX)/share/doc/c-ssnip
	@cp LICENSE $(DESTDIR)$(PREFIX)/share/doc/c-ssnip
## -- license --
