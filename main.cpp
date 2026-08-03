#define _GNU_SOURCE

#include <iostream>
#include <cstring>
#include <fstream>
#include <vector>
#include <string>
#include <sstream>
#include <iomanip>

#include <set>
#include <algorithm>
#include <functional>


#include "mainwindow.h"
#include "aes.h"

using namespace std;

int N_keywords = 128480;
int N_max_ids = 10209;

// int N_keywords = 32;
// int N_max_ids = 47;

int N_row_ids = N_max_ids;

string widxdb_file = "wiki_MDB_22k.csv";//Raw enron database
// string widxdb_file = "widxrdb.csv";//Raw enron database
string eidxdb_file = "eidxdb.csv";//Encrypted meta-keyword database
string bloomfilter_file = "bloom_filter.dat";//Bloom filter file

sw::redis::ConnectionOptions connection_options;
sw::redis::ConnectionPoolOptions pool_options;

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

unsigned int N_threads = 94;//Number of threads to use

std::mutex mrun;
std::condition_variable dataReady;
std::condition_variable workComplete;

bool ready = false;
bool processed = false;

size_t nWorkerCount = N_threads;//Number of threads
int nCurrentIteration = 0;

std::vector<thread> thread_pool(N_threads);

int sym_block_size = N_threads * 16;
int ecc_block_size = N_threads * 32;
int hash_block_size = N_threads * 64;
int bhash_block_size = N_threads * 64;
int bhash_in_block_size = N_threads * 40;

int OXT_SetUp()
{
    cout << "Starting program..." << endl;

    UIDX = new unsigned char[16*N_max_ids];
    ::memset(UIDX,0x00,16*N_max_ids);

    Sys_Init();
    
    EDB_SetUp();
    BloomFilter_WriteBFtoFile(bloomfilter_file, BF); //Store bloom filter in file

    Sys_Clear();

    delete [] UIDX;
    cout << "Program finished!" << endl;

    return 0;
}

int OXT_Search_Single()
{
    unsigned char row_vec[32] = {0x00,0x00,0x00,0x06,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
                                 0x00,0x00,0x00,0x0A,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00};
    int n_vec = 1;

    Sys_Init();

    BloomFilter_ReadBFfromFile(bloomfilter_file, BF); //Load bloom filter from file
    std::cout << "BF Read Complete!" << std::endl;
    
    auto start_time = chrono::high_resolution_clock::now();
    
    int nm = EDB_Search(row_vec,n_vec);
    
    auto stop_time = chrono::high_resolution_clock::now();
    auto time_elapsed = chrono::duration_cast<chrono::microseconds>(stop_time - start_time).count();
    
    cout << time_elapsed << endl;

    ofstream outputfile;
    outputfile.open("./MKW_HWt_TV.txt",ios_base::out);

    for(int i=0;i<nm;++i){
        for(int j=0;j<16;++j){
            outputfile << hex << setw(2) << setfill ('0') << static_cast<int>(UIDX[(16*i)+j]);
        }
        outputfile << endl;
    }
    outputfile.close();

    Sys_Clear();

    delete [] UIDX;
    cout << "Program finished!" << endl;
    return 0;
}

