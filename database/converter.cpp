#include "../utils.h"

#ifdef SHUFFLE
    bool toShuffle = true;
#endif
#ifndef SHUFFLE
    bool toShuffle = false;
#endif


/*
    Reads the file identifiers from the content
    stores them in the form of pairs <idx, file_id
*/
vector<vector<string>> strip(const vector<vector<string>>& content){
    vector<vector<string>> ans;
    for(int i=0; i<content.size(); i++){
        for(auto x:content[i]){
            ans.push_back({intToStr(i), intToStr(strToInt(x))});
        }
    }
    return ans;
}


int main(){
    // reads from the db6k.dat file

    auto content = read_file("db.dat");
    
    // erase the keyword part
    for(auto& row:content){
        row.erase(row.begin());
    }

    if(toShuffle){
        shuffle(content.begin(), content.end(), default_random_engine(0));
    }

    auto updates = strip(content);

    // lets shuffle the updates
    shuffle(updates.begin(), updates.end(), default_random_engine(0));

    write_file("db_updates.csv", updates);


    return 0;
}