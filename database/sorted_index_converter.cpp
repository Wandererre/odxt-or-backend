#include "../utils.h"


/*
    BEWARE: Carriage Return Characters (ASCII 13 or '\r') are present in the file
    This is due to the inherent nature of the file
    I manually avoided adding the string that only consists of this character
    Thats why I ignored the last element of the row
*/


int main(){

    string filename = "./OG/sorted_index_100k.csv";
    int count = 10;


    auto content = read_file(filename);
    cout << "Content size: " << content.size() << "\n";
    // the content is like <kw, hash-id of mkw>
    
    vector<vector<string>> ans;

    map<string, int> mp;
    for(auto row:content){
        if(count){
            cout << "------------\n" << row.size() << "\n";
            for(auto s:row){
                cout << "\t";
                for(auto c:s){
                    cout << "{" << (int)c << "}";
                }
                cout << "\n";
            }
            count--;
        }
        ans.push_back({});
        
        ans.back().push_back(row[0]);
        if(count) cout << "\t" << 0 << " " << row[0] << "\n";
        for(int j=1; j+1<row.size(); j++){
            if(mp.find(row[j])==mp.end()){
                mp[row[j]] = mp.size()+1;
            }
            if(count) cout << "\t" << j << " " << row[j] << "\n";
            ans.back().push_back(intToStr(mp[row[j]]));
        }
        if(count) cout << "\t" << ans.back().size() << "\n";
    }


    write_file("db.dat", ans);
}