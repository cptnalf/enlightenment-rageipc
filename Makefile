
CC=gcc
CFLAGS = -Wall -D_SERVER_ -g `pkg-config ecore-ipc --cflags` `pkg-config eet --cflags` `pkg-config ecore-file --cflags`
LIBS = `pkg-config ecore-ipc --libs` `pkg-config eet --libs ` `pkg-config ecore-file --libs` -lsqlite3
OBJS = volume.o database.o database_storage.o media_storage.o

all: raged

raged: $(OBJS) raged_main.o
	gcc -o $@  $(CFLAGS) $(LIBS) $(OBJS) raged_main.o


clean:
	rm -f $(OBJS)
	rm -f ragec raged

