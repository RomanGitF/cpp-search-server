#pragma once

#include <vector>
#include <set>
#include <string>
#include <algorithm>
#include <map>
#include <cmath>
#include <exception>
#include <iterator>
#include <execution>
#include <string_view>


#include "document.h"
#include "string_processing.h"
#include "log_duration.h"
#include "concurrent_map.h"


const int MAX_RESULT_DOCUMENT_COUNT = 5;
inline static constexpr double EPSILON = 1e-6;

class SearchServer {
public:
    template <typename StringContainer>
    explicit SearchServer(const StringContainer& stop_words);

    explicit SearchServer(const std::string& stop_words_text);

    void AddDocument(int document_id, std::string_view document, DocumentStatus status, const std::vector<int>& ratings);

    template <typename DocumentPredicate>
    std::vector<Document> FindTopDocuments(std::string_view raw_query, DocumentPredicate document_predicate) const;

    template <typename DocumentPredicate, typename ExecutionPolicy>
    std::vector<Document> FindTopDocuments(ExecutionPolicy&& policy, std::string_view raw_query, DocumentPredicate document_predicate) const;

    std::vector<Document> FindTopDocuments(std::string_view raw_query, DocumentStatus status) const;

    template <typename ExecutionPolicy>
    std::vector<Document> FindTopDocuments(ExecutionPolicy&& policy, std::string_view raw_query, DocumentStatus status) const;

    std::vector<Document> FindTopDocuments(std::string_view raw_query) const;

    template <typename ExecutionPolicy>
    std::vector<Document> FindTopDocuments(ExecutionPolicy&& policy, std::string_view raw_query) const;

    int GetDocumentCount() const;

    std::set<int>::iterator begin();

    std::set<int>::iterator end();

    const std::map<std::string_view, double>& GetWordFrequencies(int document_id) const;

    void RemoveDocument(int document_id);

    void RemoveDocument(const std::execution::sequenced_policy&, int document_id);

    void RemoveDocument(const std::execution::parallel_policy&, int document_id);

    std::tuple<std::vector<std::string_view>, DocumentStatus>
        MatchDocument(std::string_view raw_query, int document_id) const;

    std::tuple<std::vector<std::string_view>, DocumentStatus>
        MatchDocument(const std::execution::sequenced_policy& , 
            std::string_view raw_query, int document_id) const;

    std::tuple<std::vector<std::string_view>, DocumentStatus>
        MatchDocument(const std::execution::parallel_policy&,
            std::string_view raw_query, int document_id) const;

private:
    struct DocumentData {
        int rating;
        DocumentStatus status;
        std::string data;
    };
    const std::set<std::string_view, std::less<>> stop_words_;

    std::map<std::string_view, std::map<int, double>> word_to_document_freqs_;
    std::map<int, std::map<std::string_view, double>> document_to_words_freqs_;
    std::map<int, DocumentData> documents_;
    std::set<int> document_ids_;

    bool IsStopWord(std::string_view word) const;

    static bool IsValidWord(std::string_view word);

    std::vector<std::string_view> SplitIntoWordsNoStop(std::string_view text) const;

    static int ComputeAverageRating(const std::vector<int>& ratings);

    struct QueryWord {
        std::string_view data;
        bool is_minus;
        bool is_stop;
    };

    QueryWord ParseQueryWord(std::string_view text) const;

    struct Query {
        std::vector<std::string_view> plus_words;
        std::vector<std::string_view> minus_words;
    };

    Query ParseQuery(std::string_view text) const;

    Query ParseQueryPar(std::string_view text) const;

    double ComputeWordInverseDocumentFreq(std::string_view word) const {
        return log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
    }

    template <typename DocumentPredicate>
    std::vector<Document> FindAllDocuments(std::string_view raw_query, DocumentPredicate document_predicate) const;

    template <typename DocumentPredicate>
    std::vector<Document> FindAllDocuments(const std::execution::sequenced_policy& policy, std::string_view raw_query, DocumentPredicate document_predicate) const;

