#include "search_server.h"

using namespace std;

    SearchServer::SearchServer(const string& stop_words_text)
        : SearchServer(SplitIntoWords(stop_words_text)) 
    {}

    void SearchServer::AddDocument(int document_id, const string& document, DocumentStatus status, const vector<int>& ratings) {
        if ((document_id < 0) || (documents_.count(document_id) > 0)) {
            throw invalid_argument("Invalid document_id"s);
        }
        const auto words = SplitIntoWordsNoStop(document);

        const double inv_word_count = 1.0 / words.size();
        for (const string& word : words) {
            word_to_document_freqs_[word][document_id] += inv_word_count;
            document_to_words_freqs_[document_id][word] += inv_word_count;
        }
        documents_.emplace(document_id, DocumentData{ ComputeAverageRating(ratings), status });
        document_ids_.insert(document_id);
    }

    vector<Document> SearchServer::FindTopDocuments(const string& raw_query, DocumentStatus status) const {
        return FindTopDocuments(raw_query, [status](int document_id, DocumentStatus document_status, int rating) {
            return document_status == status;
            });
    }

    vector<Document> SearchServer::FindTopDocuments(const string& raw_query) const {
        return FindTopDocuments(raw_query, DocumentStatus::ACTUAL);
    }

    int SearchServer::GetDocumentCount() const {
        return documents_.size();
    }

    set<int>::iterator SearchServer::begin() {
        return document_ids_.begin();
    }

    set<int>::iterator SearchServer::end(){
        return document_ids_.end();
    }

    const map<string, double>& SearchServer::GetWordFrequencies(int document_id) const {
        static map<string, double> FreqsEmpty;
        if (document_to_words_freqs_.count(document_id) == 0) {
            return FreqsEmpty;
        }

        return document_to_words_freqs_.find(document_id)->second;        
    }


    void SearchServer::RemoveDocument(int document_id) {
        auto check = document_ids_.find(document_id);
        if (check == document_ids_.cend()) {
            return;
        }

        documents_.erase(document_id);
        document_ids_.erase(check);
        for (const auto& [RemoveData, Freqs] : document_to_words_freqs_[document_id]) {
            word_to_document_freqs_[RemoveData].erase(document_id);
        }
        document_to_words_freqs_.erase(document_id);
    }

    tuple<vector<string>, DocumentStatus> SearchServer::MatchDocument(const string& raw_query, int document_id) const {
        const auto query = ParseQuery(raw_query);

        vector<string> matched_words;
        for (const string& word : query.plus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            if (word_to_document_freqs_.at(word).count(document_id)) {
                matched_words.push_back(word);
            }
        }
        for (const string& word : query.minus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            if (word_to_document_freqs_.at(word).count(document_id)) {
                matched_words.clear();
                break;
            }
        }
        return { matched_words, documents_.at(document_id).status };
    }

    bool SearchServer::IsStopWord(const string& word) const {
        return stop_words_.count(word) > 0;
    }

    bool SearchServer::IsValidWord(const string& word) {
        // A valid word must not contain special characters
        return none_of(word.begin(), word.end(), [](char c) {
            return c >= '\0' && c < ' ';
            });
    }

    vector<string> SearchServer::SplitIntoWordsNoStop(const string& text) const {
        vector<string> words;
        for (const string& word : SplitIntoWords(text)) {
            if (!IsValidWord(word)) {
                throw invalid_argument("Word "s + word + " is invalid"s);
            }
            if (!IsStopWord(word)) {
                words.push_back(word);
            }
        }
        return words;
    }

    int SearchServer::ComputeAverageRating(const vector<int>& ratings) {
        if (ratings.empty()) {
            return 0;
        }
        int rating_sum = 0;
        for (const int rating : ratings) {
            rating_sum += rating;
        }
        return rating_sum / static_cast<int>(ratings.size());
    }

    SearchServer::QueryWord SearchServer::ParseQueryWord(const string& text) const {
        if (text.empty()) {
            throw invalid_argument("Query word is empty"s);
        }
        string word = text;
        bool is_minus = false;
        if (word[0] == '-') {
            is_minus = true;
            word = word.substr(1);
        }
        if (word.empty() || word[0] == '-' || !IsValidWord(word)) {
            throw invalid_argument("Query word "s + text + " is invalid");
        }

        return { word, is_minus, IsStopWord(word) };
    }

    SearchServer::Query SearchServer::ParseQuery(const string& text) const {
        Query result;
        for (const string& word : SplitIntoWords(text)) {
            const auto query_word = ParseQueryWord(word);
            if (!query_word.is_stop) {
                if (query_word.is_minus) {
                    result.minus_words.insert(query_word.data);
                }
                else {
                    result.plus_words.insert(query_word.data);
                }
            }
        }
        return result;
    }

void PrintMatchDocumentResult(int document_id, const vector<string>& words, DocumentStatus status) {
    cout << "{ "s
        << "document_id = "s << document_id << ", "s
        << "status = "s << static_cast<int>(status) << ", "s
        << "words ="s;
    for (const string& word : words) {
        cout << ' ' << word;
    }
    cout << "}"s << endl;
}

