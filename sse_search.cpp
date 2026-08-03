#define _GNU_SOURCE

#include <iostream>
#include <cstring>
#include <fstream>
#include <vector>
#include <string>
#include <sstream>
#include <iomanip>
#include "mainwindow.h"
#include "aes.h"

using namespace std;

int N_keywords = 64;//2;
int N_max_ids = 1282;//212;
int N_row_ids = N_max_ids;
//int BF_length = 36 * N_keywords * N_max_ids;//36*64*2048 = 4718592 --- 196608 -- 1048576

string widxdb_file = "widxdb.csv";//Raw meta-keyword database
string eidxdb_file = "eidxdb.csv";//Encrypted meta-keyword database
string bloomfilter_file = "bloom_filter.dat";//Bloom filter file

mongocxx::uri uri_pool{"mongodb://localhost:27017/?minPoolSize=16&maxPoolSize=24"};
mongocxx::pool pool{uri_pool};

mpz_class Prime{"7237005577332262213973186563042994240857116359379907606001950938285454250989",10};//Curve25519 curve order
mpz_class InvExp{"7237005577332262213973186563042994240857116359379907606001950938285454250987",10};

unsigned char ecc_basep[32] = {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x09};

unsigned char KS[16] = {0x2B,0x7E,0x15,0x16,0x28,0xAE,0xD2,0xA6,0xAB,0xF7,0x15,0x88,0x09,0xCF,0x4F,0x3C};
unsigned char KI[16] = {0x2B,0x7E,0x15,0x16,0x28,0xAE,0xD2,0xA6,0xAB,0xF7,0x15,0x88,0x09,0xCF,0x4F,0x3C};
unsigned char KZ[16] = {0x2B,0x7E,0x15,0x16,0x28,0xAE,0xD2,0xA6,0xAB,0xF7,0x15,0x88,0x09,0xCF,0x4F,0x3C};
unsigned char KX[16] = {0x2B,0x7E,0x15,0x16,0x28,0xAE,0xD2,0xA6,0xAB,0xF7,0x15,0x88,0x09,0xCF,0x4F,0x3C};
unsigned char KT[16] = {0x2B,0x7E,0x15,0x16,0x28,0xAE,0xD2,0xA6,0xAB,0xF7,0x15,0x88,0x09,0xCF,0x4F,0x3C};

unsigned char **BF;

unsigned char *UIDX;

unsigned char *GL_AES_PT;
unsigned char *GL_AES_KT;
unsigned char *GL_AES_CT;

unsigned char *GL_ECC_INVA;
unsigned char *GL_ECC_INVP;

unsigned char *GL_ECC_SCA;
unsigned char *GL_ECC_BP;
unsigned char *GL_ECC_SMP;

unsigned char *GL_ECC_INA;
unsigned char *GL_ECC_INB;
unsigned char *GL_ECC_PRD;

unsigned char *GL_HASH_MSG;
unsigned char *GL_HASH_DGST;

unsigned char *GL_BLM_MSG;
unsigned char *GL_BLM_DGST;

unsigned char *GL_MGDB_RES;
unsigned char *GL_MGDB_BIDX;
unsigned char *GL_MGDB_JIDX;
unsigned char *GL_MGDB_LBL;

unsigned int *GL_OPCODE;

unsigned int N_threads = 24;

std::mutex mrun;
std::condition_variable dataReady;
std::condition_variable workComplete;

bool ready = false;
bool processed = false;

size_t nWorkerCount = 24;//Number of threads
int nCurrentIteration = 0;

std::vector<thread> thread_pool(N_threads);

int sym_block_size = N_threads * 16;
int ecc_block_size = N_threads * 32;
int hash_block_size = N_threads * 64;
int bhash_block_size = N_threads * 64;
int bhash_in_block_size = N_threads * 40;

int main()
{
    cout << "Starting program..." << endl;

    UIDX = new unsigned char[16*N_max_ids];
    ::memset(UIDX,0x00,16*N_max_ids);

    unsigned char row_vec[32] = {0x00,0x00,0x00,0x06,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
                                 0x00,0x00,0x00,0x07,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00};
    int n_vec = 1;

    ofstream outputfile;
    outputfile.open("./MKW_HWt_TV.txt",ios_base::out);

    Sys_Init();
    BloomFilter_ReadBFfromFile(bloomfilter_file, BF); //Load bloom filter from file
    int nm = EDB_Search(row_vec,n_vec);

    for(int i=0;i<nm;++i){
        for(int j=0;j<16;++j){
            outputfile << hex << setw(2) << setfill ('0') << static_cast<int>(UIDX[(16*i)+j]);
        }
        outputfile << endl;
    }
    outputfile.close();

    Sys_Clear();
    delete [] UIDX;


    cout << "Search complete!" << endl;

    return 0;
}
