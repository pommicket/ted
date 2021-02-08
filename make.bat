@echo off
if _%VCVARS% == _ ( 
	set VCVARS=1
	call vcvarsall x64
)
if not exist pcre2-32.lib (
	pushd pcre2-10.36
	cmake -D PCRE2_BUILD_PCRE2_8=OFF -D PCRE2_BUILD_TESTS=OFF -D PCRE2_BUILD_PCRE2_32=ON -D CMAKE_BUILD_TYPE=Release -D CMAKE_GENERATOR_PLATFORM=x64 -D PCRE2_STATIC=ON .
	cmake --build . --config Release
	popd
	copy pcre2-10.36\Release\pcre2-32.lib
)
SET C_FLAGS=/nologo /W4 /MD /wd4200 /wd4204 /wd4221 /wd4706 /wd4214 /D_CRT_SECURE_NO_WARNINGS /I pcre2-10.36 /I SDL2/include SDL2/lib/x64/SDL2main.lib SDL2/lib/x64/SDL2.lib opengl32.lib shell32.lib ole32.lib pcre2-32.lib
rc /nologo ted.rc 
if _%1 == _ (
	cl main.c ted.res /DDEBUG /DEBUG /Zi %C_FLAGS% /Fe:ted
)
if _%1 == _release cl main.c ted.res /O2 %C_FLAGS% /Fe:ted
if _%1 == _release_with_debug_info cl main.c ted.res /DEBUG /Zi /O2 %C_FLAGS% /Fe:ted
if _%1 == _profile cl main.c ted.res /O2 /DPROFILE %C_FLAGS% /Fe:ted
