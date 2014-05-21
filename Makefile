PROGS     = abused

prefix    = /usr/bin
BINDIR    = /usr/local/bin
INSTALL   = $(prefix)/install

CFLAGS   += -O3 -Wall -g
LDLIBS   += -lresolv


all: $(PROGS)

install: all
	$(INSTALL) -d $(BINDIR)
	$(INSTALL) -m 0755 $(PROGS) $(BINDIR)

clean:
	$(RM) $(PROGS) $(PROGS).o *~
