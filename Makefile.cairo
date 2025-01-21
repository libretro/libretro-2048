# Things specific to the cairo backend
ifeq (,$(TARGET_NAME))
$(error "Don't use this Makefile directly.  Instead use `make -f Makefile.libretro WANT_CAIRO=1`")
endif

ifeq (,$(HOST))
PKG_CONFIG := pkg-config
else
PKG_CONFIG := $(HOST)-pkg-config
endif

packages=cairo pixman-1
ifeq ($(words $(packages)),$(words $(shell $(PKG_CONFIG) --modversion --silence-errors $(packages))))
USE_STATIC_LIBS := 0
else
USE_STATIC_LIBS := 1
endif

DEP_INSTALL_DIR := $(CURDIR)/tmp

ifneq (0,$(USE_STATIC_LIBS))
CFLAGS += -I$(DEP_INSTALL_DIR)/include
LFLAGS := -L$(DEP_INSTALL_DIR)/lib
LIBS += $(DEP_INSTALL_DIR)/lib/libcairo.a $(DEP_INSTALL_DIR)/lib/libpixman-1.a
LIBS += -lpthread -lfreetype -lfontconfig
else
packages += freetype2 fontconfig
CFLAGS += $(shell $(PKG_CONFIG) --cflags $(packages))
LFLAGS := $(shell $(PKG_CONFIG) --libs-only-L --libs-only-other $(packages))
LIBS += $(shell $(PKG_CONFIG) --libs-only-l $(packages))
endif

host_opts=
ifneq ($(HOST),)
       host_opts := --host=$(HOST)
endif

deps: $(DEP_INSTALL_DIR)/lib/libcairo.a

ifneq (0,$(USE_STATIC_LIBS))
$(OBJECTS): deps
endif

$(DEP_INSTALL_DIR)/lib/libpixman-1.a:
	cd pixman; \
		libtoolize; aclocal; automake --add-missing; \
		./configure $(host_opts) --enable-shared=no --enable-static=yes $(with_fpic) CFLAGS="-fno-lto" --prefix=$(DEP_INSTALL_DIR) && \
		make; make install

$(DEP_INSTALL_DIR)/lib/libcairo.a: $(DEP_INSTALL_DIR)/lib/libpixman-1.a
	cd cairo; \
		aclocal; \
		./configure $(host_opts) --enable-static=yes --enable-ft=yes --enable-shared=no \
			--enable-gobject=no --enable-trace=no --enable-interpreter=no \
			--enable-symbol-lookup=no --enable-svg=no --enable-pdf=no --enable-ps=no \
			--enable-wgl=no --enable-glx=no --enable-egl=no --disable-valgrind \
			--enable-silent-rules --enable-png=no  --enable-xlib=no \
			--enable-drm=no --enable-xcb-drm=no --enable-drm-xr=no --disable-lto  \
			$(with_fpic) CFLAGS="-fno-lto" \
			pixman_CFLAGS="-I$(DEP_INSTALL_DIR)/include/pixman-1" pixman_LIBS="-L$(DEP_INSTALL_DIR)/lib -lpixman-1" --prefix=$(DEP_INSTALL_DIR) && \
		make; make install

clean_cairo:
	cd cairo; [[ -f Makefile ]] && make distclean || true

clean_pixman:
	cd pixman; [[ -f Makefile ]] && make distclean || true

clean_deps:
ifneq (0,$(USE_STATIC_LIBS))
	$(MAKE) -f Makefile.libretro clean_cairo clean_pixman
endif
	rm -Rf $(DEP_INSTALL_DIR)

clean: clean_deps

.PHONY: clean_pixman clean_cairo clean_deps deps

