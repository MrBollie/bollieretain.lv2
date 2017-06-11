#!/usr/bin/make -f

include Makefile.mk

# --------------------------------------------------------------

PREFIX  ?= /usr/local
DESTDIR ?=
BUILDDIR ?= build/bollieretain.lv2

# --------------------------------------------------------------
# Default target is to build all plugins

all: build
build: bollieretain

# --------------------------------------------------------------
# bollieretain build rules

bollieretain: $(BUILDDIR) $(BUILDDIR)/bollieretain$(LIB_EXT) $(BUILDDIR)/manifest.ttl $(BUILDDIR)/modgui.ttl $(BUILDDIR)/bollieretain.ttl $(BUILDDIR)/modgui

$(BUILDDIR):
	mkdir -p $(BUILDDIR)

$(BUILDDIR)/bollieretain.o: src/bollie-retain.c
	$(CC) $^ $(BUILD_C_FLAGS) $(LINK_FLAGS) -lm -o $@ -c

$(BUILDDIR)/bollieretain$(LIB_EXT): $(BUILDDIR)/bollieretain.o
	$(CC) $^ $(BUILD_C_FLAGS) $(LINK_FLAGS) -lm $(SHARED) -o $@

$(BUILDDIR)/manifest.ttl: lv2ttl/manifest.ttl.in
	sed -e "s|@LIB_EXT@|$(LIB_EXT)|" $< > $@

$(BUILDDIR)/modgui.ttl: lv2ttl/modgui.ttl.in
	sed -e "s|@LIB_EXT@|$(LIB_EXT)|" $< > $@

$(BUILDDIR)/bollieretain.ttl: lv2ttl/bollieretain.ttl
	cp $< $@

$(BUILDDIR)/modgui: modgui
	mkdir -p $@ 
	cp -rv $^/* $@/

# --------------------------------------------------------------

clean:
	rm -f $(BUILDDIR)/bollieretain* $(BUILDDIR)/*.ttl
	rm -fr $(BUILDDIR)/modgui

# --------------------------------------------------------------

install: build
	echo "Install"
	install -d $(DESTDIR)$(PREFIX)/lib/lv2/bollieretain.lv2
	install -d $(DESTDIR)$(PREFIX)/lib/lv2/bollieretain.lv2/modgui

	install -m 644 $(BUILDDIR)/*.so  $(DESTDIR)$(PREFIX)/lib/lv2/bollieretain.lv2/
	install -m 644 $(BUILDDIR)/*.ttl $(DESTDIR)$(PREFIX)/lib/lv2/bollieretain.lv2/
	cp -rv $(BUILDDIR)/modgui/* $(DESTDIR)$(PREFIX)/lib/lv2/bollieretain.lv2/modgui/

# --------------------------------------------------------------
uninstall:
	echo "Uninstall"
	rm -fr $(DESTDIR)$(PREFIX)/lib/lv2/bollieretain.lv2
	
# --------------------------------------------------------------