int OXT_Search_Full()
{
    cout << "Starting program..." << endl;

    UIDX = new unsigned char[16*N_max_ids];
    ::memset(UIDX,0x00,16*N_max_ids);
    
    vector<std::string> raw_tv_data;
    vector<std::string> raw_fq_data;
    vector<std::string> raw_bv_data;

    vector<std::string> rowdata;
    int n_testvectors = 0;
    string fline;

    unsigned char row_vec[2048]; //Number of keywords in the query
    int n_vec = 0;

    stringstream ss;
    string s;

    std::set<std::string> result_union;
    std::set<std::string> result_union_temp;

    Sys_Init();

    ofstream time_outputfile;
    
    time_outputfile.open("./results/HW3_test2_time.dat",ios_base::out);

    ofstream freq_outputfile;
    freq_outputfile.open("./results/HW3_test2_freq.dat",ios_base::out);

    ofstream outputfile;
    outputfile.open("./results/HW3_test2_union.dat",ios_base::out);

    ifstream tv_file;
    tv_file.open("./test_vectors/IEXHW3_hex_testvector.dat",ios_base::in);

    ifstream fq_file;
    fq_file.open("./test_vectors/IEXHW3_hex_freqvector.dat",ios_base::in);

    ifstream bv_file;
    bv_file.open("./test_vectors/IEXHW3_hex_binvector.dat",ios_base::in);

    n_testvectors = 0;
    while(getline(tv_file,fline)){
        raw_tv_data.push_back(fline);
        fline.clear();
        n_testvectors++;
    }

    while(getline(fq_file,fline)){
        raw_fq_data.push_back(fline);
        fline.clear();
    }

    while(getline(bv_file,fline)){
        raw_bv_data.push_back(fline);
        fline.clear();
    }

    tv_file.close();
    fq_file.close();
    bv_file.close();

    vector<std::string> row_tv_data;
    vector<unsigned int> row_fq_data;
    vector<unsigned int> row_bv_data;

    string q_mkws;
    unsigned int bin_idx;
    unsigned int mkw_freq;

    vector<std::string> mkws;
    int search_count = 0;
    int nm = 0;
    int result_set_size = 0;

    auto start_time = chrono::high_resolution_clock::now();
    auto stop_time = chrono::high_resolution_clock::now();
    auto time_elapsed = chrono::duration_cast<chrono::microseconds>(stop_time - start_time).count();
    auto total_time = chrono::duration_cast<chrono::microseconds>(stop_time - start_time).count();

    BloomFilter_ReadBFfromFile(bloomfilter_file, BF); //Load bloom filter from file

    for(unsigned int i=0;i<n_testvectors;++i){

        result_union.clear();

        cout << "----------------" << endl;
        cout << "Test Vector " << i << endl;

        ss.clear();
		ss << raw_tv_data.at(i);//Get the current testvector row
        std::getline(ss,s,',');// Skip the 0000
		row_tv_data.clear();
		while(std::getline(ss,s,',') && !ss.eof()) {
		    if(!s.empty()){
		        row_tv_data.push_back(s);
		    }
		}

        ss.clear();
		ss << raw_bv_data.at(i);//Get the current bin index row
		row_bv_data.clear();
		while(std::getline(ss,s,',') && !ss.eof()) {
		    if(!s.empty()){
		        row_bv_data.push_back(std::stoi(s));
		    }
		}

        total_time = 0;
        result_set_size = 0;
        mkws.clear();
        for(unsigned int j=0;j<row_bv_data.size();++j){
            if(j<row_bv_data.size()-1){
                if(row_bv_data.at(j) == row_bv_data.at(j+1)){//Push the current mkw to vector if the next mkw is from the same bin
                    mkws.push_back(row_tv_data.at(j));
                }
                else{//Search for previous mkws when an mkw from different bin is encounterd
                    mkws.push_back(row_tv_data.at(j));

                    ::memset(row_vec,0x00,2048);
                    n_vec = 0;

                    for(auto rs:mkws){
                        DB_StrToHex8(row_vec+(16*n_vec),rs.data());//Defined in mainwindow.cpp file
                        // PrintBuffer(row_vec+(16*n_vec),16);
                        n_vec++;
                    }

                    cout << "1 Querying: ";

                    for(auto v:mkws){
                        cout << v << ", ";
                    }
                    cout << endl;
                    
                    //-----------------------------------
                    ::memset(UIDX,0x00,16*N_max_ids);
                    result_union_temp.clear();
                    
                    start_time = chrono::high_resolution_clock::now();
                    nm = EDB_Search(row_vec,n_vec-1);
                    stop_time = chrono::high_resolution_clock::now();
                    time_elapsed = chrono::duration_cast<chrono::microseconds>(stop_time - start_time).count();
                    total_time += time_elapsed;
                    cout << "Time elapsed: " << time_elapsed << " us" << endl;

                    result_set_size += nm;

                    for(unsigned int k=0;k<nm;++k){
                        result_union_temp.insert(DB_HexToStr_N(UIDX+(16*k),16));
                    }
                    //-----------------------------------

                    mkws.clear();
                }
            }
            else{//Need to handle separately when last mkw is same with the preceeding ones
                if(row_bv_data.at(j-1) == row_bv_data.at(j)){
                    // mkws.push_back(row_tv_data.at(j-1));
                    mkws.push_back(row_tv_data.at(j));

                    ::memset(row_vec,0x00,2048);
                    n_vec = 0;

                    for(auto rs:mkws){
                        DB_StrToHex8(row_vec+(16*n_vec),rs.data());//Defined in mainwindow.cpp file
                        // PrintBuffer(row_vec+(16*n_vec),16);
                        n_vec++;
                    }

                    cout << "2 Querying: ";
                    for(auto v:mkws){
                        cout << v << ", ";
                    }
                    cout << endl;
                    
                    //-----------------------------------
                    ::memset(UIDX,0x00,16*N_max_ids);
                    result_union_temp.clear();
                    
                    start_time = chrono::high_resolution_clock::now();
                    nm = EDB_Search(row_vec,n_vec-1);
                    stop_time = chrono::high_resolution_clock::now();
                    time_elapsed = chrono::duration_cast<chrono::microseconds>(stop_time - start_time).count();
                    total_time += time_elapsed;
                    cout << "Time elapsed: " << time_elapsed << " us" << endl;

                    result_set_size += nm;

                    for(unsigned int k=0;k<nm;++k){
                        result_union_temp.insert(DB_HexToStr_N(UIDX+(16*k),16));
                    }

                    //-----------------------------------

                    mkws.clear();
                }
                else{//This is when only the last mkw is from a different bin
                    cout << "3 Querying: ";
                    cout << row_tv_data.at(j) << endl;

                    for(auto rs:mkws){
                        StrToHexBVec(row_vec+(16*n_vec),rs.data());
                        // PrintBuffer(row_vec+(16*n_vec),16);
                        n_vec++;
                    }

                    //-----------------------------------
                    ::memset(UIDX,0x00,16*N_max_ids);
                    result_union_temp.clear();

                    start_time = chrono::high_resolution_clock::now();
                    nm = EDB_Search(row_vec,0);
                    stop_time = chrono::high_resolution_clock::now();
                    time_elapsed = chrono::duration_cast<chrono::microseconds>(stop_time - start_time).count();
                    total_time += time_elapsed;
                    cout << "Time elapsed: " << time_elapsed << " us" << endl;

                    result_set_size += nm;

                    for(unsigned int k=0;k<nm;++k){
                        result_union_temp.insert(DB_HexToStr_N(UIDX+(16*k),16));
                    }
                    //-----------------------------------

                    mkws.clear();
                }
            }
            // result union for each bin
            result_union.insert(result_union_temp.begin(),result_union_temp.end());
            result_union_temp.clear();
        }
        //Write results to file
        for(auto v:result_union){
            outputfile << v << ",";
        }
        outputfile << endl;

        cout << "Total time elapsed: " << total_time << " us"<< endl;
        time_outputfile << total_time << endl;

        freq_outputfile << result_set_size << endl;

        result_union.clear();
        result_union_temp.clear();
    }

    outputfile.close();
    time_outputfile.close();
    freq_outputfile.close();
    
    Sys_Clear();

    delete [] UIDX;
    cout << "Program finished!" << endl;

    return 0;
}

int main()
{
    std::cout << "Starting..." << std::endl;
    // OXT_SetUp();
    // OXT_Search_Single();
    OXT_Search_Full();
    std::cout << "Complete!" << std::endl;

    return 0;
}