#include "odxt_main.h"

auto redis = Redis("tcp://127.0.0.1:6379");

int Sys_Init()
{
    connection_options.host = "127.0.0.1";  // Required.
    pool_options.size = N_threads;  // Pool size, i.e. max number of connections.

    BloomFilter_Init(BF);
    SetUpThreads();

    return 0;
}

int Sys_Clear()
{
    BloomFilter_Clean(BF);
    ReleaseThreads();

    return 0;
}


int SetUpThreads()
{
    GL_AES_PT = new unsigned char[N_threads*16];
    GL_AES_KT = new unsigned char[16];
    GL_AES_CT = new unsigned char[N_threads*16];
    GL_ECC_INVA = new unsigned char[N_threads*32];
    GL_ECC_INVP = new unsigned char[N_threads*32];
    GL_ECC_SCA = new unsigned char[N_threads*32];
    GL_ECC_BP = new unsigned char[N_threads*32];
    GL_ECC_SMP = new unsigned char[N_threads*32];
    GL_ECC_INB = new unsigned char[N_threads*32];
    GL_ECC_INA = new unsigned char[N_threads*32];
    GL_ECC_PRD = new unsigned char[N_threads*32];
    GL_HASH_MSG = new unsigned char[N_threads*16];
    GL_HASH_DGST = new unsigned char[N_threads*64];
    GL_BLM_MSG = new unsigned char[N_threads*40];
    GL_BLM_DGST = new unsigned char[N_threads*64];

    GL_MGDB_RES = new unsigned char[N_threads*49];
    GL_MGDB_BIDX = new unsigned char[N_threads*2];
    GL_MGDB_JIDX = new unsigned char[N_threads*2];
    GL_MGDB_LBL = new unsigned char[N_threads*12];

    GL_OPCODE = new unsigned int;

    ready = false;
    processed = false;

    nWorkerCount = N_threads;
    nCurrentIteration = 0;

    thread_pool.clear();
    for(unsigned int i=0;i<N_threads;++i){
        thread_pool.push_back(thread(
          WorkerThread,
          GL_AES_PT+(i*16),
          GL_AES_KT,
          GL_AES_CT+(i*16),
          GL_ECC_INVA+(i*32),
          GL_ECC_INVP+(i*32),
          GL_ECC_SCA+(i*32),
          GL_ECC_BP+(i*32),
          GL_ECC_SMP+(i*32),
          GL_ECC_INB+(i*32),
          GL_ECC_INA+(i*32),
          GL_ECC_PRD+(i*32),
          GL_HASH_MSG+(i*16),
          GL_HASH_DGST+(i*64),
          GL_BLM_MSG+(i*40),
          GL_BLM_DGST+(i*64),
          GL_MGDB_RES+(i*49),
          GL_MGDB_BIDX+(i*2),
          GL_MGDB_JIDX+(i*2),
          GL_MGDB_LBL+(i*12),
          GL_OPCODE
        ));
    }

    int rc = 0;
    for(unsigned int i=0;i<N_threads;++i){
          cpu_set_t cpuset;
          CPU_ZERO(&cpuset);
          CPU_SET(i, &cpuset);
          rc = pthread_setaffinity_np(thread_pool[i].native_handle(), sizeof(cpu_set_t), &cpuset);
          if (rc != 0) {
              std::cout << "Error calling pthread_setaffinity_np: " << rc << std::endl;
          }
    }

    return 0;
}

int ReleaseThreads()
{

    {
        std::lock_guard<std::mutex> lock(mrun);
        nWorkerCount = N_threads;
        processed = true;
        ++nCurrentIteration;
    }
    dataReady.notify_all();

    {
      std::unique_lock<std::mutex> lock(mrun);
      workComplete.wait(lock, [] { return nWorkerCount == 0; });
    }

    for(std::thread &every_thread : thread_pool){
        every_thread.join();
    }

    thread_pool.clear();

    delete [] GL_AES_PT;
    delete [] GL_AES_KT;
    delete [] GL_AES_CT;
    delete [] GL_ECC_INVA;
    delete [] GL_ECC_INVP;
    delete [] GL_ECC_SCA;
    delete [] GL_ECC_BP;
    delete [] GL_ECC_SMP;
    delete [] GL_ECC_INB;
    delete [] GL_ECC_INA;
    delete [] GL_ECC_PRD;
    delete [] GL_HASH_MSG;
    delete [] GL_HASH_DGST;
    delete [] GL_BLM_MSG;
    delete [] GL_BLM_DGST;

    delete [] GL_MGDB_RES;
    delete [] GL_MGDB_BIDX;
    delete [] GL_MGDB_JIDX;
    delete [] GL_MGDB_LBL;

    delete GL_OPCODE;

    return 0;
}

int WorkerThread(
  unsigned char *AES_PT,
  unsigned char *AES_KT,
  unsigned char *AES_CT,
  unsigned char *ECC_INVA,
  unsigned char *ECC_INVP,
  unsigned char *ECC_SCA,
  unsigned char *ECC_BP,
  unsigned char *ECC_SMP,
  unsigned char *ECC_INA,
  unsigned char *ECC_INB,
  unsigned char *ECC_PRD,
  unsigned char *HASH_MSG,
  unsigned char *HASH_DGST,
  unsigned char *BLM_MSG,
  unsigned char *BLM_DGST,
  unsigned char *MGDB_RES,
  unsigned char *MGDB_BIDX,
  unsigned char *MGDB_JIDX,
  unsigned char *MGDB_LBL,
  unsigned int *OPCODE
)
{

    int nNextIteration = 1;

    Redis redis_thread(connection_options, pool_options);
    blake3_hasher hasher;
    blake3_hasher_init(&hasher);

    while(!processed){
        std::unique_lock<std::mutex> lock(mrun);
        dataReady.wait(lock, [&nNextIteration] { return nCurrentIteration==nNextIteration; });
        lock.unlock();

        ++nNextIteration;

        //Do thread stuff here
        if(*OPCODE == 1){
            AESENC(AES_CT,AES_PT,AES_KT);
        }
        else if(*OPCODE == 2){
            SHA3_HASH(&hasher,HASH_MSG,HASH_DGST);
        }
        else if(*OPCODE == 3){
            SHA3_HASH_K(&hasher,BLM_MSG,BLM_DGST);
        }
        else if(*OPCODE == 4){
            ECC_MUL(ECC_INA,ECC_INB,ECC_PRD);
        }
        else if(*OPCODE == 5){
            ECC_FPINV(ECC_INVA,ECC_INVP);
        }
        else if(*OPCODE == 6){
            ScalarMul(ECC_SMP,ECC_SCA,ecc_basep);
        }
        else if(*OPCODE == 7){
            ScalarMul(ECC_SMP,ECC_SCA,ECC_BP);
        }
        else if(*OPCODE == 8){
            string s = HexToStr(MGDB_BIDX,2) + HexToStr(MGDB_JIDX,2) + HexToStr(MGDB_LBL,12);
            auto val = redis_thread.get(s);
            unsigned char *t_res = reinterpret_cast<unsigned char *>(val->data());
            DB_StrToHex49(MGDB_RES,t_res);
        }
        else{
            std::this_thread::sleep_for (std::chrono::milliseconds(1));//Check if it results in computation error
        }

        lock.lock();
        if (--nWorkerCount == 0)
        {
          lock.unlock();
          workComplete.notify_one();
        }
    }
    return 0;
}

int ODXT_Setup()
{
    //Sample KT
    //Hardcoded in .h file

    //Sample KX, KY, KZ for PRF
    //Hardcoded in .h file

    update_count.clear();

    //Clear TSet -- do manually, clear redis
    //Clear XSet -  do manually, close the program and delete the file on disk

    return 0;
}

