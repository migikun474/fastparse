# Build and install the library:  make && sudo make install [PREFIX=/usr/local] [DESTDIR=...]
# Then:  cc app.c $(pkg-config --cflags --libs fastparse)
# Portable by default (-march=x86-64 on x86): SSE and AVX2 are still used at runtime when the
# CPU has them. Tests and benchmarks are built by scripts/build.sh, not here.
VERSION = 1.1
SOVERSION = 1
PREFIX ?= /usr/local
LIBDIR = $(PREFIX)/lib
INCLUDEDIR = $(PREFIX)/include
CFLAGS ?= -O2

ifeq ($(shell uname -m),x86_64)
ARCHFLAGS = -march=x86-64
SRCS = $(wildcard src/*.c) $(wildcard src/*.S)
else
SRCS = $(wildcard src/*.c)
endif
OBJS = $(SRCS:src/%=build/pic/%.o)
SO = libfastparse.so.$(VERSION)

all: build/libfastparse.a build/$(SO) build/fastparse.pc

build/pic/%.o: src/%
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(ARCHFLAGS) -fPIC -Wall -Wextra -Iinclude -c $< -o $@

build/libfastparse.a: $(OBJS)
	rm -f $@
	ar rcs $@ $^

build/$(SO): $(OBJS)
	$(CC) -shared -Wl,-soname,libfastparse.so.$(SOVERSION) -o $@ $^

# Paths relative to the .pc file, so an unpacked release tarball works wherever it lands.
build/fastparse.pc: Makefile
	@mkdir -p $(@D)
	printf '%s\n' 'prefix=$${pcfiledir}/../..' 'libdir=$${prefix}/lib' \
	    'includedir=$${prefix}/include' '' 'Name: fastparse' \
	    'Description: SIMD parsers for market-data numbers into fixed-point integers' \
	    'Version: $(VERSION)' 'Libs: -L$${libdir} -lfastparse' 'Cflags: -I$${includedir}' >$@

install: all
	install -d $(DESTDIR)$(LIBDIR)/pkgconfig $(DESTDIR)$(INCLUDEDIR)
	install -m 644 build/libfastparse.a $(DESTDIR)$(LIBDIR)/
	install -m 755 build/$(SO) $(DESTDIR)$(LIBDIR)/
	ln -sf $(SO) $(DESTDIR)$(LIBDIR)/libfastparse.so.$(SOVERSION)
	ln -sf libfastparse.so.$(SOVERSION) $(DESTDIR)$(LIBDIR)/libfastparse.so
	install -m 644 include/fastparse.h $(DESTDIR)$(INCLUDEDIR)/
	install -m 644 build/fastparse.pc $(DESTDIR)$(LIBDIR)/pkgconfig/

uninstall:
	rm -f $(DESTDIR)$(LIBDIR)/libfastparse.a $(DESTDIR)$(LIBDIR)/libfastparse.so* \
	    $(DESTDIR)$(INCLUDEDIR)/fastparse.h $(DESTDIR)$(LIBDIR)/pkgconfig/fastparse.pc

clean:
	rm -rf build/pic build/libfastparse.a build/$(SO) build/fastparse.pc

.PHONY: all install uninstall clean
