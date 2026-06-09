@echo off
title 2D Graphics Editor Workshop
echo Compiling graphics_editor.c...
gcc -Wall -Wextra graphics_editor.c -o graphics_editor.exe -lm

if %ERRORLEVEL% EQU 0 (
    echo Compilation successful! Starting workshop...
    timeout /t 1 >nul
    graphics_editor.exe
) else (
    echo.
    echo [ERROR] Compilation failed! Make sure GCC is installed and in your PATH.
    pause
)
