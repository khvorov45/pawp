SCRIPT_DIR=$(dirname "$0")
RUN_BIN=$SCRIPT_DIR/hm2.exe
clang -g -Wall -Wextra $SCRIPT_DIR/hm2.c -o $RUN_BIN -lpthread -lm && $RUN_BIN
