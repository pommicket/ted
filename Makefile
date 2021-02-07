ALL_CFLAGS=$(CFLAGS) -Wall -Wextra -Wshadow -Wconversion -Wpedantic -pedantic -std=gnu11 \
	-Wno-unused-function -Wno-fixed-enum-extension -Wimplicit-fallthrough -Wno-format-truncation -Wno-unknown-warning-option
LIBS=-lSDL2 -lGL -ldl -lm libpcre2-32.a -Ipcre2-10.36/build
DEBUG_CFLAGS=$(ALL_CFLAGS) -DDEBUG -O0 -g
RELEASE_CFLAGS=$(ALL_CFLAGS) -O3
PROFILE_CFLAGS=$(ALL_CFLAGS) -O3 -DPROFILE=1
GLOBAL_DATA_DIR=/usr/share/ted
LOCAL_DATA_DIR=/home/`logname`/.local/share/ted
INSTALL_BIN_DIR=/usr/bin
ted: *.[ch] libpcre2-32.a
	$(CC) main.c -o ted $(DEBUG_CFLAGS) $(LIBS)
release: *.[ch] libpcre2-32.a
	$(CC) main.c -o ted $(RELEASE_CFLAGS) $(LIBS)
profile: *.[ch] libpcre2-32.a
	$(CC) main.c -o ted $(PROFILE_CFLAGS) $(LIBS)
clean:
	rm -f ted *.o
install: release
	@[ -w `dirname $(GLOBAL_DATA_DIR)` ] || { echo "You need permission to write to $(GLOBAL_DATA_DIR). Try running with sudo/as root." && exit 1; }
	@[ -w `dirname $(INSTALL_BIN_DIR)` ] || { echo "You need permission to write to $(INSTALL_BIN_DIR). Try running with sudo/as root." && exit 1; }

	mkdir -p $(GLOBAL_DATA_DIR) $(LOCAL_DATA_DIR)
	chown `logname`:`logname` $(LOCAL_DATA_DIR)
	cp -r assets $(GLOBAL_DATA_DIR)
	install -m 644 ted.cfg $(GLOBAL_DATA_DIR)
	[ ! -e $(LOCAL_DATA_DIR)/ted.cfg ] && install -o `logname` -g `logname` -m 644 ted.cfg $(LOCAL_DATA_DIR) || :
	install ted $(INSTALL_BIN_DIR)
libpcre2-32.a: pcre2-10.36.zip
	rm -rf pcre2-10.36
	unzip $<
	mkdir pcre2-10.36/build
	cd pcre2-10.36/build && cmake -DPCRE2_BUILD_PCRE2_32=ON .. && $(MAKE) -j8
	cp pcre2-10.36/build/$@ ./

