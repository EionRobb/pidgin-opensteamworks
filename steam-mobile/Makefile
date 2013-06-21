
COMPILER = gcc

LIBPURPLE_CFLAGS += $(shell pkg-config --cflags glib-2.0 json-glib-1.0 purple nss)
LIBPURPLE_LIBS += $(shell pkg-config --libs glib-2.0 json-glib-1.0 purple nss)
TELEPATHY_CFLAGS += $(shell pkg-config --cflags gnome-keyring-1)
TELEPATHY_LIBS += $(shell pkg-config --libs gnome-keyring-1)

STEAM_SOURCES = \
	steam_connection.c \
	libsteam.c 

.PHONY:	all clean install
all: libsteam.so
clean:
	rm libsteam.so

libsteam.so: ${STEAM_SOURCES}
	${COMPILER} ${LIBPURPLE_CFLAGS} ${LIBPURPLE_LIBS} -Wall -I. -g -O2 -fPIC -pipe ${STEAM_SOURCES} -o $@ -shared 

libsteam-telepathy.so: ${STEAM_SOURCES}
	${COMPILER} ${LIBPURPLE_CFLAGS} ${LIBPURPLE_LIBS} ${TELEPATHY_CFLAGS} ${TELEPATHY_LIBS} -DTELEPATHY -Wall -I. -g -O2 -fPIC -pipe ${STEAM_SOURCES} -o $@ -shared 
