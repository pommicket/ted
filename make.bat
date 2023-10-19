@echo off
if _%VCVARS% == _ ( 
	set VCVARS=1
	call vcvarsall x64
)
if not exist pcre2-8-static.lib (
	pushd pcre2
	cmake -D PCRE2_BUILD_PCRE2_8=ON -D PCRE2_BUILD_TESTS=OFF -D PCRE2_BUILD_PCRE2_32=ON -D CMAKE_BUILD_TYPE=Release -D CMAKE_GENERATOR_PLATFORM=x64 -D PCRE2_STATIC=ON .
	cmake --build . --config Release
	popd
	copy /y pcre2\Release\pcre2-32-static.lib
	copy /y pcre2\Release\pcre2-8-static.lib
)
SET C_FLAGS=/nologo /W4 /MD /wd4200 /wd4204 /wd4221 /wd4706 /wd4214 /D_CRT_SECURE_NO_WARNINGS /I SDL2/include /I pcre2 User32.lib SDL2/lib/x64/SDL2main.lib SDL2/lib/x64/SDL2.lib pcre2-8-static.lib pcre2-32-static.lib
rc /nologo ted.rc 
if _%1 == _ (
	if not exist debug mkdir debug
	pushd debug
	cmake -DCMAKE_BUILD_TYPE=Debug -GNinja ..
	ninja
	ninja -t compdb C_COMPILER__ted_Debug > ..\compile_commands.json
	copy /y ted.exe ..
	popd
)
if _%1 == _release (
	cl main.c ted.res /O2 /wd4702 %C_FLAGS% /Fe:ted
	devenv windows_installer\ted\ted.vdproj /nologo /build "Release|Default"
	copy /y windows_installer\ted\Release\ted.msi
)
if _%1 == _release_with_debug_info cl main.c ted.res /DEBUG /Zi /O2 /wd4702 %C_FLAGS% /Fe:ted
if _%1 == _profile cl main.c ted.res /O2 /wd4702 /DPROFILE %C_FLAGS% /Fe:ted
