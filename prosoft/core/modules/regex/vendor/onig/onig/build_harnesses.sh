#!/bin/bash

make clean
autoreconf -vfi

# build the library with ASAN
#NO_LINK="-fsanitize=fuzzer-no-link"
NO_LINK=""
./configure CC=clang LD=clang CFLAGS="-g -fsanitize=address -fno-omit-frame-pointer $NO_LINK" LDFLAGS="-g -fsanitize=address -fno-omit-frame-pointer $NO_LINK"
make -j4

OUT=`pwd`/fuzzers
mkdir -p $OUT
LIBFUZZER_FLAGS="-fsanitize=fuzzer,address -fno-omit-frame-pointer"
#LIBS="src/.libs/libonig.a"
LIBS="src/.libs/libonig.a /usr/local/lib/libLLVMFuzzerMain.a"

CFLAGS="-Isrc -g $LIBFUZZER_FLAGS"

# Libfuzzer builds
clang++ contributed/libfuzzer-onig.cpp $LIBS $CFLAGS -o $OUT/libfuzzer-onig
clang harnesses/syntax-harness.c $LIBS $CFLAGS -o $OUT/syntax-libfuzzer
clang harnesses/encode-harness.c $LIBS $CFLAGS -o $OUT/encode-libfuzzer
clang harnesses/deluxe-encode-harness.c $LIBS $CFLAGS -o $OUT/deluxe-encode-libfuzzer

clang -DUTF16_BE harnesses/encode-harness.c $LIBS $CFLAGS -o $OUT/utf16-be-libfuzzer
clang -DUTF16_LE harnesses/encode-harness.c $LIBS $CFLAGS -o $OUT/utf16-le-libfuzzer
clang -DWITH_READ_MAIN harnesses/encode-harness.c src/.libs/libonig.a $CFLAGS -o $OUT/main-encode
clang -DWITH_READ_MAIN -DUTF16_LE harnesses/encode-harness.c src/.libs/libonig.a $CFLAGS -o $OUT/main-utf16-le
clang -DWITH_READ_MAIN -DUTF16_BE harnesses/encode-harness.c src/.libs/libonig.a $CFLAGS -o $OUT/main-utf16-be
clang -DWITH_READ_MAIN harnesses/deluxe-encode-harness.c $LIBS $CFLAGS -o $OUT/main-deluxe-encode
