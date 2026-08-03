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

#include <unordered_set>


#include "odxt_main_single_thread.h"
#include "aes.h"

using namespace std;

#define time_t std::chrono::_V2::system_clock::time_point
#define TIME_MARKER() chrono::high_resolution_clock::now()
#define TIME_ELAPSED(start, stop) chrono::duration_cast<chrono::microseconds>(stop - start).count()

// #define TIME_START() start_marker = chrono::high_resolution_clock::now()
// #define TIME_STOP() stop_marker = chrono::high_resolution_clock::now()
// #define TIME_ELAPSED() chrono::duration_cast<chrono::microseconds>(stop_marker - start_marker).count()
// std::chrono::_V2::system_clock::time_point start_marker, stop_marker;

string widxdb_file = "db6k_updates.csv";//Raw enron database
string subdir_name;

mpz_class Prime{"7237005577332262213973186563042994240857116359379907606001950938285454250989",10};//Curve25519 curve order
mpz_class InvExp{"7237005577332262213973186563042994240857116359379907606001950938285454250987",10};

unsigned char ecc_basep[32] = {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x09};

unsigned char KS[16] = {0x2B,0x7E,0x15,0x16,0x28,0xAE,0xD2,0xA6,0xAB,0xF7,0x15,0x88,0x09,0xCF,0x4F,0x3C};
unsigned char KX[16] = {0x2B,0x7E,0x15,0x16,0x28,0xAE,0xD2,0xA6,0xAB,0xF7,0x15,0x88,0x09,0xCF,0x4F,0x3D};
unsigned char KY[16] = {0x2B,0x7E,0x15,0x16,0x28,0xAE,0xD2,0xA6,0xAB,0xF7,0x15,0x88,0x09,0xCF,0x4F,0x3E};
unsigned char KZ[16] = {0x2B,0x7E,0x15,0x16,0x28,0xAE,0xD2,0xA6,0xAB,0xF7,0x15,0x88,0x09,0xCF,0x4F,0x3F};
unsigned char KT[16] = {0x2B,0x7E,0x15,0x16,0x28,0xAE,0xD2,0xA6,0xAB,0xF7,0x15,0x88,0x09,0xCF,0x4F,0x3C};

std::map<std::string,unsigned int> update_count;

MKW_Converter* mdb = NULL;

std::mutex update_count_mutex;


void ODXT_SetUp_Thread(int start, int step, vector<vector<string>>& updates, vector<vector<string>>& mkw_updates, vector<vector<int64_t>>& update_time, vector<vector<int>>& mkw_update_count){
    for(int i=start; i<updates.size(); i+=step){
        auto row = updates[i];
        auto kw = row[0];
        auto id = row[1];
        auto start = TIME_MARKER();
        vector<pair<int,int>> mkws;
        {
            auto mkw_list = mdb->find_mkw(strToInt(kw));
            std::lock_guard<std::mutex> lock(update_count_mutex);
            for(auto mkw:mkw_list){
                auto mkw_str = intToStr(mkw);
                int cnt = update_count[mkw_str]++;
                mkws.push_back({mkw, cnt});
            }
        }
        for(auto [mkw, cnt]:mkws){
            ODXT_Update(intToStr(mkw), kw, id, cnt, 0x01);
            mkw_updates.push_back({intToStr(mkw), kw, id});
        }
        auto stop = TIME_MARKER();
        update_time.push_back({TIME_ELAPSED(start, stop)});
        mkw_update_count.push_back({(int)mkws.size()});
    }
}