int ODXT_Update(std::string keyword, std::string id, unsigned char op)
{
    //////////////////////////////////////////////////////////////
    //Server
    //////////////////////////////////////////////////////////////
    
    unsigned char keyword_chars[4];
    unsigned char keyword_chars_all[16];
    unsigned char id_chars[4];
    unsigned char addr_input[16];
    unsigned char addr[16];

    unsigned char val_input[16];
    unsigned char val[16];

    unsigned char alpha_inv_in[32];
    unsigned char alpha_inv_out[32];
    unsigned char alpha_prf_in[16];
    unsigned char alpha_prf[32];
    unsigned char alpha[32];

    unsigned char xtag_exp_1[32];
    unsigned char xtag_exp[32];
    unsigned char xtag[32];

    ::memset(keyword_chars,0x00,4);
    ::memset(keyword_chars_all,0x00,16);

    ::memset(addr_input,0x00,16);
    ::memset(addr,0x00,16);

    ::memset(val_input,0x00,16);
    ::memset(val,0x00,16);

    //Check if keyword is present or not
    if(update_count.count(keyword) == 0){
        update_count.insert(std::make_pair(keyword,0));
    }

    auto cnt = (update_count[keyword]++);

    //Copy keyword
    StrToHex(keyword_chars, keyword, 4);
    StrToHex(keyword_chars_all, keyword, 16);

    //Copy id
    StrToHex(id_chars, id, 4);
    
    //Copy keyword
    ::memcpy(addr_input,keyword_chars,4);
    //Assume 4 bytes kw, 4 bytes count, 1 byte indicator, rest guard bytes between kw and count
    //Append last 4 bytes (with one left for indicator)
    addr_input[11] = cnt & 0xFF;
    addr_input[12] = (cnt >> 8) & 0xFF;
    addr_input[13] = (cnt >> 16) & 0xFF;
    addr_input[14] = (cnt >> 24) & 0xFF;

    //Append indicator variable
    addr_input[15] = 0x00;

    //Compute PRF, key is inbuilt
    PRF_K(addr,addr_input);

    //Append indicator variable, only the last byte changes for val coumputation
    addr_input[15] = 0x01;

    //Compute PRF, key is inbuilt
    PRF_K(val_input,addr_input);

    //Pre-load val with id and op
    ::memset(val,0x00,16);
    ::memcpy(val,id_chars,4);
    val[4] = op;

    //Xor id||op and PRF output
    for(unsigned int i=0;i<16;++i){
        val[i] = val[i] ^ val_input[i];
    }

    //Clear the indicator variable
    addr_input[15] = 0x00;

    //Compute PRF using Kz
    ::memset(alpha_inv_in,0x00,32);
    PRF_K(alpha_inv_in,addr_input);

    //Compute inverse
    ::memset(alpha_inv_out,0x00,32);
    ECC_FPINV(alpha_inv_in, alpha_inv_out);

    //Pre-load alpha_prf_in with id and op
    ::memset(alpha_prf_in,0x00,16);
    ::memcpy(alpha_prf_in,id_chars,4);
    alpha_prf_in[4] = op;

    //Compute PRF
    ::memset(alpha_prf,0x00,32);
    PRF_K(alpha_prf,alpha_prf_in);

    //Multiply
    ::memset(alpha,0x00,32);
    ECC_MUL(alpha_prf,alpha_inv_out,alpha);

    //Compute F(Kx,w)
    ::memset(xtag_exp_1,0x00,32);
    PRF_K(xtag_exp_1,keyword_chars_all);

    //Multiply
    ::memset(xtag_exp,0x00,32);
    ECC_MUL(xtag_exp_1,alpha_prf_in,xtag_exp);

    //Compute xtag
    ::memset(xtag,0x00,32);
    ScalarMul(xtag,xtag_exp,ecc_basep);

    //////////////////////////////////////////////////////////////
    //Server
    //////////////////////////////////////////////////////////////

    auto tset_key = HexToStr(addr,16);
    auto tset_val = HexToStr(val,16) + HexToStr(alpha,32);

    auto xset_key = HexToStr(xtag,32);
    auto xset_val = std::string("1");
    
    redis.set(tset_key.data(), tset_val.data());
    redis.set(xset_key.data(), xset_val.data());

    return 0;
}

int ODXT_Search(std::vector<std::string> query)
{
    //////////////////////////////////////////////////////////////
    //Client
    //////////////////////////////////////////////////////////////

    unsigned char saddrj_in[16];
    unsigned char saddrj[16];

    unsigned char xtoken_w1_prf_in[16];
    unsigned char xtoken_w1_prf_out[32];

    unsigned char xtoken_wi_prf_in[16];
    unsigned char xtoken_wi_prf_out[32];

    unsigned char xtoken_exp[32];
    unsigned char xtoken_j[32];

    //Stokentlist
    std::vector<std::string> stokenList;
    std::vector<std::vector<std::string>> xtokenList;

    //Get update count
    auto update_cnt = update_count[query.at(0)];
    //Get least frequent keyword, order the keywords in this way
    auto w1 = query.at(0).data();
    //Number of crossterms
    auto n = query.size()-1;//Remeber! Number of cross terms, not all query keywords!

    for(unsigned int j=0;j<update_cnt;++j){
        ::memset(saddrj_in,0x00,16);
        //Copy w1
        StrToHex(saddrj_in,w1,4);
        
        //Append counter value
        saddrj_in[11] = j & 0xFF;
        saddrj_in[12] = (j >> 8) & 0xFF;
        saddrj_in[13] = (j >> 16) & 0xFF;
        saddrj_in[14] = (j >> 24) & 0xFF;

        //Append indicator variable
        saddrj_in[15] = 0x00;

        //Compute PRF
        ::memset(saddrj,0x00,16);
        PRF_K(saddrj,saddrj_in);

        //Insert into stokenList
        stokenList.push_back(HexToStr(saddrj,16));

        //Create xtokenlist container
        std::vector<std::string> xtokenList_j;

        for(unsigned int i=0;i<n;++i){
            //F(Kz,w1||j)
            ::memset(xtoken_w1_prf_out,0x00,32);
            PRF_K(xtoken_w1_prf_out,saddrj_in);

            //F(Kx,wi)
            ::memset(xtoken_wi_prf_out,0x00,32);
            ::memset(xtoken_wi_prf_in,0x00,16);
            //Copy wi
            StrToHex(xtoken_wi_prf_in,query.at(i+1),4);
            PRF_K(xtoken_wi_prf_out,xtoken_wi_prf_in);

            //Multiply
            ::memset(xtoken_exp,0x00,32);
            ECC_MUL(xtoken_w1_prf_out,xtoken_wi_prf_out,xtoken_exp);

            //Scalar multiplication
            ::memset(xtoken_j,0x00,32);
            ScalarMul(xtoken_j,xtoken_exp,ecc_basep);

            //Insert into container
            xtokenList_j.push_back(HexToStr(xtoken_j,32));
        }
        //Insert xtokenlist_j into container
        xtokenList.push_back(xtokenList_j);
    }

    //////////////////////////////////////////////////////////////
    //Server
    //////////////////////////////////////////////////////////////

    std::vector<std::pair<std::string,std::pair<unsigned int, unsigned int>>> sEOpList;
    unsigned int cnt_j = 0;
    unsigned char sval_alpha[32];
    unsigned char sval[16];
    unsigned char alpha[32];
    unsigned char xtoken[32];
    unsigned char xtag[32];

    //Get stoken list size
    auto stokelist_size = stokenList.size();

    for(unsigned int j=0;j<stokelist_size;++j){
        cnt_j = 1;

        ::memset(sval_alpha,0x00,32);
        ::memset(sval,0x00,16);
        ::memset(alpha,0x00,32);

        //TSet retrieval
        string s = stokenList[j];
        auto val = redis.get(s);
        // unsigned char *t_res = reinterpret_cast<unsigned char *>(val->data());
        StrToHex(sval_alpha,val,32);
        ::memcpy(sval,sval_alpha,16);
        ::memcpy(alpha,sval_alpha+16,16);//Need to copy only 16 bytes, rest are zero

        //Get the xtokenlist_j
        auto xtokelist_j = xtokenList[j];
        for(unsigned int i=0;i<n;++i){
            //Retrieve xtoken
            auto xtoken_str = xtokelist_j[i];

            //Scalar multiplication
            ::memset(xtag,0x00,32);
            ::memset(xtoken,0x00,32);
            StrToHex(xtoken,xtoken_str,32);
            ScalarMul(xtag,alpha,xtoken);

            //XSet retrieval
            string xtag_str = HexToStr(xtag,32);
            auto val = redis.get(xtag_str);
            
            //If val exists, the increment count
            if(val){
                cnt_j++;
            }
        }
        //Insert into sEOpList (j,sval,cnt_j) == (sval,(j,cnt_j))
        sEOpList.push_back(std::make_pair(HexToStr(sval,16),std::make_pair(j,cnt_j)));//Send this to client
    }

    //////////////////////////////////////////////////////////////
    //Client
    //////////////////////////////////////////////////////////////
    std::unordered_set<std::string> IdList;

    unsigned char sval_hex[16];
    unsigned char prf_kt_in[16];
    unsigned char prf_kt_out[16];

    unsigned char id_op[16];

    auto seoplist_size = sEOpList.size();

    for(unsigned int l=0;l<seoplist_size;++l){
        auto sval = sEOpList[l].first;
        auto j = sEOpList[l].second.first;
        auto cnt_j = sEOpList[l].second.second;

        ::memset(sval_hex,0x00,16);
        ::memset(prf_kt_in,0x00,16);

        StrToHex(sval_hex,sval,16);
        StrToHex(prf_kt_in,w1,4);
        
        //Append j value
        prf_kt_in[11] = j & 0xFF;
        prf_kt_in[12] = (j >> 8) & 0xFF;
        prf_kt_in[13] = (j >> 16) & 0xFF;
        prf_kt_in[14] = (j >> 24) & 0xFF;
        prf_kt_in[15] = 0x01;

        //Compute PRF
        PRF_K(prf_kt_out,prf_kt_in);

        for(unsigned int k=0;k<16;++k){
            id_op[k] = sval_hex[k] ^ prf_kt_out[k];
        }

        if((id_op[4] == 0x01) && (cnt_j == n)){
            IdList.insert(HexToStr(id_op,4));
        }
        else if((id_op[4] == 0x00) && (cnt_j > 0)){
            IdList.erase(HexToStr(id_op,4));
        }
    }

    return 0;
}

