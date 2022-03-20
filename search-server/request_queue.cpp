#include "request_queue.h" 

using namespace std;

RequestQueue::RequestQueue(const SearchServer& search_server)
    :queue_search_server(search_server) {}

vector<Document> RequestQueue::AddFindRequest(const string& raw_query, DocumentStatus status) {
    MoveDeque(queue_search_server.FindTopDocuments(raw_query, status));
    return requests_.back().results_;
}

int RequestQueue::GetNoResultRequests() const {
    int result = 0;
    for (auto it : requests_) {
        if (it.results_.empty()) {
            ++result;
        }
    }
    return result;
}

void RequestQueue::MoveDeque(vector<Document> requests) {
    if (requests_.size() == min_in_day_) {
        requests_.pop_front();
    }
    requests_.push_back(requests);
}
/* При передаче вектора по ссылке не работает 9ая строчка кода, потому что FindTopDocuments возвращает вектор.*/
RequestQueue::QueryResult::QueryResult(vector<Document> request) 
    :results_(request) {}