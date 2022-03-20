#pragma once
#include <vector> 
#include <deque>
#include <string>


#include "search_server.h"
#include "document.h"

class RequestQueue {
public:
    explicit RequestQueue(const SearchServer& search_server);

    template <typename DocumentPredicate>
    std::vector<Document> AddFindRequest(const std::string& raw_query, DocumentPredicate document_predicate);

    std::vector<Document> AddFindRequest(const std::string& raw_query, DocumentStatus status = DocumentStatus::ACTUAL);

    int GetNoResultRequests() const;
    
private:
    struct QueryResult {
        QueryResult(std::vector<Document> request);
        std::vector<Document> results_;
    };   
    
    std::deque<QueryResult> requests_;
    const static int min_in_day_ = 1440;
    const SearchServer& queue_search_server;
    void MoveDeque(std::vector<Document> requests); 
    /* ѕо ссылке не получаетс€, FindTopDocuments возвращает вектор*/

};

template <typename DocumentPredicate>
std::vector<Document> RequestQueue::AddFindRequest(const std::string& raw_query, DocumentPredicate document_predicate) {
    MoveDeque(queue_search_server.FindTopDocuments(raw_query, document_predicate));
    return requests_.back().results_;
}