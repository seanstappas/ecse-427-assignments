CC=gcc
LIBS=-lrt -lpthread
CFLAGS=-g -std=c99 -D _BSD_SOURCE
SOURCE1=disk_emu.c ssfs.c test.c

EXEC1=ssfs

ssfs: $(SOURCE1)
	$(CC) -o $(EXEC1) $(CFLAGS) $(SOURCE1) $(LIBS)

clean:
	rm $(EXEC1)
