#include "string_processing.h"

using namespace std;

vector<string_view> SplitIntoWords(string_view str) {
    vector<string_view> result;
    int64_t pos = 0;
    const int64_t pos_end = str.npos;
    while (true) {
        int64_t space = str.find(' ', pos);
        result.push_back(space == pos_end ? str.substr(pos) : str.substr(pos, space - pos));
        if (space == pos_end) {
            break;
        }
        else {
            str.remove_prefix(space + 1);
        }
    }
    return result;
}