    template <typename DocumentPredicate>
    std::vector<Document> FindAllDocuments(const std::execution::parallel_policy& policy, std::string_view raw_query, DocumentPredicate document_predicate) const;
};

    template <typename StringContainer>
    SearchServer::SearchServer(const StringContainer& stop_words)
        : stop_words_(MakeUniqueNonEmptyStrings(stop_words)) 
    {
        if (!all_of(stop_words_.begin(), stop_words_.end(), IsValidWord)) {
            throw std::invalid_argument("Some of stop words are invalid");
        }
    }
  
    template <typename DocumentPredicate>
    std::vector<Document> SearchServer::FindTopDocuments(std::string_view raw_query, DocumentPredicate document_predicate) const {
        return SearchServer::FindTopDocuments(std::execution::seq, raw_query, document_predicate);
    }

    template <typename DocumentPredicate, typename ExecutionPolicy>
    std::vector<Document> SearchServer::FindTopDocuments(ExecutionPolicy&& policy, std::string_view raw_query, DocumentPredicate document_predicate) const {
        auto matched_documents = SearchServer::FindAllDocuments(policy, raw_query, document_predicate);

        std::sort(
            policy,
            matched_documents.begin(), matched_documents.end(),
            [](const Document& lhs, const Document& rhs) {
                if (std::abs(lhs.relevance - rhs.relevance) < EPSILON) {
                    return lhs.rating > rhs.rating;
                }
                else {
                    return lhs.relevance > rhs.relevance;
                }
            });

        if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
            matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
        }
        return matched_documents;
    }

    template <typename ExecutionPolicy>
    std::vector<Document> SearchServer::FindTopDocuments(ExecutionPolicy&& policy, std::string_view raw_query, DocumentStatus status) const {
        return FindTopDocuments(policy, raw_query, [status](int document_id, DocumentStatus document_status, int rating) {
            return document_status == status;
            });
    }

    template <typename ExecutionPolicy>
    std::vector<Document> SearchServer::FindTopDocuments(ExecutionPolicy&& policy, std::string_view raw_query) const {
        return FindTopDocuments(policy, raw_query, DocumentStatus::ACTUAL);
    }

    template <typename DocumentPredicate>
    std::vector<Document> SearchServer::FindAllDocuments(const std::execution::parallel_policy& policy, std::string_view raw_query, DocumentPredicate document_predicate) const {

        ConcurrentMap<int, double> document_to_relevance(16);
        const auto query = ParseQuery(raw_query);
        
        std::for_each(
            policy,
            query.minus_words.begin(), query.minus_words.end(),
            [this, &document_to_relevance](std::string_view word) {
                if (word_to_document_freqs_.count(word)) {
                    for (const auto [document_id, _] : word_to_document_freqs_.at(word)) {
                        document_to_relevance.Erase(document_id);
                    }
                }
            });

        std::for_each(
            policy,
            query.plus_words.begin(), query.plus_words.end(),
            [this, &document_predicate, &document_to_relevance](std::string_view word) {
                if (word_to_document_freqs_.count(word)) {
                    const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
                    for (const auto [document_id, term_freq] : word_to_document_freqs_.at(word)) {
                        const auto& document_data = documents_.at(document_id);
                        if (document_predicate(document_id, document_data.status, document_data.rating)) {
                            document_to_relevance[document_id].ref_to_value += term_freq * inverse_document_freq;
                        }
                    }
                }
            }
        );
        std::map<int, double> document_to_relevance_reduced = document_to_relevance.BuildOrdinaryMap();
        std::vector<Document> matched_documents;
        matched_documents.reserve(document_to_relevance_reduced.size());

        for (const auto [document_id, relevance] : document_to_relevance_reduced) {
            matched_documents.push_back({ document_id, relevance, documents_.at(document_id).rating });
        }
        return matched_documents;
    }


    template <typename DocumentPredicate>
    std::vector<Document> SearchServer::FindAllDocuments(const std::execution::sequenced_policy&, std::string_view raw_query, DocumentPredicate document_predicate) const {
        return SearchServer::FindAllDocuments(raw_query, document_predicate);

    }

    template <typename DocumentPredicate>
    std::vector<Document> SearchServer::FindAllDocuments(std::string_view raw_query, DocumentPredicate document_predicate) const {
        std::map<int, double> document_to_relevance;
        const auto query = ParseQuery(raw_query);
        for (std::string_view word : query.plus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
            for (const auto [document_id, term_freq] : word_to_document_freqs_.at(word)) {
                const auto& document_data = documents_.at(document_id);
                if (document_predicate(document_id, document_data.status, document_data.rating)) {
                    document_to_relevance[document_id] += term_freq * inverse_document_freq;
                }
            }
        }

        for (auto word : query.minus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            for (const auto [document_id, _] : word_to_document_freqs_.at(word)) {
                document_to_relevance.erase(document_id);
            }
        }

        std::vector<Document> matched_documents;
        for (const auto [document_id, relevance] : document_to_relevance) {
            matched_documents.push_back({ document_id, relevance, documents_.at(document_id).rating });
        }
        return matched_documents;
    }