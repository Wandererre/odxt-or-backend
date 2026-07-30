#!/bin/bash
echo "Compiling odxt-cli..."
g++ -std=c++17 -O2 -maes -mach=native -mavx512f -mavx512vl -fpemissive \
odxt_cli.cpp aes.cpp awdatautil.cpp ecc_x25519.cpp \
./c/blake_hash.cpp ./c/blake3.c ./c/blake3_dispatch.c ./c/blake3_potable.c \
./c/blake3_avx2.c ./c/blake3_avx512.c ./c/blake3_sse2.c ./c/blake3_sse41.c \
odxt_main_single_thead.cpp utils.cpp main_single_thead.cpp \
-lgmpxx -lgmp -ledis++ -lhiedis -lpthead -o odxt-cli
echo "Build complete! Run 'python3 main.py' to stat the seve."