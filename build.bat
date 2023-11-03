@echo off
@REM clang -g -Wall -Wextra hm2.c -Wno-unused-function -fdiagnostics-absolute-paths -o hm2.exe -Wl,-incremental:no && hm2.exe
@REM echo ====
@REM clang -g -Wall -Wextra hm2.c -fdiagnostics-absolute-paths -DPAWP_PROFILE -o hm2.exe -Wl,-incremental:no && hm2.exe

clang -DPAWP_PROFILE -g -O1 -Wall -Wextra pawp.c -o pawp.exe -Wl,-incremental:no && pawp.exe
