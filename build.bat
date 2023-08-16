@echo off
clang -g -Wall -Wextra hm2.c -Wno-unused-function -fdiagnostics-absolute-paths -o hm2.exe -Wl,-incremental:no && hm2.exe
echo ====
clang -g -Wall -Wextra hm2.c -fdiagnostics-absolute-paths -DPAWP_PROFILE -o hm2.exe -Wl,-incremental:no && hm2.exe
