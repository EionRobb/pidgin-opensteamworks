
COMPILER = gcc
PKG_CONFIG ?= pkg-config

# Global libraries
LIBPURPLE_CFLAGS += $(shell ${PKG_CONFIG} --cflags glib-2.0 json-glib-1.0 purple gnome-keyring-1)
LIBPURPLE_LIBS += $(shell ${PKG_CONFIG} --libs glib-2.0 json-glib-1.0 purple)

CFLAGS = ${LIBPURPLE_CFLAGS}
LIBS = ${LIBPURPLE_LIBS}

# Crypt backend to use
CRYPT_BACKEND = nss

ifeq (${CRYPT_BACKEND}, nss)
	CFLAGS += $(shell ${PKG_CONFIG} --cflags nss)
	LIBS += $(shell ${PKG_CONFIG} --libs nss)
else ifeq (${CRYPT_BACKEND}, gcrypt)
	CFLAGS += $(shell libgcrypt-config --cflags) -DUSE_GCRYPT_CRYPTO
	LIBS += $(shell libgcrypt-config --libs)
endif

STEAM_SOURCES = \
	steam_connection.c \
	libsteam.c 

.PHONY:	all clean install
all: libsteam.so
clean:
	rm -f libsteam.so

libsteam.so: ${STEAM_SOURCES}
	${COMPILER} -Wall -I. -g -O2 -fPIC -pipe ${STEAM_SOURCES} -o $@ ${CFLAGS} ${LIBS} -shared
