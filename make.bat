@echo off
if _%VCVARS% == _ ( 
	set VCVARS=1
	call vcvarsall x64
)

SET CFLAGS=/nologo /W4 /wd4200 /wd4204 /wd4221 /wd4706 /D_CRT_SECURE_NO_WARNINGS /I SDL2/include SDL2/lib/x64/SDL2main.lib SDL2/lib/x64/SDL2.lib opengl32.lib shell32.lib ole32.lib
rc /nologo ted.rc
if _%1 == _ (
	cl main.c text.c ted.res /DDEBUG /DEBUG /Zi %CFLAGS% /Fe:ted
)
if _%1 == _release cl main.c ted.res /O2 %CFLAGS% /Fe:ted
if _%1 == _profile cl main.c ted.res /O2 /DPROFILE %CFLAGS% /Fe:ted
