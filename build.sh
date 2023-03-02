SCRIPT_DIR=$(dirname "$0")
RUN_BIN=$SCRIPT_DIR/hm.exe
clang -g -Wall -Wextra $SCRIPT_DIR/hm.c -o $RUN_BIN -lpthread && $RUN_BIN
