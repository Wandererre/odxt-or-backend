#ifndef ODXTMAIN_H
#define ODXTMAIN_H

#include <cstdio>
#include <unistd.h>
#include <cstdlib>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <iomanip>
#include <bits/stdc++.h>
#include <thread>
#include <algorithm>
#include <functional>
#include <mutex>
#include <condition_variable>
#include <future>
#include <atomic>

#include <random>
#include <cassert>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <chrono>
#include <map>
#include <unordered_set>

#include <sched.h>
#include <pthread.h>

#include <gmpxx.h>

#include "size_parameters.h"
#include "aes.h"
#include "rawdatautil.h"
#include "ecc_x25519.h"
#include "c/blake3.h" 
#include "c/blake_hash.h"

#include <sw/redis++/redis++.h>
#include <sw/redis++/connection.h>
#include <sw/redis++/redis_cluster.h>

using namespace std;
using namespace sw::redis;

extern string widxdb_file;


extern mpz_class Prime;
extern mpz_class InvExp;

extern unsigned char ecc_basep[32];

extern unsigned char KS[16];
extern unsigned char KX[16];
extern unsigned char KY[16];
extern unsigned char KZ[16];
extern unsigned char KT[16];

//----------------------------------------
extern std::map<std::string,unsigned int> update_count;
//----------------------------------------

int Redis_Init();
int Sys_Init();
int Sys_Clear();

/**
 * @brief ODXT Setup function
 *
 * @details
 * Executes ODXT Setup function
 * 
 * @param none Take no parameter as input
 */
int ODXT_Setup();

/**
 * @brief ODXT Update function
 *
 * @details
 * Executes ODXT Update function
 * 
 * @param keyword Keyword to update
 * @param id Document identifier to update
 * @param op Update operation to perform
 */
int ODXT_Update(std::string keyword, std::string kw, std::string id, int cnt, unsigned char op);

/**
 * @brief ODXT Search function
 *
 * @details
 * Executes ODXT Search function
 * 
 * @param query Vector or conjunctive query keywords to search
 */
int ODXT_Search(std::unordered_set<std::string> *IdList, std::vector<std::string> query, int update_cnt);

int SHA3_HASH(blake3_hasher *hasher,unsigned char *msg, unsigned char *digest);
int SHA3_HASH_K(blake3_hasher *hasher,unsigned char *msg, unsigned char *digest);
int ECC_FPINV(unsigned char *fp_x, unsigned char *fp_invx);
int ECC_MUL(unsigned char *in_A,unsigned char *in_B,unsigned char *prod);

std::string HexToStr(unsigned char *hexarr,int len=16);
std::string NumToHexStr(int num);
int NumStrToHex(unsigned char *hexarr,string numin);

/**
 * @brief Utility function to convert from string to hex array
 *
 * @details
 * Converts a standard c++ string object to unsigned char array
 * Not ASCII value conversion! rather, actual string representation is coverted to bytes
 * 
 * @param hexarr output unsigend char array (or bytes)
 * @param str_in input std string
 * @param n_byte number of bytes to consider in the final hex array. Remember no length checking is done! 
 */
int StrToHex(unsigned char *hexarr, std::string str_in, unsigned int n_bytes);
int StrToHexBVec(unsigned char *hexarr,string bvec);
int PrintBuffer(unsigned char *dat,unsigned int n=16);

#include "utils.h"

#endif // ODXTMAIN_H
