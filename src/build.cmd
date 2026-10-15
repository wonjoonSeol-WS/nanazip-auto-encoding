@echo off
rem Builds NanaZipAutoCP.dll (next to the installer) and the test programs in tests\.
rem Run from an "x64 Native Tools Command Prompt for VS" with the Windows SDK installed.
setlocal
cd /d "%~dp0"
rem 64-bit Explorer cannot load a 32-bit DLL, so the menus would silently never appear
if /i not "%VSCMD_ARG_TGT_ARCH%"=="x64" (
    echo Run this from an x64 Native Tools Command Prompt.
    exit /b 1
)
set LIBS=shell32.lib shlwapi.lib advapi32.lib ole32.lib user32.lib gdi32.lib uuid.lib
set FLAGS=/nologo /O2 /W4 /WX /EHsc /MT /std:c++17 /permissive- /guard:cf /DUNICODE /D_UNICODE
cl %FLAGS% /LD NanaZipAutoCP.cpp /Fe:..\NanaZipAutoCP.dll /link /DEF:NanaZipAutoCP.def /GUARD:CF %LIBS% || exit /b 1
for %%t in (test_drag test_shellmenu test_assoc) do (
    cl %FLAGS% ..\tests\%%t.cpp /Fe:..\tests\%%t.exe /link %LIBS% || exit /b 1
)
