// search_server_s1_t2_v2.cpp

#include <algorithm>
#include <cmath>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

using namespace std;

const int MAX_RESULT_DOCUMENT_COUNT = 5;

string ReadLine() {
    string s;
    getline(cin, s);
    return s;
}

int ReadLineWithNumber() {
    int result;
    cin >> result;
    ReadLine();
    return result;
}

vector<string> SplitIntoWords(const string& text) {
    vector<string> words;
    string word;
    for (const char c : text) {
        if (c == ' ') {
            if (!word.empty()) {
                words.push_back(word);
                word.clear();
            }
        } else {
            word += c;
        }
    }
    if (!word.empty()) {
        words.push_back(word);
    }

    return words;
}
    
struct Document {
    int id;
    double relevance;
    int rating;
};

enum class DocumentStatus {
    ACTUAL,
    IRRELEVANT,
    BANNED,
    REMOVED,
};

class SearchServer {
public:
    void SetStopWords(const string& text) {
        for (const string& word : SplitIntoWords(text)) {
            stop_words_.insert(word);
        }
    }    
    
    void AddDocument(int document_id, const string& document, DocumentStatus status, const vector<int>& ratings) {
        const vector<string> words = SplitIntoWordsNoStop(document);
        const double inv_word_count = 1.0 / words.size();
        for (const string& word : words) {
            word_to_document_freqs_[word][document_id] += inv_word_count;
        }
        documents_.emplace(document_id, 
            DocumentData{
                ComputeAverageRating(ratings), 
                status
            });
    }

    vector<Document> FindTopDocuments(const string& raw_query, DocumentStatus query_status = DocumentStatus::ACTUAL) const {            
        return FindTopDocuments(raw_query, [query_status](int document_id, DocumentStatus status, int rating) { return status == query_status; });  
    }    

    template <typename Predicate> 
    vector<Document> FindTopDocuments(const string& raw_query, Predicate predicate) const {            
        const Query query = ParseQuery(raw_query);
        auto matched_documents = FindAllDocuments(query, predicate);
        
        sort(matched_documents.begin(), matched_documents.end(),
             [](const Document& lhs, const Document& rhs) {
                if (abs(lhs.relevance - rhs.relevance) < 1e-6) {
                    return lhs.rating > rhs.rating;
                } else {
                    return lhs.relevance > rhs.relevance;
                }
             });
        if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
            matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
        }
        return matched_documents;
    }

    int GetDocumentCount() const {
        return documents_.size();
    }
    
    tuple<vector<string>, DocumentStatus> MatchDocument(const string& raw_query, int document_id) const {
        const Query query = ParseQuery(raw_query);
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
        return {matched_words, documents_.at(document_id).status};
    }
    
private:
    struct DocumentData {
        int rating;
        DocumentStatus status;
    };

    set<string> stop_words_;
    map<string, map<int, double>> word_to_document_freqs_;
    map<int, DocumentData> documents_;
    
    bool IsStopWord(const string& word) const {
        return stop_words_.count(word) > 0;
    }
    
    vector<string> SplitIntoWordsNoStop(const string& text) const {
        vector<string> words;
        for (const string& word : SplitIntoWords(text)) {
            if (!IsStopWord(word)) {
                words.push_back(word);
            }
        }
        return words;
    }
    
    static int ComputeAverageRating(const vector<int>& ratings) {
        if (ratings.empty()) {
            return 0;
        }
        int rating_sum = 0;
        for (const int rating : ratings) {
            rating_sum += rating;
        }
        return rating_sum / static_cast<int>(ratings.size());
    }
    
    struct QueryWord {
        string data;
        bool is_minus;
        bool is_stop;
    };
    
    QueryWord ParseQueryWord(string text) const {
        bool is_minus = false;
        // Word shouldn't be empty
        if (text[0] == '-') {
            is_minus = true;
            text = text.substr(1);
        }
        return {
            text,
            is_minus,
            IsStopWord(text)
        };
    }
    
    struct Query {
        set<string> plus_words;
        set<string> minus_words;
    };
    
    Query ParseQuery(const string& text) const {
        Query query;
        for (const string& word : SplitIntoWords(text)) {
            const QueryWord query_word = ParseQueryWord(word);
            if (!query_word.is_stop) {
                if (query_word.is_minus) {
                    query.minus_words.insert(query_word.data);
                } else {
                    query.plus_words.insert(query_word.data);
                }
            }
        }
        return query;
    }
    
    // Existence required
    double ComputeWordInverseDocumentFreq(const string& word) const {
        return log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
    }

    template <typename Predicate>
    vector<Document> FindAllDocuments(const Query& query, Predicate predicate) const {
        map<int, double> document_to_relevance;
        for (const string& word : query.plus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
            for (const auto [document_id, term_freq] : word_to_document_freqs_.at(word)) {
                DocumentData data = documents_.at(document_id); 
                if (predicate(document_id, data.status, data.rating))
                document_to_relevance[document_id] += term_freq * inverse_document_freq;
             }

        }
        
        for (const string& word : query.minus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            for (const auto [document_id, _] : word_to_document_freqs_.at(word)) {
                document_to_relevance.erase(document_id);
            }
        }

        vector<Document> matched_documents;
        for (const auto [document_id, relevance] : document_to_relevance) {
            matched_documents.push_back({
                document_id,
                relevance,
                documents_.at(document_id).rating
            });     
        } 
        return matched_documents;
    }
};

