#include <iostream>

using namespace std;

#include "blake3.h"
#include <stdlib.h>
#include <cstring>

int Blake3(blake3_hasher *hasher, uint8_t *digest,uint8_t *message)
{
    blake3_hasher_init(hasher);
    blake3_hasher_update(hasher, message, 16);
    blake3_hasher_finalize(hasher, digest, BLAKE3_OUT_LEN);
    return 0;
}

int Blake3_K(blake3_hasher *hasher, uint8_t *digest,uint8_t *message)
{
    blake3_hasher_init(hasher);
    blake3_hasher_update(hasher, message, 40);
    blake3_hasher_finalize(hasher, digest, BLAKE3_OUT_LEN);
    return 0;
}

// int main() {
//     // Initialize the hasher.
//     blake3_hasher hasher;
//     blake3_hasher_init(&hasher);

//     auto start_time = chrono::high_resolution_clock::now();

//     // Read input bytes from stdin.
//     unsigned char buf[16] = {0x01, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};
//     blake3_hasher_update(&hasher, buf, 16);

//     // Finalize the hash. BLAKE3_OUT_LEN is the default output length, 32 bytes.
//     uint8_t output[64];
//     blake3_hasher_finalize(&hasher, output, BLAKE3_OUT_LEN);

//     auto stop_time = chrono::high_resolution_clock::now();
//     auto time_elapsed = chrono::duration_cast<chrono::microseconds>(stop_time - start_time).count();
//     cout << dec << time_elapsed << endl;

//     // Print the hash as hexadecimal.
//     for (size_t i = 0; i < BLAKE3_OUT_LEN; i++) {
//         printf("%02x", output[i]);
//     }
//     printf("\n");
//     return 0;
// }
