#include "process_queries.h"

#include <algorithm>
#include <execution>

using namespace std;

std::vector<std::vector<Document>> ProcessQueries(
    const SearchServer& search_server,
    const std::vector<std::string>& queries) {

    std::vector<std::vector<Document>> res(queries.size());

    std::transform(std::execution::par, queries.cbegin(), queries.cend(), res.begin(), [&search_server](const std::string& query)
        { return move(search_server.FindTopDocuments(query)); });

    return res;
}

std::list<Document> ProcessQueriesJoined(
    const SearchServer& search_server,
    const std::vector<std::string>& queries) {

    std::list<Document> result;

    for (std::vector<Document>& vect : ProcessQueries(search_server, queries)) {
        for (Document& g : vect) {
            result.push_back(g);
        }
    }
    return result;

}