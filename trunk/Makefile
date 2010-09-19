LIBPURPLE_CFLAGS = -I/usr/include/libpurple -I/usr/local/include/libpurple -DPURPLE_PLUGINS -DENABLE_NLS -lglib-2.0
GLIB_CFLAGS = -I/usr/include/glib-2.0 -I/usr/lib/glib-2.0/include -I/usr/local/include/glib-2.0 -I/usr/local/lib/glib-2.0/include -I/usr/local/include
STEAMWORKS_CFLAGS = -Iopensteamworks/Open\ Steamworks/

all:	libsteamworks.cpp
	gcc libsteamworks.cpp -o libsteamworks.so -shared -dynamic ${LIBPURPLE_CFLAGS} ${GLIB_CFLAGS} ${STEAMWORKS_CFLAGS} -g -Wall -pipe -fPIC -DPIC


libsteamworks.dll:	libsteamworks.cpp libsteamworks.h
	i586-mingw32-g++ libsteamworks.cpp -o libsteamworks.dll -shared ${LIBPURPLE_CFLAGS} ${GLIB_CFLAGS} ${STEAMWORKS_CFLAGS} -g -Wall -pipe -L/root/pidgin/pidgin-2.3.0_win32/libpurple -L/root/pidgin/win32-dev/gtk_2_0/lib -lws2_32 -lpurple 
#-Wno-write-strings -mno-cygwin -Wl,--export-all-symbols
#	upx libsteamworks.dll
