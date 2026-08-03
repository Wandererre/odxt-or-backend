#include "utils.h"


vector<vector<string>> read_file(string filename){
    vector<vector<string>> ans;
    ifstream file(filename);
    string line;
    while(getline(file, line, '\n')){
        ans.push_back({});
        stringstream ss(line);
        string s;
        while(getline(ss, s, ',')){
            ans.back().push_back(s);
        }
    }
    file.close();
    return move(ans);
}




long double average(string filename){
    auto content = read_file(filename);
    long double sum = 0;
    for(auto& row:content){
        sum += stold(row[0]);
    }
    return sum/content.size();
}


string intToStr(int num){
    stringstream ss;
    ss << hex;
    ss << setw(8) << setfill('0') << num;
    return ss.str();
}


int strToInt(string s){
    return stoi(s, nullptr, 16);
}

bool isPowerOfTwo(int n){
    return (n & (n-1)) == 0;
}

void FMap::insert(const string& s){
    mp[s]++;
}

void FMap::insert(const vector<string>& vec){
    for(auto& s:vec){
        insert(s);
    }
}

void FMap::erase(const string& s){
    if(mp.find(s) == mp.end()){
        return;
    }
    mp[s]--;
    if(mp[s] == 0){
        mp.erase(s);
    }
}

void FMap::erase(const vector<string>& vec){
    for(auto& s:vec){
        erase(s);
    }
}

vector<string> FMap::get(int limit){
    vector<string> ans;
    for(auto& p:mp){
        if(p.second >= limit){
            ans.push_back(p.first);
        }
    }
    return move(ans);
}


MKW_Converter::MKW_Converter(int bucketSize, bool isOptimized):bucketSize(bucketSize), isOptimized(isOptimized){
    kws.resize(bucketSize);
    for(int i=0; i<bucketSize; i++){
        for(int j=i; j<bucketSize; j++){
            if(j-i+1 == bucketSize) continue;
            if(isOptimized && !isPowerOfTwo(j-i+1)) continue;
            mkws.push_back({});
            int mkw_num = mkws.size() - 1;
            mp[{i, j}] = mkw_num;
            for(int k=0; k<bucketSize; k++){
                if(i<=k && k<=j) continue;
                kws[k].push_back(mkw_num);
                mkws[mkw_num].push_back(k);
            }
        }
    }
}


/**
    @brief Breaks the meta-keyword as Opt-Log-TWINSSE
    @param range (i,j) indicating mkw(i,j)
    @return Broken down meta-keywords (inform of {i,j})
 */
vector<pair<int,int>> MKW_Converter::break_mkw_range(pair<int,int> range){
    int i = range.first, j = range.second;
    if(!isOptimized || isPowerOfTwo(j-i+1)) return {range};
    vector<pair<int,int>> res;
    int len = j-i+1;

    // find largest power of 2 that is less than len
    int p = -1;
    while((1<<(p+1)) < len) p++;
    
    int largest_len = 1<<p;
    return {{i, i+largest_len-1}, {j-largest_len+1, j}};
}


/**
   @brief Returns keywords that are covered by the given meta-keyword
   @param mkw Chosen meta-keyword
   @return Vector of keywords
*/
vector<int> MKW_Converter::find_kw(int mkw){
    int bucket = mkw / mkws.size();
    mkw = mkw % mkws.size();
    vector<int> res;
    for(auto x:mkws[mkw]){
        res.push_back(bucket*kws.size() + x);
    }
    return std::move(res);
}


/**
   @brief Returns meta-keywords that cover the given keyword
   @param kw Chosen keyword
   @return Vector of meta-keywords
*/
vector<int> MKW_Converter::find_mkw(int kw){
    int bucket = kw / bucketSize;
    kw = kw % bucketSize;
    vector<int> res;
    for(auto x:kws[kw]){
        res.push_back(bucket*mkws.size() + x);
    }
    return std::move(res);
}


/**
 @brief Converts a given disjunctive keyword query into a conjunctive meta-keyword query
 @param query Vector of keywords
 @return Vector of meta-keywords
 
 Requires: All the keywords in the query are distinct but belong to the same bucket 
 */
vector<int> MKW_Converter::convert_query(vector<int> query){
    if(query.size()==0) return {};
    sort(query.begin(), query.end());
    int bucket = query[0] / kws.size();
    for(auto& x:query){
        x = x % kws.size();
    }
    vector<int> res;
    int prev = -1; // upto where you have already left
    query.push_back(kws.size());
    for(auto x:query){
        // leave from prev+1 to curr-1
        int i = prev + 1;
        int j = x - 1;
        if(j-i+1 <= 0 || j-i+1 == kws.size()){}
        else{
            auto ranges = break_mkw_range({i, j});
            for(auto r:ranges){
                res.push_back(bucket*mkws.size() + mp[r]);
            }
        }
        prev = x;
    }
    return std::move(res);
}

/**
    @brief Group together the keywords in the query that belong to the same bucket
    @param query Vector of keywords
    @return keywords grouped by bucket
*/
vector<vector<int>> MKW_Converter::bucketize_query(vector<int> query){
    sort(query.begin(), query.end());
    vector<vector<int>> res;
    for(int start=0, end=0; end<query.size(); start=end){
        res.push_back({});
        while(end<query.size() && query[end]/bucketSize == query[start]/bucketSize){
            res.back().push_back(query[end]);
            end++;
        }
    }
    return std::move(res);
}







