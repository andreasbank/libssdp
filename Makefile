STRIP    ?= $(shell which strip)
UPX      ?= $(shell which upx)
UPX_LEVEL = -9

PROG      = scanssdp
PROG_SO   = libssdp.so

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
LIBS     +=

SRCS      = $(wildcard $(SRCS_DIR)/*.c)
OBJS      = $(patsubst $(SRCS_DIR)/%.c,$(OBJS_DIR)/%.o,$(SRCS))
OBJS_FPIC = $(patsubst $(OBJS_DIR)/%.o,$(OBJS_DIR)/%_fpic.o,$(OBJS))
DEPS      = $(wildcard $(INCL_DIR)/*.h)

all: makedirs $(PROG) $(PROG_SO)

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
	$(UPX) $(UPX_LEVEL) $@
else
	@echo -e '\e[1;33m*** NOTE: Not compressing binary ***\e[0m'
endif

$(PROG_SO): $(OBJS_FPIC)
	$(CC) $(CFLAGS) -shared $(LDFLAGS) $^ $(LIBS) -o $@
ifneq ("$(STRIP)","")
	$(STRIP) $@
else
	@echo -e '\e[1;33m*** NOTE: Not stripping library ***\e[0m'
endif
ifneq ("$(UPX)","")
	$(UPX) $(UPX_LEVEL) $@
else
	@echo -e '\e[1;33m*** NOTE: Not compressing library ***\e[0m'
endif

$(OBJS): $(OBJS_DIR)/%.o : $(SRCS_DIR)/%.c $(DEPS)
	$(CC) -c $(CFLAGS) $(LDFLAGS) $< -o $@

$(OBJS_FPIC): $(OBJS_DIR)/%_fpic.o : $(SRCS_DIR)/%.c $(DEPS)
	$(CC) -c $(CFLAGS) -fPIC $(LDFLAGS) $< -o $@

install: all
	$(INSTALL) -d $(BINDIR)
	$(INSTALL) -m 0755 $(PROG) $(BINDIR)

clean:
	$(RM) $(PROG) $(OBJS_DIR)/*.o *~

.PHONY: makedirs
