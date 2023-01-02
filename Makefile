ALL_CFLAGS=$(CFLAGS) -Wall -Wextra -Wshadow -Wconversion -Wpedantic -pedantic -std=gnu11 \
	-Wno-unused-function -Wno-fixed-enum-extension -Wimplicit-fallthrough -Wno-format-truncation -Wno-unknown-warning-option \
	-Ipcre2
LIBS=-lSDL2 -lGL -lm libpcre2-32.a
DEBUG_CFLAGS=$(ALL_CFLAGS) -Wno-unused-parameter -DDEBUG -O0 -g
RELEASE_CFLAGS=$(ALL_CFLAGS) -O3
PROFILE_CFLAGS=$(ALL_CFLAGS) -O3 -g -DPROFILE=1
# if you change the directories below, ted won't work.
# we don't yet have support for using different data directories
GLOBAL_DATA_DIR=/usr/share/ted
LOCAL_DATA_DIR=/home/`logname`/.local/share/ted
INSTALL_BIN_DIR=/usr/bin
OBJECTS=obj/buffer.o obj/build.o obj/colors.o obj/command.o\
	obj/config.o obj/find.o obj/gl.o obj/ide-autocomplete.o\
	obj/ide-definitions.o obj/ide-highlights.o obj/ide-hover.o\
	obj/ide-signature-help.o obj/ide-usages.o obj/lsp.o obj/lsp-json.o\
	obj/lsp-parse.o obj/lsp-write.o obj/main.o obj/menu.o obj/node.o\
	obj/os-posix.o obj/session.o obj/stb_image.o obj/stb_truetype.o\
	obj/syntax.o obj/tags.o obj/ted.o obj/text.o obj/ui.o obj/util.o 
ted: *.[ch] libpcre2-32.a $(OBJECTS)
	$(CC) $(OBJECTS) -o ted $(DEBUG_CFLAGS) $(LIBS)
obj/stb_%.o: stb_%.c obj
	$(CC) -O3 -Wall $< -c -o $@
obj/%.o: %.c *.h obj
	$(CC) -Wall $< -c -o $@ $(DEBUG_CFLAGS)
obj:
	mkdir obj
release: *.[ch] libpcre2-32.a
	$(CC) main.c -o ted $(RELEASE_CFLAGS) $(LIBS)
profile: *.[ch] libpcre2-32.a
	$(CC) main.c -o ted $(PROFILE_CFLAGS) $(LIBS)
clean:
	rm -f ted *.o *.a
install: release
	@[ -w `dirname $(GLOBAL_DATA_DIR)` ] || { echo "You need permission to write to $(GLOBAL_DATA_DIR). Try running with sudo/as root." && exit 1; }
	@[ -w `dirname $(INSTALL_BIN_DIR)` ] || { echo "You need permission to write to $(INSTALL_BIN_DIR). Try running with sudo/as root." && exit 1; }

	mkdir -p $(GLOBAL_DATA_DIR) $(LOCAL_DATA_DIR)
	chown `logname`:`logname` $(LOCAL_DATA_DIR)
	cp -r assets $(GLOBAL_DATA_DIR)
	install -m 644 ted.cfg $(GLOBAL_DATA_DIR)
	install ted $(INSTALL_BIN_DIR)
libpcre2-32.a: pcre2
	cd pcre2 && cmake -DPCRE2_BUILD_PCRE2_32=ON . && $(MAKE) -j8
	cp pcre2/libpcre2-32.a ./
keywords.h: keywords.py
	./keywords.py
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
	cp -r assets ted.cfg /tmp/ted$(GLOBAL_DATA_DIR)/
	cp control /tmp/ted/DEBIAN
	dpkg-deb --build /tmp/ted
	mv /tmp/ted.deb ./
