cmake_minimum_required(VERSION 3.0)
project(ted)
set(SOURCES buffer.c build.c colors.c command.c config.c find.c gl.c ide-autocomplete.c
	ide-definitions.c ide-highlights.c ide-hover.c ide-signature-help.c ide-usages.c
	lsp.c lsp-json.c lsp-parse.c lsp-write.c main.c  menu.c node.c os.c session.c
	stb_image.c stb_truetype.c syntax.c tags.c ted.c text.c ui.c util.c)
add_executable(ted ${SOURCES})
target_compile_definitions(ted PUBLIC DEBUG=1)
target_link_libraries(ted m SDL2)
target_link_libraries(ted ${CMAKE_SOURCE_DIR}/libpcre2-32.a)
