ALL_CFLAGS=$(CFLAGS) -Wall -Wextra -Wshadow -Wconversion -Wpedantic -pedantic -std=gnu11 \
	-Wno-unused-function -Wno-fixed-enum-extension -Wimplicit-fallthrough
LIBS=-lSDL2 -lGL -ldl -lm
DEBUG_CFLAGS=$(ALL_CFLAGS) -DDEBUG -O0 -g
ted: *.[ch] text.o
	$(CC) main.c text.o -o $@ $(DEBUG_CFLAGS) $(LIBS)
%.o: %.c
	$(CC) $< -c -o $@ $(DEBUG_CFLAGS)
