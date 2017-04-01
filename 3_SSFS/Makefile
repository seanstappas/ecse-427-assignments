# To compile with test1, make test1
# To compile with test2, make test2
CC = clang -g -Wall
EXECUTABLE=sfs

SOURCES_TEST1= disk_emu.c sfs_api.c sfs_test1.c tests.c
SOURCES_TEST2= disk_emu.c sfs_api.c sfs_test2.c tests.c
SOURCES_TEST3= disk_emu.c sfs_api.c sean_test.c

test1: $(SOURCES_TEST1) 
	$(CC) -o $(EXECUTABLE) $(SOURCES_TEST1)

test2: $(SOURCES_TEST2)
	$(CC) -o $(EXECUTABLE) $(SOURCES_TEST2)

test3: $(SOURCES_TEST3)
	$(CC) -o $(EXECUTABLE) $(SOURCES_TEST3)

clean:
	rm $(EXECUTABLE)
