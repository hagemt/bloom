CC = gcc
CFLAGS = -c -Wall -Wextra
DFLAGS = -g -O0 -pedantic
LFLAGS = -lm -lcalg
PFLAGS = -pg
RFLAGS = -DNDEBUG -O3
WFLAGS = -Wall -Wextra -pedantic

all : debug release

bloom_debug.o : bloom.c file_entry.h file_info.h file_hash.h
	$(CC) $(CFLAGS) $(DFLAGS) bloom.c -o bloom_debug.o

bloom_profile.o : bloom.c file_entry.h file_info.h file_hash.h
	$(CC) $(CFLAGS) $(DFLAGS) $(PFLAGS) bloom.c -o bloom_profile.o

bloom_release.o : bloom.c file_entry.h file_info.h file_hash.h
	$(CC) $(CFLAGS) $(RFLAGS) bloom.c -o bloom_release.o

debug: bloom_debug.o
	$(CC) $(LFLAGS) $(DFLAGS) bloom_debug.o -o bloom_debug

profile: bloom_profile.o
	$(CC) $(LFLAGS) $(DFLAGS) $(PFLAGS) bloom_profile.o -o bloom_profile
	./bloom_profile ~
	gprof ./bloom_profile

release: bloom_release.o
	$(CC) $(LFLAGS) $(RFLAGS) bloom_release.o -o bloom_release
	strip bloom_release

clean:
	$(RM) bloom_debug
	$(RM) bloom_debug.o
	$(RM) bloom_profile
	$(RM) bloom_profile.o
	$(RM) bloom_release
	$(RM) bloom_release.o
	$(RM) gmon.out
