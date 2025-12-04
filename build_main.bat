@echo off
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"

if exist "icon.ico" (
    rc /D__RESOURCE_ICON_ICO__ resources.rc
    cl /EHsc /DUNICODE /D_UNICODE /D__RESOURCE_ICON_ICO__ main.cpp /I. /link resources.res gdiplus.lib user32.lib shell32.lib comctl32.lib gdi32.lib advapi32.lib /out:TransparentWindowApp.exe
    if exist "resources.res" del resources.res
) else (
    cl /EHsc /DUNICODE /D_UNICODE main.cpp /I. /link gdiplus.lib user32.lib shell32.lib comctl32.lib gdi32.lib advapi32.lib /out:TransparentWindowApp.exe
)