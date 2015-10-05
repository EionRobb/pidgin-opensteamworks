
CC ?= gcc
CFLAGS	?= -O2 -g -pipe
PKG_CONFIG ?= pkg-config

DIR_PERM	= 0755
FILE_PERM	= 0644

# Global libraries
LIBPURPLE_CFLAGS    +=  $(shell $(PKG_CONFIG) --cflags glib-2.0 json-glib-1.0 purple gnome-keyring-1)
LIBPURPLE_LIBS      +=  $(shell $(PKG_CONFIG) --libs glib-2.0 json-glib-1.0 purple)
PLUGIN_DIR_PURPLE    =  $(shell $(PKG_CONFIG) --variable=plugindir purple)
DATA_ROOT_DIR_PURPLE =  $(shell $(PKG_CONFIG) --variable=datarootdir purple)

# Crypt backend to use
STEAM_CRYPT_BACKEND ?= nss

ifeq ($(STEAM_CRYPT_BACKEND), nss)
	LIBPURPLE_CFLAGS += $(shell ${PKG_CONFIG} --cflags nss)
	LIBPURPLE_LIBS += $(shell ${PKG_CONFIG} --libs nss)
else ifeq ($(STEAM_CRYPT_BACKEND), gcrypt)
	LIBPURPLE_CFLAGS += $(shell libgcrypt-config --cflags) -DUSE_GCRYPT_CRYPTO
	LIBPURPLE_LIBS += $(shell libgcrypt-config --libs)
endif

STEAM_SOURCES = \
	steam_connection.c \
	libsteam.c 

.PHONY:	all clean install
all: libsteam.so
install:
	mkdir -m $(DIR_PERM) -p $(DESTDIR)$(PLUGIN_DIR_PURPLE)
	install -m $(FILE_PERM) libsteam.so $(DESTDIR)$(PLUGIN_DIR_PURPLE)/$(PRPL_NAME)
	mkdir -m $(DIR_PERM) -p $(DESTDIR)$(DATA_ROOT_DIR_PURPLE)/pixmaps/pidgin/protocols/16
	install -m $(FILE_PERM) steam16.png $(DESTDIR)$(DATA_ROOT_DIR_PURPLE)/pixmaps/pidgin/protocols/16/steam.png
	mkdir -m $(DIR_PERM) -p $(DESTDIR)$(DATA_ROOT_DIR_PURPLE)/pixmaps/pidgin/protocols/22
	install -m $(FILE_PERM) steam22.png $(DESTDIR)$(DATA_ROOT_DIR_PURPLE)/pixmaps/pidgin/protocols/22/steam.png
	mkdir -m $(DIR_PERM) -p $(DESTDIR)$(DATA_ROOT_DIR_PURPLE)/pixmaps/pidgin/protocols/48
	install -m $(FILE_PERM) steam48.png $(DESTDIR)$(DATA_ROOT_DIR_PURPLE)/pixmaps/pidgin/protocols/48/steam.png
clean:
	rm -f libsteam.so

libsteam.so: $(STEAM_SOURCES)
	$(CC) -Wall -I. -fPIC $(CFLAGS) $(STEAM_SOURCES) -o $@ $(LIBPURPLE_CFLAGS) $(LIBPURPLE_LIBS) -shared