int EDB_SetUp()
{
    unsigned char *W;
    unsigned char *KE;
    unsigned char *ID;
    unsigned char *XID;
    unsigned char *XIDA;
    unsigned char *WC;
    unsigned char *ZW;
    unsigned char *ZWP;
    unsigned char *EC;
    unsigned char *ZWI;
    unsigned char *YID;
    
    int N_words = 0;
    unsigned int N_max_id_words = 0;
    
    N_words = (N_max_ids/N_threads) + ((N_max_ids%N_threads==0)?0:1);
    N_max_id_words = N_words * N_threads;

    W = new unsigned char[16];//Holds the keyword
    KE = new unsigned char[16];//ID encryption key
    ID = new unsigned char[16*N_max_id_words];//Maximum number of IDs in a row
    XID = new unsigned char[16*N_max_id_words];//Maximum number of IDs in a row
    XIDA = new unsigned char[32*N_max_id_words];//XID for multiplication with Zinv
    WC = new unsigned char[16*N_max_id_words];//IDs with counter value
    ZW = new unsigned char[16*N_max_id_words];//Z value
    ZWP = new unsigned char[32*N_max_id_words];//Z value for computing inverse
    EC = new unsigned char[16*N_max_id_words];//Encrypted IDs
    ZWI = new unsigned char[32*N_max_id_words];//Z values inverse
    YID = new unsigned char[32*N_max_id_words];//Encrypted Y (mul of XIDA and ZWI)

    ifstream widxdb_file_handle;
    widxdb_file_handle.open(widxdb_file,ios_base::in|ios_base::binary);

    ofstream eidxdb_file_handle;
    eidxdb_file_handle.open(eidxdb_file,ios_base::out|ios_base::binary);

    stringstream ss;

    string widxdb_row;
    vector<string> widxdb_data;
    string widxdb_row_current;
    string s;

    ::memset(W,0x00,16);
    ::memset(KE,0x00,16);
    ::memset(ID,0x00,16*N_max_id_words);
    ::memset(XID,0x00,16*N_max_id_words);
    ::memset(WC,0x00,16*N_max_id_words);
    ::memset(ZWI,0x00,32*N_max_id_words);
    ::memset(ZWP,0x00,32*N_max_id_words);
    ::memset(XIDA,0x00,32*N_max_id_words);
    ::memset(ZW,0x00,16*N_max_id_words);
    ::memset(EC,0x00,16*N_max_id_words);
    ::memset(YID,0x00,32*N_max_id_words);

    unsigned char *zw_local = ZW;
    unsigned char *zwp_local = ZWP;
    unsigned char *zwi_local = ZWI;
    unsigned char *id_local = ID;
    unsigned char *xid_local = XID;
    unsigned char *xida_local = XIDA;
    unsigned char *wc_local = WC;
    unsigned char *ec_local = EC;
    unsigned char *yid_local = YID;

    unsigned char *local_s;

    //int N_words = 0;
    int n_rows = 0;
    int n_row_ids = 0;

    widxdb_row.clear();
    while(getline(widxdb_file_handle,widxdb_row)){
        widxdb_data.push_back(widxdb_row);
        widxdb_row.clear();
        n_rows++;
    }

    cout << "Number of Keywords: " << n_rows << endl;

    widxdb_file_handle.close();

    for(int n=0;n<n_rows;++n){

        zw_local = ZW;
        zwp_local = ZWP;
        xid_local = XID;
        wc_local = WC;

        ::memset(KE,0x00,16);
        ::memset(ID,0x00,16*N_max_id_words);
        ::memset(ZWP,0x00,32*N_max_id_words);
        ::memset(ZWI,0x00,32*N_max_id_words);
        ::memset(XIDA,0x00,32*N_max_id_words);
        ::memset(EC,0x00,16*N_max_id_words);
        ::memset(YID,0x00,32*N_max_id_words);

        widxdb_row_current = widxdb_data.at(n);

        ss.str(std::string());
        ss << widxdb_row_current;
        std::getline(ss,s,',');//Get the keyword

        DB_StrToHex8(W,s.data());//Read the keyword

        id_local = ID;

        n_row_ids = 0;//For row id count
        while(std::getline(ss,s,',') && !ss.eof()) {
            if(!s.empty()){
                DB_StrToHex8(id_local,s.data());//Read the id
                id_local += 16;
                n_row_ids++;
            }
        }

        ss.clear();
        ss.seekg(0);

        N_words = (n_row_ids/N_threads) + ((n_row_ids%N_threads==0)?0:1);

        id_local = ID;//Set local id pointer to the beginning of the array

        AESENC(KE,W,KS);//Generate KE from W and KS

        id_local = ID;
        xid_local = XID;
        for(int nword = 0;nword < N_words;++nword){
            FPGA_AES_ENC(id_local,KI,xid_local);
            id_local += sym_block_size;
            xid_local += sym_block_size;
        }
        id_local = ID;
        xid_local = XID;


        //PRF with C value Appended
        wc_local = WC;
        for(int nword = 0;nword < N_words;++nword){
            for(int nid=0;nid<N_threads;++nid){
    	         ::memcpy(wc_local,W,16);
               wc_local += 16;
            }
        }
        wc_local = WC;
        
        //Compute Zw after appending c
        unsigned int count_wc = 0;
        unsigned int count_wc_local = 0;
        for(unsigned int nword = 0;nword < n_row_ids;++nword){
            count_wc_local = count_wc;
            WC[(nword*16)+15] = count_wc_local & 0xFF;
            count_wc_local >>= 8;
            WC[(nword*16)+14] = count_wc_local & 0xFF;
            count_wc++;
        }


        wc_local = WC;
        zw_local = ZW;
        for(int nword = 0;nword < N_words;++nword){
            //FPGA_AES_ENC_C(wc_local,KZ,zw_local);
            FPGA_AES_ENC(wc_local,KZ,zw_local);
            wc_local += sym_block_size;
            zw_local += sym_block_size;
        }
        wc_local = WC;
        zw_local = ZW;

        //AES Encryption of id using KE
        id_local = ID;
        ec_local = EC;
        for(int nword = 0;nword < N_words;++nword){
            FPGA_AES_ENC(id_local,KE,ec_local);
            id_local += sym_block_size;
            ec_local += sym_block_size;
        }
        id_local = ID;
        ec_local = EC;

        //Prepare for inverse computation
        zw_local = ZW;
        zwp_local = ZWP;
        for(int nword = 0;nword < N_words;++nword){
            for(int nid=0;nid<N_threads;++nid){
                ::memcpy(zwp_local+16,zw_local,16);
                zw_local += 16;//sym_block_size;
                zwp_local += 32;//ecc_block_size;
            }
        }
        zw_local = ZW;
        zwp_local = ZWP;

        //Compute inverse of zw
        zwi_local = ZWI;
        zwp_local = ZWP;
        for(int nword = 0;nword < N_words;++nword){
            FPGA_ECC_FPINV(zwp_local,zwi_local);
            zwp_local += ecc_block_size;
            zwi_local += ecc_block_size;
        }
        zwi_local = ZWI;
        zwp_local = ZWP;

        //Copy XID array for multiplication
        xid_local = XID;
        xida_local = XIDA;
        for(int nword = 0;nword < N_words;++nword){
            for(int nid=0;nid<N_threads;++nid){
                ::memcpy(xida_local+16,xid_local,16);
                xid_local += 16;
                xida_local += 32;
            }
        }
        xid_local = XID;
        xida_local = XIDA;

        //Multiply XID and ZWI  = YC
        xida_local = XIDA;
        zwi_local = ZWI;
        yid_local = YID;
        for(int nword = 0;nword < N_words;++nword){
            FPGA_ECC_MUL(xida_local,zwi_local,yid_local); //Verify again
            xida_local += ecc_block_size;
            zwi_local += ecc_block_size;
            yid_local += ecc_block_size;
        }
        xida_local = XIDA;
        zwi_local = ZWI;
        yid_local = YID;


        eidxdb_file_handle << DB_HexToStr8(W) << ',';
        for(int n_eidx=0;n_eidx < n_row_ids;++n_eidx){
            eidxdb_file_handle << HexToStr(YID+(32*n_eidx),32) << HexToStr(EC+(16*n_eidx),16) + ",";
        }
        eidxdb_file_handle << endl;
    }

    eidxdb_file_handle.close();

/////////////////////////////////////////////////////////////////////////////////////////////////

    cout << "Encrypted Indices Generation Done!" << endl;

    TSet_SetUp();

    cout << "TSet SetUp Done!" << endl;

/////////////////////////////////////////////////////////////////////////////////////////////////

    unsigned char *bhash;
    unsigned char *kxw;
    unsigned char *kxw_arr;
    unsigned char *xid_arr;
    unsigned char *xid_kwx;
    unsigned char *gfp_kwx;

    bhash = new unsigned char[64*N_threads];
    kxw = new unsigned char[16];
    kxw_arr = new unsigned char[32*N_max_id_words];
    xid_arr = new unsigned char[32*N_max_id_words];
    xid_kwx = new unsigned char[32*N_max_id_words];
    gfp_kwx = new unsigned char[32*N_max_id_words];

    unsigned int bfidx = 0;
    unsigned int bf_indices[N_HASH];

    ::memset(bhash,0x00,64*N_threads);
    ::memset(W,0x00,16);
    ::memset(kxw,0x00,16);
    ::memset(kxw_arr,0x00,32*N_max_id_words);
    ::memset(XID,0x00,16*N_max_id_words);

    unsigned char *kxw_arr_local = kxw_arr;
    unsigned char *xid_arr_local = xid_arr;
    unsigned char *xid_kwx_local = xid_kwx;
    unsigned char *gfp_kwx_local = gfp_kwx;

    xid_local = XID;//declared previously

    ss.clear();
    ss.seekg(0);

    for(int n=0;n<n_rows;++n){
        ::memset(W,0x00,16);
        ::memset(ID,0x00,16*N_max_id_words);
        ::memset(XID,0x00,16*N_max_id_words);

        ::memset(kxw_arr,0x00,32*N_max_id_words);
        ::memset(xid_arr,0x00,32*N_max_id_words);
        ::memset(xid_kwx,0x00,32*N_max_id_words);
        ::memset(gfp_kwx,0x00,32*N_max_id_words);

        id_local = ID;

        widxdb_row_current = widxdb_data.at(n);

        ss.str(std::string());
        ss << widxdb_row_current;

        std::getline(ss,s,',');//Get the keyword
        DB_StrToHex8(W,s.data());//Read the keyword

        n_row_ids = 0;//Row id count
        while(std::getline(ss,s,',') && !ss.eof()) {
            if(!s.empty()){
                DB_StrToHex8(id_local,s.data());//Read the id
                id_local += 16;
                n_row_ids++;
            }
        }

        ss.clear();
        ss.seekg(0);

        N_words = (n_row_ids/N_threads) + ((n_row_ids%N_threads==0)?0:1);

        id_local = ID;

        AESENC(kxw,W,KX);

        //Encrypt IDs
        id_local = ID;
        xid_local = XID;
        for(int nword = 0;nword < N_words;++nword){
            FPGA_AES_ENC(id_local,KI,xid_local);
            id_local += sym_block_size;
            xid_local += sym_block_size;
        }
        id_local = ID;
        xid_local = XID;

        //Copy XID array for multiplication
        kxw_arr_local = kxw_arr;
        xid_arr_local = xid_arr;
        xid_local = XID;
        for(int nword = 0;nword < N_words;++nword){
            for(int nid=0;nid<N_threads;++nid){
                ::memcpy(kxw_arr_local+16,kxw,16);
                ::memcpy(xid_arr_local+16,xid_local,16);
                kxw_arr_local += 32;
                xid_arr_local += 32;
                xid_local += 16;
            }
        }
        kxw_arr_local = kxw_arr;
        xid_arr_local = xid_arr;
        xid_local = XID;

        //Multiply Keyword - IDs
        kxw_arr_local = kxw_arr;
        xid_arr_local = xid_arr;
        xid_kwx_local = xid_kwx;
        for(int nword = 0;nword < N_words;++nword){
            FPGA_ECC_MUL(kxw_arr_local,xid_arr_local,xid_kwx_local);
            kxw_arr_local += ecc_block_size;
            xid_arr_local += ecc_block_size;
            xid_kwx_local += ecc_block_size;
        }
        kxw_arr_local = kxw_arr;
        xid_arr_local = xid_arr;
        xid_kwx_local = xid_kwx;

        //Multiply Keyword - IDs
        gfp_kwx_local = gfp_kwx;
        xid_kwx_local = xid_kwx;
        for(int nword = 0;nword < N_words;++nword){
            FPGA_ECC_SCAMUL(xid_kwx_local,gfp_kwx_local);
            gfp_kwx_local += ecc_block_size;
            xid_kwx_local += ecc_block_size;
        }
        gfp_kwx_local = gfp_kwx;
        xid_kwx_local = xid_kwx;

        for(int i=0;i<n_row_ids;++i){

            ::memset(bhash,0x00,bhash_block_size);
            ::memset(bf_indices,0x00,N_HASH);
            FPGA_BLOOM_HASH(gfp_kwx_local,bhash);

            for(int j=0;j<N_HASH;++j){
                //27 bits for 134,217,728
                bf_indices[j] = (bhash[64*j] & 0xFF) + ((bhash[64*j+1] & 0xFF) << 8) + ((bhash[64*j+2] & 0xFF) << 16) + ((bhash[64*j+3] & 0x07) << 24);//Change here!
                // bf_indices[j] = (bhash[64*j] & 0xFF) + ((bhash[64*j+1] & 0xFF) << 8);//Change here!
            }

            BloomFilter_Set(BF, bf_indices);

            gfp_kwx_local +=32;
        }

        gfp_kwx_local = gfp_kwx;
    }

    cout << "Bloom filter generation complete!" << endl;

    delete [] bhash;
    delete [] kxw;
    delete [] kxw_arr;
    delete [] xid_arr;
    delete [] xid_kwx;
    delete [] gfp_kwx;

    delete [] W;
    delete [] KE;
    delete [] ID;
    delete [] XID;
    delete [] XIDA;
    delete [] WC;
    delete [] ZW;
    delete [] ZWP;
    delete [] EC;
    delete [] ZWI;
    delete [] YID;

    return 0;
}

