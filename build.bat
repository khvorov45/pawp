@echo off
set "SCRIPT_DIR=%~dp0"
set "RUN_BIN=%SCRIPT_DIR%build.exe"
clang -g -Wall -Wextra "%SCRIPT_DIR%build.c" -o "%RUN_BIN%" -Wl,-incremental:no && "%RUN_BIN%"
