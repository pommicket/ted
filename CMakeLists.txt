cmake_minimum_required(VERSION 3.1)
project(ted)
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
	set(SOURCES buffer.c build.c colors.c command.c config.c find.c gl.c ide-autocomplete.c
		ide-definitions.c ide-highlights.c ide-hover.c ide-signature-help.c ide-usages.c
		lsp.c lsp-json.c lsp-parse.c lsp-write.c main.c  menu.c node.c os.c session.c
		stb_image.c stb_truetype.c syntax.c tags.c ted.c text.c ui.c util.c)
else()
	set(SOURCES main.c)
endif()

if (MSVC)
	add_executable(ted WIN32 ${SOURCES})
else()
	add_executable(ted ${SOURCES})
endif()


if(CMAKE_BUILD_TYPE STREQUAL "Debug")
	target_compile_definitions(ted PUBLIC DEBUG=1)
endif()
if(MSVC)
	target_sources(ted PRIVATE ted.rc)
	target_include_directories(ted PUBLIC ${CMAKE_SOURCE_DIR}/SDL2/include ${CMAKE_SOURCE_DIR}/pcre2)
	set(CMAKE_C_FLAGS "/MD /W4 /wd4200 /wd4204 /wd4221 /wd4706 /wd4214 /D_CRT_SECURE_NO_WARNINGS")
	set(CMAKE_C_FLAGS_DEBUG "/WX /Zi /Ob0 /Od /RTC1")
	set(SDL2_LIB_DIR ${CMAKE_SOURCE_DIR}/SDL2/lib/x64)
	target_link_libraries(ted ${SDL2_LIB_DIR}/SDL2.lib)
	target_link_libraries(ted ${CMAKE_SOURCE_DIR}/pcre2-32-static.lib)
else()
	set(CMAKE_C_FLAGS "-Wall -Wextra -Wshadow -Wconversion -Wpedantic -pedantic -std=gnu11 -Wno-unused-function -Wno-fixed-enum-extension -Wimplicit-fallthrough -Wno-format-truncation -Wno-unknown-warning-option")
	target_link_libraries(ted m SDL2)
	target_link_libraries(ted ${CMAKE_SOURCE_DIR}/libpcre2-32.a)
endif()