int EDB_Search(unsigned char *query_str, int NWords)
{
    unsigned char Q1[16];

    unsigned char *stag;
    unsigned char *tset_row;
    unsigned char *WC;
    unsigned char *FW1;
    unsigned char *KXWL;
    unsigned char *FPKXWL;
    unsigned char *G_WC;
    unsigned char *G_FW1;
    unsigned char *GFW_KX;
    unsigned char *XTOKEN;
    unsigned char *XTAG;
    unsigned char *bhash;
    unsigned char *ESET;
    
    int N_words = 0;
    unsigned int N_max_id_words = 0;
    
    N_words = (N_max_ids/N_threads) + ((N_max_ids%N_threads==0)?0:1);
    N_max_id_words = N_words * N_threads;

    stag = new unsigned char[16];
    tset_row = new unsigned char[48*N_max_id_words];
    WC = new unsigned char[16*N_max_id_words];
    FW1 = new unsigned char[16*N_max_id_words];
    KXWL = new unsigned char[16*N_max_id_words];
    FPKXWL = new unsigned char[16*N_max_id_words];
    G_WC = new unsigned char[32*N_max_id_words];
    G_FW1 = new unsigned char[32*N_max_id_words];
    GFW_KX = new unsigned char[32*N_max_id_words];
    XTOKEN = new unsigned char[32*N_max_id_words];
    XTAG = new unsigned char[32*N_max_id_words];
    bhash = new unsigned char[64*N_threads];
    ESET = new unsigned char[16*N_max_id_words];

    unsigned char *YID_ALL;
    YID_ALL = new unsigned char[32*N_max_id_words];

    unsigned char YID[32];
    unsigned char ECE[16];

    bool idx_in_set = false;
    int nmatch = 0;
    unsigned int bfidx = 0;

    unsigned int** bf_n_indices;

    int n_ids_tset = 0;

    double time_elapsed;

    bf_n_indices = new unsigned int * [N_HASH];
    for(unsigned int i=0;i<N_HASH;++i){
        bf_n_indices[i] = new unsigned int [NWords];//check what happens if NWords is 0
    }

    ::memset(stag,0x00,16);
    ::memset(tset_row,0x00,48*N_max_id_words);
    ::memset(WC,0x00,16*N_max_id_words);
    ::memset(FW1,0x00,16*N_max_id_words);
    ::memset(KXWL,0x00,16*N_max_id_words);
    ::memset(FPKXWL,0x00,16*N_max_id_words);
    ::memset(G_WC,0x00,32*N_max_id_words);
    ::memset(G_FW1,0x00,32*N_max_id_words);
    ::memset(GFW_KX,0x00,32*N_max_id_words);
    ::memset(XTOKEN,0x00,32*N_max_id_words);
    ::memset(XTAG,0x00,32*N_max_id_words);
    ::memset(ESET,0x00,16*N_max_id_words);
    ::memset(YID_ALL,0x00,32*N_max_id_words);

    unsigned char *wc_local = WC;
    unsigned char *fw1_local = FW1;

    unsigned char *kxwl_local = KXWL;
    unsigned char *fpkxwl_local = FPKXWL;

    unsigned char *g_wc_local = G_WC;
    unsigned char *g_fw1_local = G_FW1;

    unsigned char *tset_row_local = tset_row;
    unsigned char *xtg_local = nullptr;
    unsigned char *eset_local = ESET;
    unsigned char *yid_local = YID_ALL;

    ::memcpy(Q1,query_str,16);

    // for(int i=0;i<NWords;++i){
    //     PrintBuffer(query_str+(i*16),16);
    // }

    TSet_GetTag(Q1,stag);
    // PrintBuffer(stag,16);
    TSet_Retrieve(stag,tset_row,&n_ids_tset);

    cout << "N IDs TSet: " << n_ids_tset << endl;

    N_words = (n_ids_tset/N_threads) + ((n_ids_tset%N_threads==0)?0:1);

    for(int i=0;i< N_max_id_words;++i)
    {
        ::memcpy(WC+(i*16),Q1,16);
    }

	  //Copy all query keywords except first
    ::memcpy(KXWL,query_str+16,(NWords*16));

    unsigned int count_wc = 0;
    unsigned int count_wc_local = 0;
    for(unsigned int nword = 0;nword < n_ids_tset;++nword){
        count_wc_local = count_wc;
        WC[(nword*16)+15] = count_wc_local & 0xFF;
        count_wc_local >>= 8;
        WC[(nword*16)+14] = count_wc_local & 0xFF;
        count_wc++;
    }

    wc_local = WC;
    fw1_local = FW1;
    for(int nword = 0;nword < N_words;++nword){
        FPGA_AES_ENC(wc_local,KZ,fw1_local);
        wc_local += sym_block_size;
        fw1_local += sym_block_size;
    }
    wc_local = WC;
    fw1_local = FW1;

    kxwl_local = KXWL;
    fpkxwl_local = FPKXWL;
    for(int nword = 0;nword < N_words;++nword){
        FPGA_AES_ENC(kxwl_local,KX,fpkxwl_local);
        kxwl_local += sym_block_size;
        fpkxwl_local += sym_block_size;
    }
    kxwl_local = KXWL;
    fpkxwl_local = FPKXWL;

    tset_row_local = tset_row;

    for(int n=0;n<n_ids_tset;++n){

        idx_in_set = false;

        ::memset(G_WC,0x00,32*N_max_id_words);
        ::memset(G_FW1,0x00,32*N_max_id_words);
        ::memset(GFW_KX,0x00,32*N_max_id_words);
        ::memset(GFW_KX,0x00,32*N_max_id_words);
        ::memset(ECE,0x00,16);

        g_wc_local = G_WC;
        g_fw1_local = G_FW1;
        fpkxwl_local = FPKXWL;
        yid_local = YID_ALL;

        for(int i=0;i<NWords;++i){
            ::memcpy(g_wc_local+16,fpkxwl_local,16);
            ::memcpy(g_fw1_local+16,fw1_local,16);
            g_wc_local += 32;
            g_fw1_local += 32;
            fpkxwl_local += 16;
        }

        g_wc_local = G_WC;
        g_fw1_local = G_FW1;
        fpkxwl_local = FPKXWL;

        FPGA_ECC_MUL(G_WC,G_FW1,GFW_KX);//Assumed that NWords is less than 128

        FPGA_ECC_SCAMUL(GFW_KX,XTOKEN);//Assumed that NWords is less than 128

        ::memcpy(YID,tset_row_local,32);
        ::memcpy(ECE,tset_row_local+32,16);

        yid_local = YID_ALL;
        for(int i=0;i<NWords;++i){//This should run till row_len
            ::memcpy(yid_local,YID,32);
            yid_local += 32;
        }
        yid_local = YID_ALL;

        if(NWords == 0){
            ::memcpy(eset_local,ECE,16);
            eset_local +=16;
            ++nmatch;
        }
        else{
            //xtag computation

            FPGA_ECC_SCAMUL_BASE(YID_ALL,XTOKEN,XTAG);//Assumed that NWords is less than 128

            xtg_local = XTAG;

            for(int i=0;i<NWords;++i){//Check this length

                // std::cout << "Ami ekhane! 1.0" << std::endl;

                ::memset(bhash,0x00,bhash_block_size);
                FPGA_BLOOM_HASH(xtg_local,bhash);

                for(int j=0;j<N_HASH;++j){
                    //27 bits for 134217728
                    bf_n_indices[j][i] = (bhash[64*j] & 0xFF) + ((bhash[64*j+1] & 0xFF) << 8) + ((bhash[64*j+2] & 0xFF) << 16) + ((bhash[64*j+3] & 0x07) << 24);//Change this
                    // bf_n_indices[j][i] = (bhash[64*j] & 0xFF) + ((bhash[64*j+1] & 0xFF) << 8);//Change this
                }

                xtg_local +=32;
            }

            BloomFilter_Match_N(BF, bf_n_indices, NWords, &idx_in_set);

            if(idx_in_set){
                ::memcpy(eset_local,ECE,16);
                eset_local +=16;
                ++nmatch;
            }

            tset_row_local +=48;
            fw1_local += 16;
        }
    }

    cout << "Nmatch: " << nmatch << endl;

    unsigned char KE[16];

    ::memset(KE,0x00,16);

    AESENC(KE,Q1,KS);

    for(int i=0;i<nmatch;++i){
        AESDEC(UIDX+(16*i),ESET+(16*i),KE);
    }
    
    //auto stop_time = chrono::high_resolution_clock::now();
    //auto time_elapsed = chrono::duration_cast<chrono::microseconds>(stop_time - start_time).count();
    //cout << time_elapsed << endl;

    for(unsigned int i=0;i<N_HASH;++i){
        delete [] bf_n_indices[i];
    }
    delete [] bf_n_indices;

    delete [] stag;
    delete [] tset_row;
    delete [] WC;
    delete [] FW1;
    delete [] KXWL;
    delete [] FPKXWL;
    delete [] G_WC;
    delete [] G_FW1;
    delete [] GFW_KX;
    delete [] XTOKEN;
    delete [] XTAG;
    delete [] bhash;
    delete [] ESET;

    delete [] YID_ALL;

    return n_ids_tset;//nmatch;
}