int ODXT_SetUp_Top()
{
    cout << "Executing ODXT setup..." << endl;
    string update_count_file = "update_count.csv";

    Redis_Init();
    
    // check for existing setup (check whether the updatecnt file exists)
    if(filesystem::exists(update_count_file)){
        cout << "Loading existing update count..." << endl;
        update_count.clear();
        auto content = read_file(update_count_file);
        for(auto& row:content){
            update_count[row[0]] = stoi(row[1]);
        }
    }
    else{
        cout << "Creating new update count..." << endl;
        Sys_Init();
        ODXT_Setup();

        string mkw_updates_file = "./test_vectors/" + subdir_name + "/mkw_updates.csv";
        string update_time_file = "./test_vectors/" + subdir_name + "/update_time.csv";
        string mkw_update_count_file = "./test_vectors/" + subdir_name + "/mkw_update_count.csv";

        auto updates = read_file(widxdb_file);

        int N_Threads = std::thread::hardware_concurrency();
        // we have to use threads but we have to make sure that the 
        // access to the update_count is synchronized
        // 1: we can access it in the thread wrapper by ourselves and pass the count to ODXT_update
        // 2: Or pass mutex to ODXT_update and lock it before updating the count
        // 1st method is better because we are not passing the mutex to ODXT_update
        
        vector<vector<int64_t>> update_time[N_Threads];
        vector<vector<string>> mkw_updates[N_Threads];
        vector<vector<int>> mkw_update_count[N_Threads];

        vector<std::thread> threads;
        
        for(int i=0; i<N_Threads; i++){
            update_time[i] = vector<vector<int64_t>>();
            mkw_updates[i] = vector<vector<string>>();
            mkw_update_count[i] = vector<vector<int>>();
            threads.emplace_back(ODXT_SetUp_Thread, i, N_Threads, ref(updates), ref(mkw_updates[i]), ref(update_time[i]), ref(mkw_update_count[i]));
        }
        for(auto& t:threads){
            t.join();
        }

        vector<vector<string>> update_count_vec;
        for(auto& kv:update_count){
            update_count_vec.push_back({kv.first, to_string(kv.second)});
        }
        write_file(update_count_file, update_count_vec);

        for(int i=0; i<N_Threads; i++){
            write_file(mkw_updates_file, mkw_updates[i], i!=0);
            write_file(update_time_file, update_time[i], i!=0);
            write_file(mkw_update_count_file, mkw_update_count[i], i!=0);
        }

        Sys_Clear();
    }

    cout << "Update complete!" << endl;

    return 0;
}


void search_single(vector<string> mkw_str, int cnt, set<string>& res, std::mutex& res_mutex){
    unordered_set<string> IdList;
    ODXT_Search(&IdList, mkw_str, cnt);
    {
        std::lock_guard<std::mutex> lock(res_mutex);
        res.insert(IdList.begin(), IdList.end());
    }
}


void ODXT_Search_Thread(int start, int step, vector<vector<string>>& kw_queries, vector<vector<string>>& act_res, vector<vector<string>>& mkw_queries, vector<vector<int>>& bucket_ids, vector<vector<string>>& mkw_result, vector<vector<int>>& mkw_query_count, vector<vector<int>>& bucket_query_count, vector<vector<string>>& query_time, vector<vector<long double>>& precision){
    // how to perform search here
    // perform all essential tasks here
    // we will later consolidate the results using modulo
    // may not work well for updates but is essential for search to check correctness

    for(int i=start; i<kw_queries.size(); i+=step){
        auto query_line_str = kw_queries[i];
        auto start = TIME_MARKER();
        vector<int> query_line;
        for(auto x:query_line_str){
            query_line.push_back(strToInt(x));
        }

        auto buckets = mdb->bucketize_query(query_line);

        mkw_queries.push_back({});
        bucket_ids.push_back({});
        mkw_result.push_back({});
        mkw_query_count.push_back({0});
        bucket_query_count.push_back({(int)buckets.size()});
        
        set<string> res;
        vector<thread> threads;
        std::mutex res_mutex;

        for(int i=0; i<buckets.size(); i++){
            auto mkws = mdb->convert_query(buckets[i]);
            mkw_query_count.back()[0] += mkws.size();
            int upd_cnt = 0;
            {
                std::lock_guard<std::mutex> lock(update_count_mutex);
                pair<int, int> sterm = {update_count[intToStr(mkws[0])], 0};
                for(int i=1; i<mkws.size(); i++){
                    int cnt = update_count[intToStr(mkws[i])];
                    if(cnt < sterm.first){
                        sterm = {cnt, i};
                    }
                }
                swap(mkws[0], mkws[sterm.second]);
                upd_cnt = sterm.first;
            }
            vector<string> mkw_str;
            for(auto mkw:mkws){
                mkw_str.push_back(intToStr(mkw));
                mkw_queries.back().push_back(intToStr(mkw));
                bucket_ids.back().push_back(i);
            }
            threads.emplace_back(search_single, mkw_str, upd_cnt, ref(res), ref(res_mutex));
        }
        for(auto& t:threads){
            t.join();
        }
        auto stop = TIME_MARKER();
        query_time.push_back({to_string(TIME_ELAPSED(start, stop))});
        mkw_result.back() = vector<string>(res.begin(), res.end());
        precision.push_back({(long double) act_res[i].size()/mkw_result.back().size()});
    }
    for(int i=start; i<kw_queries.size(); i+=step){
        if(!check_correctness_helper(act_res[i], mkw_result[i/step])){
            cout << "Incorrect " << i << endl;
            break;
        }
    }
}


