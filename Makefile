CC = gcc
CFLAGS = -c -Wall -Wextra -D_FILE_OFFSET_BITS=64
DFLAGS = -g -O0 -pedantic
IFLAGS = -b -s -v
LFLAGS = -lm -lcalg -lssl
PFLAGS = -g -p -pg
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
	$(CC) $(LFLAGS) $(RFLAGS) $(PFLAGS) bloom_profile.o -o bloom_profile
	./bloom_profile ~
	gprof ./bloom_profile

release: bloom_release.o
	$(CC) $(LFLAGS) $(RFLAGS) bloom_release.o -o bloom_release
	strip bloom_release

install: release
	install $(IFLAGS) -T bloom_release /usr/local/bin/bloom

uninstall: /usr/local/bin/bloom
	$(RM) /usr/local/bin/bloom

clean:
	$(RM) bloom_debug
	$(RM) bloom_debug.o
	$(RM) bloom_profile
	$(RM) bloom_profile.o
	$(RM) bloom_release
	$(RM) bloom_release.o
	$(RM) gmon.out

.PHONY: all debug profile release install uninstall clean
