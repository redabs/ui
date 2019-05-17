(cd ../src/; ./build.sh)
CFLAGS="-Wall -std=c11 -pedantic -lSDL2 -lGL -O3 -g -Werror=implicit-function-declaration"
clang $CFLAGS demo.c ../src/ui.o -I../src -o build/demo.bin
