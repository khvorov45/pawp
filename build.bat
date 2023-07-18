@echo off
clang -g -Wall -Wextra hm2.c -Wno-unused-function -o hm2.exe -Wl,-incremental:no && hm2.exe
echo ====
clang -g -Wall -Wextra hm2.c -DPAWP_PROFILE -o hm2.exe -Wl,-incremental:no && hm2.exe
