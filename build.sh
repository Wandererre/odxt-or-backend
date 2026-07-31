#!/bin/bash
echo "Compiling odxt-cli..."
g++ -std=c++17 -I/usr/local/include -O2 -maes -mavx2 -msse4.1 -DBLAKE3_NO_AVX512 -fpermissive \
odxt_cli.cpp aes.cpp rawdatautil.cpp ecc_x25519.cpp \
./c/blake_hash.cpp ./c/blake3.c ./c/blake3_dispatch.c ./c/blake3_portable.c \
./c/blake3_avx2.c ./c/blake3_sse2.c ./c/blake3_sse41.c \
odxt_main_single_thread.cpp utils.cpp main_single_thread.cpp \
-lgmpxx -lgmp -lredis++ -lhiredis -lhiredis_ssl -lssl -lcrypto -lpthread -o odxt-cli
echo "Build complete! Run 'python3 main.py' to start the server."