// --------------------------------------------Start sprint 2
void TestAddDocument(){  // 1
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = {1, 2, 3};
    SearchServer server;
    server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);

    ASSERT(!server.FindTopDocuments("cat"s).empty());
}

void TestExcludeStopWordsFromAddedDocumentContent() { // 2
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = {1, 2, 3};
    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("in"s);
        ASSERT_EQUAL(found_docs.size(), 1u);
        const Document& doc0 = found_docs[0];
        ASSERT_EQUAL(doc0.id, doc_id);
    }
    {
        SearchServer server;
        server.SetStopWords("in the"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT_HINT(server.FindTopDocuments("in"s).empty(), "Stop words must be excluded from documents"s);
    }
}

void TestExcludeMinusWords() {  // 3
    const int doc_id = 42; 
    const string content = "cat in the city"s;
    const vector<int> ratings = {1, 2, 3};
    SearchServer server;
    server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
    ASSERT_HINT(server.FindTopDocuments("in the -cat"s).empty(), "Wrong: found minus word");
    }

void TestMatchedDocuments() { // 4
    const int doc_id = 42;
    const string content = "cat and dog in the city"s;
    const vector<int> ratings = {1, 2, 3};
    vector<string> answer = {"in"s, "the"s, "cat"s};

    SearchServer server;
    server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);

    ASSERT_HINT(get<vector<string>>(server.MatchDocument("in the -cat"s, doc_id)).empty(), "Wrong: count minus words");

        vector<string> answer_for_test = get<vector<string>>(server.MatchDocument("in the cat"s, doc_id));
        auto MD = [] (vector<string>& lhs, vector<string>& rhs){
            if (rhs.size() != lhs.size())
                return false;
            sort(lhs.begin(), lhs.end());
            sort(rhs.begin(), rhs.end());
            for (int i = 0; i<rhs.size(); ++i){
                if (lhs[i] == rhs[i])
                    continue; else return false;}
                return true;};

    ASSERT_HINT(MD(answer_for_test, answer), "Wrong count words");
}

void TestLineRelevance(){ // 5
    SearchServer server;
    server.AddDocument(100, "cat in the city"s, DocumentStatus::ACTUAL, {1, 2, 3});
    server.AddDocument(101, "dog in the town"s, DocumentStatus::ACTUAL, {1, 2, 3});
    server.AddDocument(102, "dog and rabbit in the town"s, DocumentStatus::ACTUAL, {1, 2, 3});
    const auto found_docs = server.FindTopDocuments("dog in the town"s);

    ASSERT_HINT(found_docs[0].relevance >= found_docs[1].relevance, "Wrong line relevance");
    ASSERT_HINT(found_docs[1].relevance >= found_docs[2].relevance, "Wrong line relevance");
}

void TestRating() { // 6
    SearchServer server;
    server.AddDocument(100, "cat"s, DocumentStatus::ACTUAL, {1, 2, 3, 4, 5});
    server.AddDocument(101, "dog"s, DocumentStatus::ACTUAL, {-1, -2, -3, -4, -5}); 
    server.AddDocument(102, "cow"s, DocumentStatus::ACTUAL, {0});     
    ASSERT_HINT(server.FindTopDocuments("cat"s)[0].rating == 3, "Wrong rating");
    ASSERT_HINT(server.FindTopDocuments("dog"s)[0].rating == -3, "Wrong negative rating");
    ASSERT_HINT(server.FindTopDocuments("cow"s)[0].rating == 0, "Wrong O rating");   
}

