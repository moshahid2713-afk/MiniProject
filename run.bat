@echo off
title 2D Graphics Editor Workshop
setlocal

set ROOT=%~dp0
set PDC=%ROOT%deps\PDCurses
set INC=-I"%PDC%" -DHAVE_NO_INFOEX

if not exist "%PDC%\curses.h" (
    echo [ERROR] PDCurses not found.
    echo Run: git clone https://github.com/wmcbrine/PDCurses.git deps\PDCurses
    pause
    exit /b 1
)

echo Cleaning any existing graphics_editor process...
taskkill /F /IM graphics_editor.exe >nul 2>&1

echo Compiling graphics_editor with PDCurses...
gcc -Wall -Wextra %INC% graphics_editor.c ^
  "%PDC%\pdcurses\addch.c" "%PDC%\pdcurses\addchstr.c" "%PDC%\pdcurses\addstr.c" ^
  "%PDC%\pdcurses\attr.c" "%PDC%\pdcurses\beep.c" "%PDC%\pdcurses\bkgd.c" ^
  "%PDC%\pdcurses\border.c" "%PDC%\pdcurses\clear.c" "%PDC%\pdcurses\color.c" ^
  "%PDC%\pdcurses\delch.c" "%PDC%\pdcurses\deleteln.c" "%PDC%\pdcurses\getch.c" ^
  "%PDC%\pdcurses\getstr.c" "%PDC%\pdcurses\getyx.c" "%PDC%\pdcurses\inch.c" ^
  "%PDC%\pdcurses\inchstr.c" "%PDC%\pdcurses\initscr.c" "%PDC%\pdcurses\inopts.c" ^
  "%PDC%\pdcurses\insch.c" "%PDC%\pdcurses\insstr.c" "%PDC%\pdcurses\instr.c" ^
  "%PDC%\pdcurses\kernel.c" "%PDC%\pdcurses\keyname.c" "%PDC%\pdcurses\mouse.c" ^
  "%PDC%\pdcurses\move.c" "%PDC%\pdcurses\outopts.c" "%PDC%\pdcurses\overlay.c" ^
  "%PDC%\pdcurses\pad.c" "%PDC%\pdcurses\panel.c" "%PDC%\pdcurses\printw.c" ^
  "%PDC%\pdcurses\refresh.c" "%PDC%\pdcurses\scanw.c" "%PDC%\pdcurses\scr_dump.c" ^
  "%PDC%\pdcurses\scroll.c" "%PDC%\pdcurses\slk.c" "%PDC%\pdcurses\termattr.c" ^
  "%PDC%\pdcurses\touch.c" "%PDC%\pdcurses\util.c" "%PDC%\pdcurses\window.c" ^
  "%PDC%\pdcurses\debug.c" ^
  "%PDC%\wincon\pdcclip.c" "%PDC%\wincon\pdcdisp.c" "%PDC%\wincon\pdcgetsc.c" ^
  "%PDC%\wincon\pdckbd.c" "%PDC%\wincon\pdcscrn.c" "%PDC%\wincon\pdcsetsc.c" ^
  "%PDC%\wincon\pdcutil.c" ^
  -o graphics_editor.exe -lm

if %ERRORLEVEL% EQU 0 (
    echo Compilation successful! Starting workshop...
    timeout /t 1 >nul
    graphics_editor.exe
    exit /b 0
)

echo Trying system ncurses library...
gcc -Wall -Wextra graphics_editor.c -o graphics_editor.exe -lncurses -lm
if %ERRORLEVEL% EQU 0 (
    echo Compilation successful! Starting workshop...
    timeout /t 1 >nul
    graphics_editor.exe
    exit /b 0
)

echo.
echo [ERROR] Compilation failed! Make sure GCC is installed and in PATH.
pause
exit /b 1