int TSet_SetUp()
{
    unsigned char *TW;
    unsigned char *W;
    unsigned char *stag;
    unsigned char *stagi;
    unsigned char *stago;
    unsigned char *hashin;
    unsigned char *hashout;
    
    int N_words = 0;
    unsigned int N_max_id_words = 0;
    
    N_words = (N_max_ids/N_threads) + ((N_max_ids%N_threads==0)?0:1);
    N_max_id_words = N_words * N_threads;

    auto redis = Redis("tcp://127.0.0.1:6379");

    TW = new unsigned char[48*N_max_id_words];
    W = new unsigned char[16];
    stag = new unsigned char[16];
    stagi = new unsigned char[16*N_max_id_words];
    stago = new unsigned char[16*N_max_id_words];
    hashin = new unsigned char[16*N_max_id_words];
    hashout = new unsigned char[64*N_max_id_words];

    //To store TSet Value -- single execution
    unsigned char TVAL[49];
    unsigned char TBIDX[2];
    unsigned char TJIDX[2];
    unsigned char TLBL[12];

    unsigned int *FreeB;
    int bidx=0;
    int len_freeb = 65536;

    int total_count = 0;

    int freeb_idx = 0;

    FreeB = new unsigned int[len_freeb];

    ifstream eidxdb_file_handle;
    eidxdb_file_handle.open(eidxdb_file,ios_base::in|ios_base::binary);

    stringstream ss;

    string eidxdb_row;
    vector<string> eidxdb_data;
    string eidxdb_row_current;
    string s;

    ::memset(hashin,0x00,16*N_max_id_words);
    ::memset(hashout,0x00,64*N_max_id_words);
    ::memset(TJIDX,0x00,2);

    for(int bc=0;bc<len_freeb;++bc){
        FreeB[bc] = 0;
    }

    int n_rows = 0;
    int n_row_ids = 0;

    eidxdb_row.clear();
    while(getline(eidxdb_file_handle,eidxdb_row)){
        eidxdb_data.push_back(eidxdb_row);
        eidxdb_row.clear();
        n_rows++;
    }

    eidxdb_file_handle.close();

    int current_row_len = 0;

    unsigned char *tw_local = TW;
    unsigned char *stagi_local = stagi;
    unsigned char *hashin_local = hashin;
    unsigned char *hashout_local = hashout;
    
    unsigned long id_count = 0;

    std::string db_in_key = "";
    std::string db_in_val = "";

    for(int n=0;n<n_rows;++n){

        ::memset(W,0x00,16);
        ::memset(TW,0x00,48*N_max_id_words);
        ::memset(stag,0x00,16);
        ::memset(stagi,0x00,16*N_max_id_words);
        ::memset(stago,0x00,16*N_max_id_words);
        ::memset(hashin,0x00,16*N_max_id_words);
        ::memset(hashout,0x00,64*N_max_id_words);

        eidxdb_row_current = eidxdb_data.at(n);

        ss.str(std::string());
        ss << eidxdb_row_current;

        std::getline(ss,s,',');//Get the keyword
        DB_StrToHex8(W,s.data());//Read the keyword

        tw_local = TW;
        n_row_ids = 0;
        while(std::getline(ss,s,',') && !ss.eof()) {
            if(!s.empty()){
                DB_StrToHex48(tw_local,s.data());//Read the id
                tw_local += 48;
                n_row_ids++;
            }
        }

        ss.clear();
        ss.seekg(0);

        tw_local = TW;

        N_words = (n_row_ids/N_threads) + ((n_row_ids%N_threads==0)?0:1);

        AESENC(stag,W,KT);
        // PrintBuffer(stag,16);

        //Fill stagi array
        stagi_local = stagi;
        for(int nword = 0;nword < N_words;++nword){
            for(int nid=0;nid<N_threads;++nid){
                stagi_local[0] = ((nword*N_threads)+nid) & 0xFF;
                stagi_local += 16;
            }
        }
        stagi_local = stagi;

        //PRF of stag and if
        stagi_local = stagi;
        hashin_local = hashin;
        for(int nword = 0;nword < N_words;++nword){
            FPGA_AES_ENC(stagi_local,stag,hashin_local);
            stagi_local += sym_block_size;
            hashin_local += sym_block_size;
        }
        stagi_local = stagi;
        hashin_local = hashin;

        //Compute Hash
        hashin_local = hashin;
        hashout_local = hashout;
        for(int nword = 0;nword < N_words;++nword){
            FPGA_HASH(hashin_local,hashout_local);
            hashin_local += sym_block_size;
            hashout_local += hash_block_size;
        }
        hashin_local = hashin;
        hashout_local = hashout;

        //Should be done for each stag
        for(int bc=0;bc<len_freeb;++bc){
            FreeB[bc] = 0;
        }

        tw_local = TW;
        
        for(int i=0;i<n_row_ids;++i){
            ::memcpy(TVAL+1,tw_local,48);

            TVAL[0] = (i==(n_row_ids-1))?0x01:0x00;
            for(int j=0;j<49;++j){
                TVAL[j] = hashout[64*i+15+j] ^ TVAL[j];
            }
            
            ::memcpy(TBIDX,(hashout+(64*i)),2);
            ::memcpy(TLBL,(hashout+(64*i)+2),12);

            freeb_idx = (TBIDX[1] << 8) + TBIDX[0];

            bidx = (FreeB[freeb_idx]++);
            TJIDX[0] =  bidx & 0xFF;
            TJIDX[1] =  (bidx >> 8) & 0xFF;

            db_in_key.clear();
            db_in_val.clear();
            db_in_key = HexToStr(TBIDX,2) + HexToStr(TJIDX,2) + HexToStr(TLBL,12);
            db_in_val = HexToStr(TVAL,49);
            redis.set(db_in_key.data(), db_in_val.data());

            tw_local += 48;
            total_count++;
        }
    }
    
    std::cout << "Total ID Count: " << total_count << std::endl;

    delete [] TW;
    delete [] W;
    delete [] stag;
    delete [] stagi;
    delete [] stago;
    delete [] hashin;
    delete [] hashout;

    delete [] FreeB;

    return 0;
}

