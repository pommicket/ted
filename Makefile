# where to put ted's files
# INSTALL_DIR = prefix for GLOBAL_DATA_DIR, BIN_DIR, DOC_DIR, if they're not provided
# GLOBAL_DATA_DIR = files shared between all users (e.g. default ted.cfg)
# LOCAL_DATA_DIR = user-specific files
# either one can start with ~ for home directory.
# these are currently ignored for debug builds.
INSTALL_DIR?=/usr
DOC_DIR?=$(INSTALL_DIR)/share/doc/ted
GLOBAL_DATA_DIR?=$(INSTALL_DIR)/share/ted
LOCAL_DATA_DIR?=~/.local/share/ted
BIN_DIR?=$(INSTALL_DIR)/bin
DEBTMP=deb-tmp

ALL_CFLAGS=$(CFLAGS) -Wall -Wextra -Wshadow -Wconversion -Wpedantic -pedantic -std=gnu11 \
	-Wno-unused-function -Wno-fixed-enum-extension -Wimplicit-fallthrough -Wno-format-truncation -Wno-unknown-warning-option \
	-Ipcre2 -DTED_GLOBAL_DATA_DIR='"$(GLOBAL_DATA_DIR)"' -DTED_LOCAL_DATA_DIR='"$(LOCAL_DATA_DIR)"' \
	-fno-omit-frame-pointer
LIBS_NON_SDL=-lm libpcre2-32.a libpcre2-8.a
LIBS=$(LIBS_NON_SDL) -lSDL3
SDL_DIR=SDL3
STATIC_SDL=$(SDL_DIR)/build/libSDL3.a
FETCH_SDL=$(SDL_DIR)/fetched.o
RELEASE_CFLAGS=-O3 $(ALL_CFLAGS)
PROFILE_CFLAGS=-O3 -g -DPROFILE=1 $(ALL_CFLAGS)
PCRELIB=libpcre2-8.a

debug-build: ted.debug compile_commands.json
ted.debug: debug/ted
	@# note: needed so cp doesn't fail if `ted` is busy
	rm -f ted.debug
	cp debug/ted ./ted.debug
compile_commands.json: debug/ted
	rm -f compile_commands.json
	cp debug/compile_commands.json .
debug/ted: *.[ch] $(PCRELIB) CMakeLists.txt
	mkdir -p debug
	cd debug && cmake -DCMAKE_EXPORT_COMPILE_COMMANDS=1 -DCMAKE_BUILD_TYPE=Debug -GNinja ..
	ninja -C debug
ted.release: *.[ch] $(PCRELIB)
	$(CC) main.c -o ted.release $(RELEASE_CFLAGS) $(LIBS)
ted.release_debug: *.[ch] $(PCRELIB)
	$(CC) main.c -g -o ted.release_debug $(RELEASE_CFLAGS) $(LIBS)
ted-static-sdl.release: *.[ch] $(STATIC_SDL)
	$(CC) main.c -g -o ted-static-sdl.release $(RELEASE_CFLAGS) $(LIBS_NON_SDL) $(STATIC_SDL)
$(STATIC_SDL): $(FETCH_SDL)
	@mkdir -p $(SDL_DIR)/build
	cd $(SDL_DIR)/build && cmake -DCMAKE_BUILD_TYPE=Release -DSDL_TESTS=0 -DSDL_STATIC=1 -S ..
	$(MAKE) -C $(SDL_DIR)/build
$(FETCH_SDL):
	[ -e $(SDL_DIR) ] && echo 'Directory $(SDL_DIR) already exists - try deleting it and rebuilding.' && exit 1 || :
	git clone --recursive 'https://github.com/libsdl-org/SDL' $(SDL_DIR)
	touch $(FETCH_SDL)
ted.profile: *.[ch] $(PCRELIB)
	$(CC) main.c -o ted.profile $(PROFILE_CFLAGS) $(LIBS)
clean:
	rm -rf debug release *.o *.a
install: install-ted
install-%: %.release
	@[ -w `dirname $(GLOBAL_DATA_DIR)` ] || { echo "You need permission to write to $(GLOBAL_DATA_DIR). Try running with sudo/as root." && exit 1; }
	@[ -w `dirname $(BIN_DIR)` ] || { echo "You need permission to write to $(BIN_DIR). Try running with sudo/as root." && exit 1; }

	mkdir -p $(GLOBAL_DATA_DIR)
	cp -r assets $(GLOBAL_DATA_DIR)
	cp -r themes $(GLOBAL_DATA_DIR)
	install -m 644 ted.cfg $(GLOBAL_DATA_DIR)
	install $< $(BIN_DIR)/ted
$(PCRELIB):
	@if [ '!' -f pcre2/build/Makefile ]; then \
		rm -rf pcre2/build; \
		mkdir pcre2/build && cd pcre2/build && pwd && \
			cmake -DPCRE2_BUILD_TESTS=OFF -DCMAKE_BUILD_TYPE=Release -DPCRE2_BUILD_PCRE2_32=ON ..;\
	fi
	$(MAKE) -C pcre2/build
	cp pcre2/build/libpcre2-32.a pcre2/build/libpcre2-8.a .
keywords.h: keywords.py
	python3 keywords.py
%.deb: %.release control.sh makedeb.sh
	BIN_DIR='$(BIN_DIR)' \
		LOCAL_DATA_DIR='$(LOCAL_DATA_DIR)' \
		GLOBAL_DATA_DIR='$(GLOBAL_DATA_DIR)' \
		DOC_DIR='$(DOC_DIR)' \
		./makedeb.sh `basename $@ .deb`
ted-versioned.deb: ted.deb
	F=ted_`./version.sh`-1_amd64.deb; \
		echo "Outputting to $$F" && \
		cp ted.deb "$$F"
ted-static-sdl-versioned.deb: ted-static-sdl.deb
	F=ted-static-sdl_`./version.sh`-1_amd64.deb; \
		echo "Outputting to $$F" && \
		cp ted-static-sdl.deb "$$F"

publish: ted-versioned.deb ted-static-sdl-versioned.deb
	./publish.py
