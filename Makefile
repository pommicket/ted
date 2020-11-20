ALL_CFLAGS=$(CFLAGS) -Wall -Wextra -Wshadow -Wconversion -Wpedantic -pedantic -std=gnu11 \
	-Wno-unused-function -Wno-fixed-enum-extension -Wimplicit-fallthrough
LIBS=-lSDL2 -lGL -ldl
DEBUG_CFLAGS=$(ALL_CFLAGS) $(LIBS) -DDEBUG -O0 -g
ted: *.[ch]
	$(CC) main.c -o $@ $(DEBUG_CFLAGS)