int TSet_GetTag(unsigned char *word, unsigned char *stag)
{
    ::memset(stag,0x00,16);
    AESENC(stag,word,KT);
    return 0;
}

int TSet_Retrieve(unsigned char *stag,unsigned char *tset_row, int *n_ids_tset)
{
    unsigned char *stagi;
    unsigned char *stago;
    unsigned char *hashin;
    unsigned char *hashout;
    unsigned char *TV;
    
    int N_words = 0;
    unsigned int N_max_id_words = 0;
    
    N_words = (N_max_ids/N_threads) + ((N_max_ids%N_threads==0)?0:1);
    N_max_id_words = N_words * N_threads;

    stagi = new unsigned char[16*N_max_id_words];
    stago = new unsigned char[16*N_max_id_words];
    hashin = new unsigned char[16*N_max_id_words];
    hashout = new unsigned char[64*N_max_id_words];
    TV = new unsigned char[48*N_max_id_words];

    unsigned char TENTRY[61];
    unsigned char TVAL[49];
    unsigned char TBIDX[2];
    unsigned char TJIDX[2];
    unsigned char TLBL[12];
    unsigned char HLBL[12];


    unsigned int *FreeB;
    int bidx=0;
    int len_freeb = 65536;
    int freeb_idx = 0;
    bool BETA = 0;

    unsigned char *stagi_local;
    unsigned char *stago_local;
    unsigned char *hashin_local;
    unsigned char *hashout_local;

    unsigned char * TV_curr;

    unsigned char *T_RES;
    unsigned char *T_BIDX;
    unsigned char *T_JIDX;
    unsigned char *T_LBL;

    unsigned char *local_t_res;
    unsigned char *local_t_bidx;
    unsigned char *local_t_jidx;
    unsigned char *local_t_lbl;
 
    unsigned char *local_t_res_word;
    unsigned char *local_t_bidx_word;
    unsigned char *local_t_jidx_word;
    unsigned char *local_t_lbl_word;
    unsigned char *local_hashout_word;

    T_RES = new unsigned char[49*N_max_id_words];
    T_BIDX = new unsigned char[2*N_max_id_words];
    T_JIDX = new unsigned char[2*N_max_id_words];
    T_LBL = new unsigned char[12*N_max_id_words];

    int rcnt = 0;

    ::memset(stagi,0x00,16*N_max_id_words);
    ::memset(stago,0x00,16*N_max_id_words);
    ::memset(hashin,0x00,16*N_max_id_words);
    ::memset(hashout,0x00,64*N_max_id_words);
    ::memset(TV,0x00,48*N_max_id_words);

    ::memset(TVAL,0x00,49);
    ::memset(TJIDX,0x00,2);

    FreeB = new unsigned int[len_freeb];

    for(int bc=0;bc<len_freeb;++bc){
        FreeB[bc] = 0;
    }

    //Fill stagi array
    stagi_local = stagi;
    for(int nword = 0;nword < N_words;++nword){
        for(int nid=0;nid<N_threads;++nid){
            stagi_local[0] = ((nword*N_threads)+nid) & 0xFF;
            stagi_local += 16;
        }
    }
    stagi_local = stagi;

    //PRF of stag and if
    stagi_local = stagi;
    hashin_local = hashin;
    for(int nword = 0;nword < N_words;++nword){
        FPGA_AES_ENC(stagi_local,stag,hashin_local);
        stagi_local += sym_block_size;
        hashin_local += sym_block_size;
    }
    stagi_local = stagi;
    hashin_local = hashin;

    //Compute Hash
    hashin_local = hashin;
    hashout_local = hashout;
    for(int nword = 0;nword < N_words;++nword){
        FPGA_HASH(hashin_local,hashout_local);
        hashin_local += sym_block_size;
        hashout_local += hash_block_size;
    }
    hashin_local = hashin;
    hashout_local = hashout;

    TV_curr = TV;

    ::memset(T_RES,0x00,49*N_max_id_words);
    ::memset(T_BIDX,0x00,2*N_max_id_words);
    ::memset(T_JIDX,0x00,2*N_max_id_words);
    ::memset(T_LBL,0x00,12*N_max_id_words);

    local_t_res = T_RES;
    local_t_bidx = T_BIDX;
    local_t_jidx = T_JIDX;
    local_t_lbl = T_LBL;
    
    while(!BETA){

      local_hashout_word = hashout_local;
      
      local_t_res_word = local_t_res;
      local_t_bidx_word = local_t_bidx;
      local_t_jidx_word = local_t_jidx;
      local_t_lbl_word = local_t_lbl;

      for(unsigned int ni=0;ni<N_threads;++ni){
          ::memcpy(local_t_bidx,hashout_local,2);

          freeb_idx = ((local_t_bidx[1] << 8) + local_t_bidx[0]);

          bidx = (FreeB[freeb_idx]++);
          local_t_jidx[0] =  bidx & 0xFF;
          local_t_jidx[1] =  (bidx >> 8) & 0xFF;

          ::memcpy(local_t_lbl,hashout_local+2,12);
          
          local_t_bidx += 2;
          local_t_jidx += 2;
          local_t_lbl += 12;
          hashout_local +=64;
      }
      
      MGDB_QUERY(local_t_res,local_t_bidx_word,local_t_jidx_word,local_t_lbl_word);
      
      for(unsigned int ni=0;ni<N_threads;++ni){
          ::memcpy(TVAL,local_t_res,49);
          BETA = TVAL[0] ^ local_hashout_word[15];

          for(int i=0;i<48;++i){
              TV_curr[i] = local_hashout_word[16+i] ^ TVAL[i+1];
          }

          rcnt++;
          if(BETA == 0x01) break;

          TV_curr += 48;
          local_t_res += 49;

          local_hashout_word += 64;
      }
    }
    
    *n_ids_tset = rcnt;

    ::memcpy(tset_row,TV,48*rcnt);

    delete [] stagi;
    delete [] stago;
    delete [] hashin;
    delete [] hashout;
    delete [] TV;

    delete [] FreeB;

    delete [] T_RES;
    delete [] T_BIDX;
    delete [] T_JIDX;
    delete [] T_LBL;

    return 0;
}