void ODXT_Search(){
    string kw_query_file = "./test_vectors/" + subdir_name + "/kw_query.csv";
    string mkw_query_file = "./test_vectors/" + subdir_name + "/mkw_query.csv";
    string bucket_id_file = "./test_vectors/" + subdir_name + "/bucket_id.csv";
    string kw_res_file = "./results/" + subdir_name + "/kw_result.csv";
    string mkw_res_file = "./results/" + subdir_name + "/mkw_result.csv";
    string precision_file = "./results/" + subdir_name + "/precision.csv";
    string query_time_file = "./results/" + subdir_name + "/query_time.csv";
    string mkw_query_count_file = "./results/" + subdir_name + "/mkw_query_count.csv";
    string bucket_query_count_file = "./results/" + subdir_name + "/bucket_query_count.csv";

    
    auto kw_queries = read_file(kw_query_file);
    auto act_res = read_file(kw_res_file);
    
    int N_Threads = std::thread::hardware_concurrency();

    vector<vector<vector<string>>> mkw_queries(N_Threads);
    vector<vector<vector<int>>> bucket_ids(N_Threads);
    vector<vector<vector<string>>> mkw_result(N_Threads);
    vector<vector<vector<string>>> query_time(N_Threads);
    vector<vector<vector<int>>> mkw_query_count(N_Threads);
    vector<vector<vector<int>>> bucket_query_count(N_Threads);
    vector<vector<vector<long double>>> precision(N_Threads);
    
    
    vector<thread> threads;
    for(int i=0; i<N_Threads; i++){
        threads.emplace_back(ODXT_Search_Thread, i, N_Threads, ref(kw_queries), ref(act_res), ref(mkw_queries[i]), ref(bucket_ids[i]), ref(mkw_result[i]), ref(mkw_query_count[i]), ref(bucket_query_count[i]), ref(query_time[i]), ref(precision[i]));
    }
    for(auto& t:threads){
        t.join();
    }

    write_file(mkw_query_file, consolidate(mkw_queries));
    write_file(bucket_id_file, consolidate(bucket_ids));
    write_file(mkw_res_file, consolidate(mkw_result));
    write_file(query_time_file, consolidate(query_time));
    write_file(mkw_query_count_file, consolidate(mkw_query_count));
    write_file(bucket_query_count_file, consolidate(bucket_query_count));
    write_file(precision_file, consolidate(precision));
}


vector<string> gen(int set_bits, int total_bits){
    string vec = string(total_bits-set_bits, '0') + string(set_bits, '1');
    auto seed = rand();
    shuffle(vec.begin(), vec.end(), default_random_engine(seed));
    
    vector<string> ans;
    for(int i=0; i<vec.size(); i++){
        if(vec[i] == '1'){
            ans.push_back(intToStr(i));
        }
    }
    return move(ans);
}

