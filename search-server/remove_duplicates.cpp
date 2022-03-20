#include "remove_duplicates.h"

#include <iostream>
#include <map>



void RemoveDuplicates(SearchServer& search_server) {
    using namespace std;
    vector <int> RemoveId;
    set <set <string_view> > documents;
    
    for (const auto& id: search_server) {
        set<string_view> words;
        for (const auto& [word, freqs] : search_server.GetWordFrequencies(id)) {
            words.insert(word);
        }
        if (documents.count(words) != 0) {
            RemoveId.push_back(id);
        }
        documents.insert(words);
    }
    for (auto id : RemoveId) {
        cout << "Found duplicate document id N " << id << endl;
        search_server.RemoveDocument(id);
    }

}
