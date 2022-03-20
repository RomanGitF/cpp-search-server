#include "search_server.h"


using namespace std;

SearchServer::SearchServer(const string& stop_words_text)
    : SearchServer(SplitIntoWords(stop_words_text)) {
}

void SearchServer::AddDocument(int document_id, string_view document, DocumentStatus status, const vector<int>& ratings) {
    if ((document_id < 0) || (documents_.count(document_id) > 0)) {
        throw invalid_argument("Invalid document_id"s);
    }
    documents_.emplace(document_id, DocumentData{ ComputeAverageRating(ratings), status, string(document) });
    document_ids_.insert(document_id);
    auto words = SplitIntoWordsNoStop(documents_.at(document_id).data);
    const double inv_word_count = 1.0 / words.size();
    for (auto word : words) {
        word_to_document_freqs_[word][document_id] += inv_word_count;
        document_to_words_freqs_[document_id][word] += inv_word_count;
    }
}

vector<Document> SearchServer::FindTopDocuments(string_view raw_query, DocumentStatus status) const {
    return FindTopDocuments(execution::seq, raw_query, status);
}

vector<Document> SearchServer::FindTopDocuments(string_view raw_query) const {
    return FindTopDocuments(execution::seq, raw_query, DocumentStatus::ACTUAL);
}

int SearchServer::GetDocumentCount() const {
    return static_cast<int>(documents_.size());
}

set<int>::iterator SearchServer::begin() {
    return document_ids_.begin();
}

set<int>::iterator SearchServer::end() {
    return document_ids_.end();
}

const map<string_view, double>& SearchServer::GetWordFrequencies(int document_id) const {
    static map<string_view, double> FreqsEmpty;
    if (document_to_words_freqs_.count(document_id) == 0) {
        return FreqsEmpty;
    }
    return document_to_words_freqs_.find(document_id)->second;
}

void SearchServer::RemoveDocument(int document_id) {
    SearchServer::RemoveDocument(execution::seq, document_id);
}

void SearchServer::RemoveDocument(const std::execution::sequenced_policy&, int document_id) {
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

void SearchServer::RemoveDocument(const execution::parallel_policy&, int document_id) {
    if (!document_ids_.count(document_id)) return;
    documents_.erase(document_id);
    document_ids_.erase(document_id);
    auto documents_remove = move(document_to_words_freqs_.at(document_id));
    vector <string_view> tmp(documents_remove.size());
    transform(execution::par, documents_remove.begin(), documents_remove.end(), tmp.begin(),
        [](auto f) {
            return f.first;
        });
    for_each(execution::par, tmp.begin(), tmp.end(),
        [this, document_id](auto word) {
            word_to_document_freqs_.at(word).erase(document_id);
            return; });
}

tuple<vector<string_view>, DocumentStatus> SearchServer::MatchDocument(
    string_view raw_query, int document_id) const {
    return MatchDocument(execution::seq, raw_query, document_id);
}

tuple<vector<string_view>, DocumentStatus> SearchServer::MatchDocument(
    const std::execution::sequenced_policy&,
    string_view raw_query, int document_id) const {
    const auto query = ParseQuery(raw_query);
    vector<string_view> matched_words;
    for (auto word : query.minus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        if (word_to_document_freqs_.at(word).count(document_id)) {
            return { matched_words, documents_.at(document_id).status };
        }
    }
    for (auto word : query.plus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        if (word_to_document_freqs_.at(word).count(document_id)) {
            matched_words.push_back(word);
        }
    }
    return { matched_words, documents_.at(document_id).status };
}

tuple<vector<string_view>, DocumentStatus> SearchServer::MatchDocument(
    const std::execution::parallel_policy&, string_view raw_query, int document_id) const {
    const auto query = ParseQueryPar(raw_query);
    vector<string_view> matched_words_o{};
    if (any_of(query.minus_words.begin(), query.minus_words.end(), [this, &document_id](string_view word) {
        return document_to_words_freqs_.at(document_id).count(word);
        })) {
        return { matched_words_o, documents_.at(document_id).status };
    }
    vector<string_view> matched_words(query.plus_words.size());
    auto end = copy_if(execution::par, query.plus_words.begin(), query.plus_words.end(), matched_words.begin(),
        [this, &document_id](string_view word) {
            return word_to_document_freqs_.count(word) & word_to_document_freqs_.at(word).count(document_id);
        });
    sort(matched_words.begin(), end);
    end = unique(matched_words.begin(), end);
    matched_words.resize(end - matched_words.begin());
    return { matched_words, documents_.at(document_id).status };
}

bool SearchServer::IsStopWord(string_view word) const {
    return stop_words_.count(word) > 0;
}

bool SearchServer::IsValidWord(string_view word) {
    // A valid word must not contain special characters
    return none_of(word.begin(), word.end(), [](char c) {
        return c >= '\0' && c < ' ';
        });
}

vector<string_view> SearchServer::SplitIntoWordsNoStop(string_view text) const {
    vector<string_view> words;
    for (string_view word : SplitIntoWords(text)) {
        if (!IsValidWord(word)) {
            throw invalid_argument("The word is invalid"s);
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
    rating_sum = std::accumulate(ratings.begin(), ratings.end(), 0);

    return rating_sum / static_cast<int>(ratings.size());
}

SearchServer::QueryWord SearchServer::ParseQueryWord(string_view text) const {
    if (text.empty()) {
        throw invalid_argument("Query word is empty"s);
    }
    string_view word = text;
    bool is_minus = false;
    if (word[0] == '-') {
        is_minus = true;
        word = word.substr(1);
    }
    if (word.empty() || word[0] == '-' || !IsValidWord(word)) {
        throw invalid_argument("Query word is invalid"s);
    }

    return { word, is_minus, IsStopWord(word) };
}

SearchServer::Query SearchServer::ParseQueryPar(string_view text) const {
    Query result;
    for (auto word : SplitIntoWords(text)) {
        const auto query_word = ParseQueryWord(word);
        if (!query_word.is_stop) {
            if (query_word.is_minus) {
                result.minus_words.push_back(query_word.data);
            }
            else {
                result.plus_words.push_back(query_word.data);
            }
        }
    }
    return result;
}

SearchServer::Query SearchServer::ParseQuery(string_view text) const {
    Query result;
    for (auto word : SplitIntoWords(text)) {
        const auto query_word = ParseQueryWord(word);
        if (!query_word.is_stop) {
            if (query_word.is_minus) {
                result.minus_words.push_back(query_word.data);
            }
            else {
                result.plus_words.push_back(query_word.data);
            }
        }
    }
    sort(result.minus_words.begin(), result.minus_words.end());
    auto end_minus = unique(result.minus_words.begin(), result.minus_words.end());
    result.minus_words.resize(end_minus - result.minus_words.begin());
    sort(result.plus_words.begin(), result.plus_words.end());
    auto end_plus = unique(result.plus_words.begin(), result.plus_words.end());
    result.plus_words.resize(end_plus - result.plus_words.begin());
    return result;
}


