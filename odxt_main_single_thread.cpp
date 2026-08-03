#include "odxt_main_single_thread.h"

auto redis = Redis("tcp://127.0.0.1:6379");

int Redis_Init(){
    // select database 7
    redis.command("SELECT", "7");
    return 0;
}

int Sys_Init()
{
    return 0;
}

int Sys_Clear()
{
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
    //Clear XSet -  do manually, clear redis

    return 0;
}

int ODXT_Update(std::string keyword, std::string kw, std::string id, int cnt, unsigned char op)
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
    // if(update_count.count(keyword) == 0){
    //     update_count.insert(std::make_pair(keyword,0));
    // }

    // auto cnt = (update_count[keyword]++);

    //Copy keyword
    StrToHex(keyword_chars, keyword, 4);
    StrToHex(keyword_chars_all, keyword, 4); // keyword should be stored in 4 bytes only even though 16 bytes is available

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
    PRF_K(addr,addr_input,KT);

    //Append indicator variable, only the last byte changes for val coumputation
    addr_input[15] = 0x01;

    //Compute PRF, key is inbuilt
    PRF_K(val_input,addr_input,KT);

    //Pre-load val with id and op
    ::memset(val,0x00,16);
    ::memcpy(val,id_chars,4);
    val[4] = op;

    //Xor id||op and PRF output
    for(unsigned int i=0;i<16;++i){
        val[i] = val[i] ^ val_input[i];
    }

    //Clear the indicator variable, ideally this is not present
    addr_input[15] = 0x00;
    
    // Couple the kw with the meta-keyword
    unsigned char kw_chars[4];
    ::memset(kw_chars,0x00,4);
    StrToHex(kw_chars, kw, 4);
    ::memcpy(addr_input+4, kw_chars, 4);

    //Compute PRF using Kz
    ::memset(alpha_inv_in,0x00,32);
    PRF_K(alpha_inv_in,addr_input,KZ);

    //Compute inverse
    ::memset(alpha_inv_out,0x00,32);
    ECC_FPINV(alpha_inv_in, alpha_inv_out);

    //Pre-load alpha_prf_in with id and op
    ::memset(alpha_prf_in,0x00,16);
    ::memcpy(alpha_prf_in,id_chars,4);
    alpha_prf_in[4] = op;

    //Compute PRF
    ::memset(alpha_prf,0x00,32);
    PRF_K(alpha_prf,alpha_prf_in,KY);

    //Multiply
    ::memset(alpha,0x00,32);
    ECC_MUL(alpha_prf,alpha_inv_out,alpha);

    //Compute F(Kx,w)
    // append kw to meta-keyword
    ::memcpy(keyword_chars_all+4, kw_chars, 4);
    ::memset(xtag_exp_1,0x00,32);
    PRF_K(xtag_exp_1,keyword_chars_all,KX);

    //Multiply
    ::memset(xtag_exp,0x00,32);
    ECC_MUL(xtag_exp_1,alpha_prf,xtag_exp);

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

/**
    @brief Computes the stokens and xtokens required for the search operation. Part of the client side operation before server search.
*/
int Client_Pre_Thread(const int start, const int step, const vector<string>& query, const unsigned int update_cnt, const vector<vector<int>>& kws, vector<string>& stokenList, vector<vector<vector<string>>>& xtokenList){
    
    unsigned char saddrj_in[16];
    unsigned char saddrj[16];

    unsigned char xtoken_w1_prf_in[16];
    unsigned char xtoken_w1_prf_out[32];

    unsigned char xtoken_wi_prf_in[16];
    unsigned char xtoken_wi_prf_out[32];

    unsigned char xtoken_exp[32];
    unsigned char xtoken_j[32];

    auto w1 = query.at(0);
    //Number of crossterms
    auto n = query.size()-1;//Remeber! Number of cross terms, not all query keywords!

    //Stokentlist
    // std::vector<std::string> stokenList;
    // std::vector<std::vector<std::vector<string>>> xtokenList;

    for(unsigned int j=start; j<update_cnt; j+=step){
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
        PRF_K(saddrj,saddrj_in, KT);

        //Insert into stokenList
        stokenList[j] = (HexToStr(saddrj,16));

        //Create xtokenlist container
        std::vector<std::vector<std::string>> xtokenList_j;

        for(unsigned int i=0;i<n;++i){
            std::vector<std::string> xtokenList_j_i;
            for(auto w0:kws[0]){
                for(auto w1:kws[i+1]){
                    //F(Kz,w1||j)
                    ::memset(xtoken_w1_prf_out,0x00,32);
                    // append w0 to saddrj_in
                    StrToHex(saddrj_in+4, intToStr(w0), 4);
                    PRF_K(xtoken_w1_prf_out,saddrj_in,KZ);

                    //F(Kx,wi)
                    ::memset(xtoken_wi_prf_out,0x00,32);
                    ::memset(xtoken_wi_prf_in,0x00,16);
                    //Copy wi
                    StrToHex(xtoken_wi_prf_in,query.at(i+1),4);
                    // append w1
                    StrToHex(xtoken_wi_prf_in+4, intToStr(w1), 4);
                    PRF_K(xtoken_wi_prf_out,xtoken_wi_prf_in,KX);

                    //Multiply
                    ::memset(xtoken_exp,0x00,32);
                    ECC_MUL(xtoken_w1_prf_out,xtoken_wi_prf_out,xtoken_exp);

                    //Scalar multiplication
                    ::memset(xtoken_j,0x00,32);
                    ScalarMul(xtoken_j,xtoken_exp,ecc_basep);
                    
                    xtokenList_j_i.push_back(HexToStr(xtoken_j,32));
                }
            }
            shuffle(xtokenList_j_i.begin(),xtokenList_j_i.end(),default_random_engine(rand()));

            //Insert into container
            xtokenList_j.push_back(xtokenList_j_i);
        }
        shuffle(xtokenList_j.begin(),xtokenList_j.end(),default_random_engine(rand()));
        //Insert xtokenlist_j into container
        xtokenList[j] = (xtokenList_j);
    }
    return 0;
}

