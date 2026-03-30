@echo off

echo Compilation started at %date% %time%
echo,

set SDL_LIB=..\chronicles\external\SDL3-3.4.2\lib\x64\SDL3.lib
set SDLIMAGE_LIB=..\chronicles\external\SDL3_image-3.4.0\lib\x64\SDL3_image.lib
set SDL_Include=/I"..\chronicles\external\SDL3-3.4.2\include"
set SDLIMAGE_Include=/I"..\chronicles\external\SDL3_image-3.4.0\include"

set CommonCompilerFlags=-MT -nologo -fp:fast -Gm- -GR- -EHa- -Od -Oi -WX -W4 -wd4201 -wd4100 -wd4189 -wd4244 -wd4996 -wd4456 -FC -Z7 %SDL_Include% %SDLIMAGE_Include%

set CommonLinkerFlags=-incremental:no -opt:ref /DEBUG /PDB:main.pdb %SDL_LIB% %SDLIMAGE_LIB% user32.lib gdi32.lib winmm.lib shell32.lib kernel32.lib msvcrt.lib /NODEFAULTLIB:LIBCMT /NODEFAULTLIB:MSVCRTD /SUBSYSTEM:WINDOWS

REM echo Updating etags
REM echo,
REM etags *.cpp *.h raylib\*.c

IF NOT EXIST .\..\build mkdir ..\..\build
pushd ..\..\build
copy /Y ..\chronicles\external\SDL3-3.4.2\lib\x64\SDL3.dll . > NUL
copy /Y ..\chronicles\external\SDL3_image-3.4.0\lib\x64\SDL3_image.dll . > NUL

REM delete pdb because debugger maintains a lock on pdb so pdb cannot be overwritten
del *.pdb > NUL 2> NUL

cl %CommonCompilerFlags% ..\chronicles\code\main.cpp /link %CommonLinkerFlags%
REM/Fe:win32_arwin.exe

popd

echo.
if %errorlevel% equ 0 (
                       echo Compilation finished at %date% %time%
                       ) else (
                               echo Compilation failed with errors at %date% %time%
                               )