////////////////////////////////////////////////////////////////////////////////

int FPGA_AES_ENC(unsigned char *ptext,unsigned char *key, unsigned char *ctext)
{
    {
        std::lock_guard<std::mutex> lock(mrun);
        ::memset(GL_AES_CT,0x00,sym_block_size);
        ::memcpy(GL_AES_PT,ptext,sym_block_size);
        ::memcpy(GL_AES_KT,key,16);

        *GL_OPCODE = 1;

        nWorkerCount = N_threads;
        ++nCurrentIteration;
    }
    dataReady.notify_all();

    {
        std::unique_lock<std::mutex> lock(mrun);
        workComplete.wait(lock, [] { return nWorkerCount == 0; });
    }

    *GL_OPCODE = 0;

    ::memcpy(ctext,GL_AES_CT,sym_block_size);

    return 0;
}

int FPGA_HASH(unsigned char *msg, unsigned char *digest)
{

    {
        std::lock_guard<std::mutex> lock(mrun);
        ::memset(GL_HASH_DGST,0x00,hash_block_size);
        ::memcpy(GL_HASH_MSG,msg,sym_block_size);

        *GL_OPCODE = 2;

        nWorkerCount = N_threads;
        ++nCurrentIteration;
    }
    dataReady.notify_all();

    {
        std::unique_lock<std::mutex> lock(mrun);
        workComplete.wait(lock, [] { return nWorkerCount == 0; });
    }

    *GL_OPCODE = 0;

    ::memcpy(digest,GL_HASH_DGST,hash_block_size);

    return 0;
}

int FPGA_BLOOM_HASH(unsigned char *msg, unsigned char *digest)
{

    {
        std::lock_guard<std::mutex> lock(mrun);
        ::memset(GL_BLM_MSG,0x00,bhash_in_block_size);
        ::memset(GL_BLM_DGST,0x00,hash_block_size);
        for(int i=0;i<N_HASH;i++){ //keep this operation here
            ::memcpy(GL_BLM_MSG+(40*i),msg,32);
            GL_BLM_MSG[40*i+39] = (i & 0xFF);
        }

        *GL_OPCODE = 3;

        nWorkerCount = N_threads;
        ++nCurrentIteration;
    }
    dataReady.notify_all();

    {
        std::unique_lock<std::mutex> lock(mrun);
        workComplete.wait(lock, [] { return nWorkerCount == 0; });
    }

    *GL_OPCODE = 0;

    ::memcpy(digest,GL_BLM_DGST,hash_block_size);

    return 0;
}