/**
    @brief Server side search operation. Performes the search for ith stoken and corresponding xtokenlist where i mod step == start.
*/
int Server_Search_Thread(const int start, const int step, const vector<string>& stokenList, const vector<vector<vector<string>>>& xtokenList, vector<pair<string,pair<unsigned int, unsigned int>>>& sEOpList){
    
    unsigned int cnt_j = 0;
    unsigned char sval_alpha[48];
    unsigned char sval[16];
    unsigned char alpha[32];
    unsigned char xtoken[32];
    unsigned char xtag[32];

    //Get stoken list size
    auto stokelist_size = stokenList.size();

    for(unsigned int j=start; j<stokelist_size; j+=step){
        cnt_j = 0;

        ::memset(sval_alpha,0x00,48);
        ::memset(sval,0x00,16);
        ::memset(alpha,0x00,32);

        //TSet retrieval
        string s = stokenList[j];
        auto val = redis.get(s);
        StrToHex(sval_alpha,std::string(val->data()),48);
        ::memcpy(sval,sval_alpha,16);
        ::memcpy(alpha,sval_alpha+16,32);//Need to copy only 16 bytes, rest are zero

        //Get the xtokenlist_j
        auto xtokenlist_j = xtokenList[j];
        for(unsigned int i=0; i<xtokenlist_j.size(); ++i){
            //Retrieve xtoken
            for(auto xtoken_str:xtokenlist_j[i]){
                ::memset(xtag,0x00,32);
                ::memset(xtoken,0x00,32);
                StrToHex(xtoken,xtoken_str,32);
                ScalarMul(xtag,alpha,xtoken);

                //XSet retrieval
                string xtag_str = HexToStr(xtag,32);
                auto val = redis.exists(xtag_str);
                
                //If val exists, the increment count
                if(val){
                    cnt_j++;
                    break;
                }
            }
        }
        //Insert into sEOpList (j,sval,cnt_j) == (sval,(j,cnt_j))
        sEOpList[j] = (std::make_pair(HexToStr(sval,16),std::make_pair(j,cnt_j)));//Send this to client
    }
    return 0;
}



int ODXT_Search(std::unordered_set<std::string> *IdList, std::vector<std::string> query, int update_cnt)
{
    //////////////////////////////////////////////////////////////
    //Client
    //////////////////////////////////////////////////////////////

    vector<int> mkws;
    vector<vector<int>> kws;
    for(auto s:query){
        mkws.push_back(strToInt(s));
        kws.push_back(mdb->find_kw(mkws.back()));
    }

    

    //Stokentlist
    std::vector<std::string> stokenList(update_cnt);
    std::vector<std::vector<std::vector<string>>> xtokenList(update_cnt);

    int pre_thread_count = min(update_cnt, (int)std::thread::hardware_concurrency());
    std::vector<std::future<int>> pre_threads;
    for(int i=0; i<pre_thread_count; ++i){
        pre_threads.push_back(async(Client_Pre_Thread, i, pre_thread_count, std::ref(query), update_cnt, std::ref(kws), std::ref(stokenList), std::ref(xtokenList)));
    }
    for(auto& t:pre_threads){
        t.get();
    }

    //////////////////////////////////////////////////////////////
    //Server
    //////////////////////////////////////////////////////////////

    std::vector<std::pair<std::string,std::pair<unsigned int, unsigned int>>> sEOpList(stokenList.size());

    int server_thread_count = min((unsigned int)stokenList.size(), std::thread::hardware_concurrency());
    std::vector<std::future<int>> server_threads;
    for(int i=0; i<server_thread_count; ++i){
        server_threads.push_back(async(Server_Search_Thread, i, server_thread_count, std::ref(stokenList), std::ref(xtokenList), std::ref(sEOpList)));
    }
    for(auto& t:server_threads){
        t.get();
    }
    

    // //////////////////////////////////////////////////////////////
    // //Client
    // //////////////////////////////////////////////////////////////
    // // std::unordered_set<std::string> IdList;

    // // For now lets leave the client part here, we will parallelize it later

    auto w1 = query.at(0);
    auto n = query.size()-1;

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
        PRF_K(prf_kt_out,prf_kt_in,KT);

        for(unsigned int k=0;k<16;++k){
            id_op[k] = sval_hex[k] ^ prf_kt_out[k];
        }

        if((id_op[4] == 0x01) && (cnt_j == n)){
            IdList->insert(HexToStr(id_op,4));
        }
        else if((id_op[4] == 0x00) && (cnt_j > 0)){
            IdList->erase(HexToStr(id_op,4));
        }
    }

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
    NumStrToHex(fp_invx,b.get_str(16));
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
    NumStrToHex(prod,c.get_str(16));
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

int NumStrToHex(unsigned char *hexarr,string numin)
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
    char temp[3];
    for (int j=0; j<n_bytes; j++)
    {
        temp[0] = text[2*j];
        temp[1] = text[2*j+1];
        temp[2] = '\0';
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