ALL_CFLAGS=$(CFLAGS) -Wall -Wextra -Wshadow -Wconversion -Wpedantic -pedantic -std=gnu11 \
	-Wno-unused-function -Wno-fixed-enum-extension -Wimplicit-fallthrough -Wno-format-truncation -Wno-unknown-warning-option \
	-Ipcre2
LIBS=-lSDL2 -lGL -lm libpcre2-32.a libpcre2-8.a
RELEASE_CFLAGS=$(ALL_CFLAGS) -O3
PROFILE_CFLAGS=$(ALL_CFLAGS) -O3 -g -DPROFILE=1
# if you change the directories below, ted won't work.
# we don't yet have support for using different data directories
GLOBAL_DATA_DIR=/usr/share/ted
LOCAL_DATA_DIR=/home/`logname`/.local/share/ted
INSTALL_BIN_DIR=/usr/bin
debug-build: ted compile_commands.json
ted: debug/ted
	@# note: needed so cp doesn't fail if `ted` is busy
	rm -f ted
	cp debug/ted .
compile_commands.json: debug/ted
	rm -f compile_commands.json
	cp debug/compile_commands.json .
debug/ted: *.[ch] pcre-lib CMakeLists.txt
	mkdir -p debug
	cd debug && cmake -DCMAKE_EXPORT_COMPILE_COMMANDS=1 -DCMAKE_BUILD_TYPE=Debug -GNinja ..
	ninja -C debug
release: *.[ch] pcre-lib
	$(CC) main.c -o ted $(RELEASE_CFLAGS) $(LIBS)
release_debug: *.[ch] pcre-lib
	$(CC) main.c -g -o ted $(RELEASE_CFLAGS) $(LIBS)
profile: *.[ch] pcre-lib
	$(CC) main.c -o ted $(PROFILE_CFLAGS) $(LIBS)
clean:
	rm -f ted *.o *.a
install: release
	@[ -w `dirname $(GLOBAL_DATA_DIR)` ] || { echo "You need permission to write to $(GLOBAL_DATA_DIR). Try running with sudo/as root." && exit 1; }
	@[ -w `dirname $(INSTALL_BIN_DIR)` ] || { echo "You need permission to write to $(INSTALL_BIN_DIR). Try running with sudo/as root." && exit 1; }

	mkdir -p $(GLOBAL_DATA_DIR) $(LOCAL_DATA_DIR)
	chown `logname`:`logname` $(LOCAL_DATA_DIR)
	cp -r assets $(GLOBAL_DATA_DIR)
	cp -r themes $(GLOBAL_DATA_DIR)
	install -m 644 ted.cfg $(GLOBAL_DATA_DIR)
	install ted $(INSTALL_BIN_DIR)
pcre-lib:
	@if [ '(' '!' -f libpcre2-32.a ')' -o '(' '!' -f libpcre2-8.a ')' ]; then \
		cd pcre2 && cmake -DPCRE2_BUILD_PCRE2_32=ON . && $(MAKE) -j8 && \
		cp libpcre2-32.a libpcre2-8.a ../ ; \
	fi
keywords.h: keywords.py
	python3 keywords.py
ted.deb: release
	rm -rf /tmp/ted
	mkdir -p /tmp/ted/DEBIAN
	mkdir -p /tmp/ted$(INSTALL_BIN_DIR)
	mkdir -p /tmp/ted$(GLOBAL_DATA_DIR)
	mkdir -p /tmp/ted/usr/share/icons/hicolor/48x48/apps/
	convert assets/icon.bmp -resize 48x48 /tmp/ted/usr/share/icons/hicolor/48x48/apps/ted.png
	mkdir -p /tmp/ted/usr/share/applications
	cp ted.desktop /tmp/ted/usr/share/applications
	cp ted /tmp/ted$(INSTALL_BIN_DIR)/
	cp -r assets themes ted.cfg /tmp/ted$(GLOBAL_DATA_DIR)/
	cp control /tmp/ted/DEBIAN
	dpkg-deb --build /tmp/ted
	mv /tmp/ted.deb ./