void TestPredicate () { // 7
    SearchServer server;
    server.AddDocument(100, "cat in the city"s, DocumentStatus::ACTUAL, {1, 2, 3});
    server.AddDocument(101, "dog in the town"s, DocumentStatus::IRRELEVANT, {-1, -2, -3});
    server.AddDocument(102, "dog and rabbit in the town"s, DocumentStatus::ACTUAL, {-4, -5, -6});

    const auto found_docs = server.FindTopDocuments("in the cat"s, [](int document_id, DocumentStatus status, int rating) { return rating > 0; });
    ASSERT_HINT(found_docs[0].id == 100, "Wrong predicate");
}

void TestStatus () {  // 8
    const int doc_id1 = 42; const int doc_id2 = 43; const int doc_id3 = 44;
    const string content1 = "cat in the city"s;
    const string content2 = "cat in the town"s;
    const string content3 = "cat in the town or city"s;   
    const vector<int> ratings = {1, 2, 3};
    SearchServer server;
    server.AddDocument(doc_id1, content1, DocumentStatus::ACTUAL, ratings);
    server.AddDocument(doc_id2, content2, DocumentStatus::IRRELEVANT, ratings);
    server.AddDocument(doc_id3, content3, DocumentStatus::IRRELEVANT, ratings);   

    const auto found_docs = server.FindTopDocuments("in the cat"s, DocumentStatus::IRRELEVANT);  

    ASSERT_HINT(found_docs[0].id == doc_id2, "Wrong status"); 
    ASSERT_HINT(found_docs[1].id == doc_id3, "Wrong status");
    ASSERT_HINT(found_docs.size() == 2, "Wrong status request"); 

}

void TestRelevance(){ // 9
    SearchServer server;
    server.AddDocument(100, "белый кот и модный ошейник"s, DocumentStatus::ACTUAL, {1, 2, 3});
    server.AddDocument(101, "пушистый кот пушистый хвост"s, DocumentStatus::ACTUAL, {1, 2, 3});
    server.AddDocument(102, "ухоженный пёс выразительные глаза"s, DocumentStatus::ACTUAL, {1, 2, 3});
    const auto found_docs = server.FindTopDocuments("пушистый ухоженный кот"s);

    ASSERT_HINT(fabs(found_docs[0].relevance-0.6507)<1e-4, "Wrong relevance");
}

void TestSearchServer() {
    RUN_TEST(TestAddDocument);  // Добавление документов - 1
    RUN_TEST(TestExcludeStopWordsFromAddedDocumentContent); // Поддержка стоп-слов - 2
    RUN_TEST(TestExcludeMinusWords);// Поддержка минус-слов - 3
    RUN_TEST(TestMatchedDocuments); // Матчинг - 4
    RUN_TEST(TestRelevance); //Сортировка по релевантности - 5ж   
    RUN_TEST(TestRating); //вычисление рейтинга 6
    RUN_TEST(TestPredicate); //Работоспособность предиката 7
    RUN_TEST(TestStatus); // Поиск с заданным статусом 8
    RUN_TEST(TestLineRelevance); // Правильный расчет релевантности 9
}
// ------------------------------------FINISH sprint 2

void PrintDocument(const Document& document) {
    cout << "{ "s
         << "document_id = "s << document.id << ", "s
         << "relevance = "s << document.relevance << ", "s
         << "rating = "s << document.rating
         << " }"s << endl;
}

int main() {
    
    TestSearchServer();  //Sprint 2
    
    SearchServer search_server;
    search_server.SetStopWords("и в на"s);

    search_server.AddDocument(0, "белый кот и модный ошейник"s,        DocumentStatus::ACTUAL, {8, -3});
    search_server.AddDocument(1, "пушистый кот пушистый хвост"s,       DocumentStatus::ACTUAL, {7, 2, 7});
    search_server.AddDocument(2, "ухоженный пёс выразительные глаза"s, DocumentStatus::ACTUAL, {5, -12, 2, 1});
    search_server.AddDocument(3, "ухоженный скворец евгений"s,         DocumentStatus::BANNED, {9});

    cout << "ACTUAL by default:"s << endl;
    for (const Document& document : search_server.FindTopDocuments("пушистый ухоженный кот"s)) {
        PrintDocument(document);
    }

    cout << "BANNED:"s << endl;
    for (const Document& document : search_server.FindTopDocuments("пушистый ухоженный кот"s, DocumentStatus::BANNED)) {
        PrintDocument(document);
    }

    cout << "Even ids:"s << endl;
    for (const Document& document : search_server.FindTopDocuments("пушистый ухоженный кот"s, [](int document_id, DocumentStatus status, int rating) { return document_id % 2 == 0; })) {
        PrintDocument(document);
    }

    return 0;
}