int FPGA_ECC_FPINV(unsigned char *fp_x, unsigned char *fp_invx)
{

    {
        std::lock_guard<std::mutex> lock(mrun);
        ::memset(GL_ECC_INVP,0x00,ecc_block_size);
        ::memcpy(GL_ECC_INVA,fp_x,ecc_block_size);

        *GL_OPCODE = 5;

        nWorkerCount = N_threads;
        ++nCurrentIteration;
    }
    dataReady.notify_all();

    {
        std::unique_lock<std::mutex> lock(mrun);
        workComplete.wait(lock, [] { return nWorkerCount == 0; });
    }

    *GL_OPCODE = 0;

    ::memcpy(fp_invx,GL_ECC_INVP,ecc_block_size);

    return 0;
}

int FPGA_ECC_MUL(unsigned char *in_A,unsigned char *in_B,unsigned char *prod)
{

      {
          std::lock_guard<std::mutex> lock(mrun);
          ::memset(GL_ECC_PRD,0x00,ecc_block_size);
          ::memcpy(GL_ECC_INA,in_A,ecc_block_size);
          ::memcpy(GL_ECC_INB,in_B,ecc_block_size);

          *GL_OPCODE = 4;

          nWorkerCount = N_threads;
          ++nCurrentIteration;
      }
      dataReady.notify_all();

      {
          std::unique_lock<std::mutex> lock(mrun);
          workComplete.wait(lock, [] { return nWorkerCount == 0; });
      }

      *GL_OPCODE = 0;

      ::memcpy(prod,GL_ECC_PRD,ecc_block_size);

      return 0;
}

int FPGA_ECC_SCAMUL(unsigned char *sca, unsigned char *prod)
{

    {
        std::lock_guard<std::mutex> lock(mrun);
        ::memset(GL_ECC_SMP,0x00,ecc_block_size);
        ::memcpy(GL_ECC_SCA,sca,ecc_block_size);

        *GL_OPCODE = 6;

        nWorkerCount = N_threads;
        ++nCurrentIteration;
    }
    dataReady.notify_all();

    {
        std::unique_lock<std::mutex> lock(mrun);
        workComplete.wait(lock, [] { return nWorkerCount == 0; });
    }

    *GL_OPCODE = 0;

    ::memcpy(prod,GL_ECC_SMP,ecc_block_size);

    return 0;
}

int FPGA_ECC_SCAMUL_BASE(unsigned char *sca, unsigned char *basep, unsigned char *prod)
{

    {
        std::lock_guard<std::mutex> lock(mrun);
        ::memset(GL_ECC_SMP,0x00,ecc_block_size);
        ::memcpy(GL_ECC_SCA,sca,ecc_block_size);
        ::memcpy(GL_ECC_BP,basep,ecc_block_size);

        *GL_OPCODE = 7;

        nWorkerCount = N_threads;
        ++nCurrentIteration;
    }
    dataReady.notify_all();

    {
        std::unique_lock<std::mutex> lock(mrun);
        workComplete.wait(lock, [] { return nWorkerCount == 0; });
    }

    *GL_OPCODE = 0;

    ::memcpy(prod,GL_ECC_SMP,ecc_block_size);

    return 0;
}

////////////////////////////////////////////////////////////////////////////////

int MGDB_QUERY(unsigned char *RES, unsigned char *BIDX, unsigned char *JIDX, unsigned char *LBL)
{
    {
        std::lock_guard<std::mutex> lock(mrun);
        ::memcpy(GL_MGDB_BIDX,BIDX,(N_threads * 2));
        ::memcpy(GL_MGDB_JIDX,JIDX,(N_threads * 2));
        ::memcpy(GL_MGDB_LBL,LBL,(N_threads * 12));
        ::memset(GL_MGDB_RES,0x00,(N_threads * 49));
        
//        for(unsigned int i=0;i<N_threads;++i){
//            std::cout << MDB_HexToStr(LBL+(12*i),12) << std::endl;
//        }

        *GL_OPCODE = 8;

        nWorkerCount = N_threads;
        ++nCurrentIteration;
    }
    dataReady.notify_all();

    {
        std::unique_lock<std::mutex> lock(mrun);
        workComplete.wait(lock, [] { return nWorkerCount == 0; });
    }

    *GL_OPCODE = 0;

    ::memcpy(RES,GL_MGDB_RES,(N_threads*49));

    return 0;
}

////////////////////////////////////////////////////////////////////////////////

int SHA3_HASH(blake3_hasher *hasher,unsigned char *msg, unsigned char *digest)
{
    Blake3(hasher,digest,msg);
    return 0;
}

int SHA3_HASH_K(blake3_hasher *hasher,unsigned char *msg, unsigned char *digest)
{
    Blake3_K(hasher,digest,msg);
    return 0;
}

int ECC_FPINV(unsigned char *fp_x, unsigned char *fp_invx)
{
    mpz_class a, b;
    size_t ss = 32;
    mpz_import(a.get_mpz_t(),32,1,1,0,0,fp_x);
    mpz_powm(b.get_mpz_t(),a.get_mpz_t(),InvExp.get_mpz_t(),Prime.get_mpz_t());
    StrToHex(fp_invx,b.get_str(16));
    return 0;
}

int ECC_MUL(unsigned char *in_A,unsigned char *in_B,unsigned char *prod)
{
    mpz_class a, b, c, r;
    size_t ss = 32;

    mpz_import(a.get_mpz_t(),32,1,1,0,0,in_A);
    mpz_import(b.get_mpz_t(),32,1,1,0,0,in_B);
    r = a * b;
    mpz_mod(c.get_mpz_t(),r.get_mpz_t(),Prime.get_mpz_t());
    StrToHex(prod,c.get_str(16));
    return 0;
}

////////////////////////////////////////////////////////////////////////////////

std::string HexToStr(unsigned char *hexarr, int len)
{
    std::stringstream ss;
    ss << std::hex;

    for (int i = 0; i < len; ++i)
        ss << std::setw(2) << std::setfill('0') << static_cast<unsigned int>(hexarr[i]);
    return ss.str();
}

std::string NumToHexStr(int num){
    std::stringstream ss;
    ss << std::hex;

    ss << std::setw(2) << std::setfill('0') << static_cast<unsigned int>(num & 0xFF);
    return ss.str();
}

int StrToHex(unsigned char *hexarr,string numin)
{
    std::string dest = std::string( 64-numin.length(), '0').append( numin);
    const char *text = dest.data();
    char temp[2];
    for (int j=0; j<32; j++)
    {
        temp[0] = text[2*j];
        temp[1] = text[2*j+1];
        hexarr[j] = ::strtoul(temp,nullptr,16) & 0xFF;
    }
    return 0;
}

int StrToHex(unsigned char *hexarr, std::string str_in, unsigned int n_bytes)
{
    const char *text = str_in.data();
    char temp[2];
    for (int j=0; j<n_bytes; j++)
    {
        temp[0] = text[2*j];
        temp[1] = text[2*j+1];
        hexarr[j] = ::strtoul(temp,nullptr,16) & 0xFF;
    }
    return 0;
}

int StrToHexBVec(unsigned char *hexarr,string bvec)
{
    const char *text = bvec.data();
    char temp[2];
    for (int j=0; j<4; j++)
    {
        temp[0] = text[2*j];
        temp[1] = text[2*j+1];
        hexarr[j] = ::strtoul(temp,nullptr,16) & 0xFF;
    }
    return 0;
}

int PrintBuffer(unsigned char *dat,unsigned int n)
{
    std::cout << std::hex;
    for(unsigned int i = 0; i < n; ++i){
        std::cout << std::uppercase << std::setw(2) << std::setfill('0') << static_cast<unsigned int>(dat[i]);
    }
    std::cout << std::endl;
    return 0;
}