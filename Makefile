CFLAGS += -O3 -fomit-frame-pointer -g -Wall
libdir = /usr/lib
LDFLAGS += -lpthread -rpath $(libdir) -version-info 1
CC = gcc
LD = ld
ARCH = $(shell uname -m)

# Check Architecture
SUPPORTED_ARCH = NO

ifeq ($(ARCH), x86_64)
SUPPORTED_ARCH = YES
endif

ifeq ($(SUPPORTED_ARCH), NO)
$(error Your architecture $(ARCH) is not currently supported. See README.)
endif

define compile_rule
	libtool --mode=compile --tag=CC \
	$(CC) $(CFLAGS) $(CPPFLAGS) -Iinclude-$(ARCH) -c $<
endef

define link_rule
	libtool --mode=link --tag=CC \
	$(LD) $(LDFLAGS) -o $@ $^ $(LDLIBS)
endef

LIBS = libssmalloc.la
libssmalloc_OBJS = ssmalloc.lo

%.lo: %.c
	$(call compile_rule)

all: libssmalloc.la

libssmalloc.la: $(libssmalloc_OBJS)
	$(call link_rule)
	cp .libs/libssmalloc.so ./
	cp .libs/libssmalloc.a ./

install/%.la: %.la
	libtool --mode=install \
	install -c $(notdir $@) $(libdir)/$(notdir $@)

install: $(addprefix install/,$(LIBS))
	libtool --mode=finish $(libdir)

clean:
	libtool --mode=clean rm *.la *.lo *.a *.so -f

