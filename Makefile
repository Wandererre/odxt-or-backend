CC=g++
CFLAGS=-I.
CONFIG=-std=c++17 -O3 -msse2 -msse -msse4 -mssse3 -march=native -maes -lpthread -lgmpxx -lgmp -lhiredis -lredis++ -pthread -Wl,-rpath,/usr/local/lib,./c/libblake3.so
REDIS_DB=7

all: aes.cpp rawdatautil.cpp ecc_x25519.cpp bloom_filter.cpp ./c/blake_hash.cpp mainwindow.cpp main.cpp
	$(CC) -o sse_out aes.cpp rawdatautil.cpp ecc_x25519.cpp bloom_filter.cpp ./c/blake_hash.cpp mainwindow.cpp main.cpp $(CONFIG)

single_thread: aes.cpp rawdatautil.cpp ecc_x25519.cpp ./c/blake_hash.cpp odxt_main_single_thread.cpp main_single_thread.cpp utils.cpp
	$(CC) -o sse_sing_thread_out aes.cpp rawdatautil.cpp ecc_x25519.cpp ./c/blake_hash.cpp odxt_main_single_thread.cpp main_single_thread.cpp utils.cpp $(CONFIG)

clean:
	rm -rf *.o *.gch sse_out sse_sing_thread_out

clean_all:
	@make clean
	rm -rf update_count.csv
	redis-cli -n ${REDIS_DB} flushdb