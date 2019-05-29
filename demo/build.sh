(cd ../src/; ./build.sh)
mkdir -p build
CFLAGS="-Wall -std=c11 -pedantic -lSDL2 -lGL -O3 -g -Werror=implicit-function-declaration"
gcc $CFLAGS demo.c ../src/ui.o -I../src -o build/demo.bin
