STRIP    ?= $(shell which strip)
UPX      ?= $(shell which upx)

PROG      = abused

prefix    = /usr/bin
BINDIR    = /usr/local/bin
INSTALL   = $(prefix)/install
BASE_DIR  = $(shell pwd)
SRCS_DIR  = $(BASE_DIR)/src
OBJS_DIR  = $(BASE_DIR)/obj
INCL_DIR  = $(BASE_DIR)/include

INCLUDES  = -I$(INCL_DIR)

CFLAGS   += -O3 -Wall -g $(INCLUDES)
LDFLAGS  +=
LIBS     += -lresolv

SRCS      = $(wildcard $(SRCS_DIR)/*.c)
OBJS      = $(patsubst $(SRCS_DIR)/%.c,$(OBJS_DIR)/%.o,$(SRCS))
DEPS      = $(wildcard $(INCL_DIR)/*.h)

all: makedirs $(PROG)

makedirs:
	mkdir -p $(OBJS_DIR)

$(PROG): $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) $^ $(LIBS) -o $@
ifneq ("$(STRIP)","")
	$(STRIP) $@
else
	@echo -e '\e[1;33m*** NOTE: Not stripping binary ***\e[0m'
endif
ifneq ("$(UPX)","")
	$(UPX) -9 $@
else
	@echo -e '\e[1;33m*** NOTE: Not compressing binary ***\e[0m'
endif

$(OBJS): $(OBJS_DIR)/%.o : $(SRCS_DIR)/%.c $(DEPS)
	$(CC) -c $(CFLAGS) $(LDFLAGS) $< $(LIBS) -o $@

install: all
	$(INSTALL) -d $(BINDIR)
	$(INSTALL) -m 0755 $(PROG) $(BINDIR)

clean:
	$(RM) $(PROG) $(OBJS_DIR)/*.o *~

.PHONY: makedirs
