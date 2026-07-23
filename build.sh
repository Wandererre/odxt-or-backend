#!/bin/bash
echo "Compiling odxt-cli..."
g++ -std=c++17 -O2 -maes -march=native -mavx512f -mavx512vl -fpermissive \
odxt_cli.cpp aes.cpp rawdatautil.cpp ecc_x25519.cpp \
./c/blake_hash.cpp ./c/blake3.c ./c/blake3_dispatch.c ./c/blake3_portable.c \
./c/blake3_avx2.c ./c/blake3_avx512.c ./c/blake3_sse2.c ./c/blake3_sse41.c \
odxt_main_single_thread.cpp utils.cpp main_single_thread.cpp \
-lgmpxx -lgmp -lredis++ -lhiredis -lpthread -o odxt-cli
echo "Build complete! Run 'python3 main.py' to start the server."