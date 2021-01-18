@echo off
if _%VCVARS% == _ ( 
	set VCVARS=1
	call vcvarsall x64
)

SET CFLAGS=/nologo /W3 /D_CRT_SECURE_NO_WARNINGS /I SDL2/include SDL2/lib/x64/SDL2main.lib SDL2/lib/x64/SDL2.lib opengl32.lib shell32.lib ole32.lib
rc /nologo ted.rc
SET SOURCES=main.c text.c ted.res
if _%1 == _ (
	cl %SOURCES% /DDEBUG /DEBUG /Zi %CFLAGS% /Fe:ted
)
if _%1 == _release cl %SOURCES% /O2 %CFLAGS% /Fe:ted
if _%1 == _profile cl %SOURCES% /O2 /DPROFILE %CFLAGS% /Fe:ted
