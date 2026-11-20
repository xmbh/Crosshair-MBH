@echo off
setlocal
if exist "C:\msys64\ucrt64\bin\gcc.exe" set "PATH=C:\msys64\ucrt64\bin;%PATH%"
where gcc.exe >nul 2>&1 || goto :missingTools
where windres.exe >nul 2>&1 || goto :missingTools
pushd "%~dp0src" || exit /b 1
if not exist "..\build\" mkdir "..\build"
if errorlevel 1 goto :failed
windres.exe crosshair.rc -O coff -o "..\build\crosshair.res"
if errorlevel 1 goto :failed
gcc.exe -std=c11 -Os -Wall -Wextra -municode -mwindows -finput-charset=UTF-8 ^
    -ffunction-sections -fdata-sections -Wl,--gc-sections,--no-insert-timestamp -s ^
    Crosshair.c "..\build\crosshair.res" -o "..\build\MBH-Crosshair.exe" ^
    -lgdi32 -luser32 -lshell32 -lcomctl32
if errorlevel 1 goto :failed
popd
echo Creato build\MBH-Crosshair.exe
exit /b 0

:missingTools
echo Servono GCC e windres di MSYS2 UCRT64. Vedi README.md. 1>&2
exit /b 1

:failed
popd
exit /b 1