vector<vector<string>> generate_queries(int n_queries, int hamming_weight, int n_words){
    vector<vector<string>> ans;
    for(int i=0; i<n_queries; i++){
        ans.push_back(gen(hamming_weight, n_words));
    }
    return move(ans);
}

vector<vector<string>> kw_query_result(vector<vector<string>> queries){
    auto content = read_file(widxdb_file);
    map<string, vector<string>> kw_map;
    for(auto row:content){
        kw_map[row[0]].push_back(row[1]);
    }
    vector<vector<string>> res;
    for(auto query:queries){
        set<string> s;
        for(auto kw:query){
            s.insert(kw_map[kw].begin(), kw_map[kw].end());
        }
        res.push_back(vector<string>(s.begin(), s.end()));
    }
    return move(res);
}

int old_main(int argc, char **argv)
{
    if(argc != 7){
        cout << "Usage: " << argv[0] << " <test_subdir_name> <number_of_keywords> <bucket_size> <isOptimized> <hamming_weight> <number_of_queries>\n";
        return 1;
    }
    subdir_name = argv[1];
    int nKeywords = stoi(argv[2]);
    int bucketSize = stoi(argv[3]);
    bool isOptimized = stoi(argv[4]);
    int hamming_weight = stoi(argv[5]);
    int nQueries = stoi(argv[6]);
    mdb = new MKW_Converter(bucketSize, isOptimized);

    filesystem::create_directories("./test_vectors/" + subdir_name);
    filesystem::create_directories("./results/" + subdir_name);
    string kw_query_file = "./test_vectors/" + subdir_name + "/kw_query.csv";
    string kw_result_file = "./results/" + subdir_name + "/kw_result.csv";
    
    std::cout << "Starting..." << std::endl;

    time_t start, stop;
    start = TIME_MARKER();
        ODXT_SetUp_Top();
    stop = TIME_MARKER();

    auto setup_time = TIME_ELAPSED(start, stop);
    cout << "Setup time: " << setup_time << " microseconds" << std::endl;

    auto queries = generate_queries(nQueries, hamming_weight, nKeywords);
    auto kw_res = kw_query_result(queries);
    write_file(kw_query_file, queries);
    write_file(kw_result_file, kw_res);
    cout << "Queries generated" << std::endl;


    start = TIME_MARKER();
        ODXT_Search();
    stop = TIME_MARKER();

    auto search_time = TIME_ELAPSED(start, stop);
    cout << "Search time: " << search_time << " microseconds" << std::endl;

    string update_time_file = "./test_vectors/" + subdir_name + "/update_time.csv";
    string mkw_update_count_file = "./test_vectors/" + subdir_name + "/mkw_update_count.csv";
    
    string query_time_file = "./results/" + subdir_name + "/query_time.csv";
    string precision_file = "./results/" + subdir_name + "/precision.csv";
    string mkw_query_count_file = "./results/" + subdir_name + "/mkw_query_count.csv";
    string bucket_query_count_file = "./results/" + subdir_name + "/bucket_query_count.csv";
    string stats_file = "./results/" + subdir_name + "/res_stats.csv";
    
    vector<vector<string>> stats_content;
    stats_content.push_back({to_string(setup_time)});
    stats_content.push_back({to_string(search_time)});
    stats_content.push_back({to_string(average(update_time_file))});
    stats_content.push_back({to_string(average(mkw_update_count_file))});
    stats_content.push_back({to_string(average(query_time_file))});
    stats_content.push_back({to_string(average(precision_file))});
    stats_content.push_back({to_string(average(mkw_query_count_file))});
    stats_content.push_back({to_string(average(bucket_query_count_file))});
    write_file(stats_file, stats_content);

    std::cout << "Complete!" << std::endl;

    return 0;
}