#pragma once

#include <iostream> 
#include <iterator>
#include <vector>

#include "document.h"

template <typename Iterator>
class IteratorRange {
public:
    IteratorRange(Iterator begin, Iterator end)
        : first_(begin)
        , last_(end)
        , size_(distance(first_, last_)) {
    }

    Iterator begin() const {
        return first_;
    }

    Iterator end() const {
        return last_;
    }

    size_t size() const {
        return size_;
    }

private:
    Iterator first_, last_;
    size_t size_;
};

template <typename Iterator>
std::ostream& operator<<(std::ostream& out, const IteratorRange<Iterator>& range) {
    for (Iterator it = range.begin(); it != range.end(); ++it) {
        out << *it;
    }
    return out;
}

template <typename Iterator>
class Paginator {
public:

    Paginator(Iterator begin, Iterator end, size_t page_size) {  
        Iterator begin_page = begin;
        Iterator end_page = begin;
        while (end_page != end) {
            if (distance(begin_page, end) > page_size) {
                advance(end_page, page_size);
            }
            else {
                end_page = end;
            }
            pages_.push_back({ begin_page, end_page });
            begin_page = end_page;
        }
    }

    auto begin() const {
        return pages_.begin();
    }

    auto end() const {
        return pages_.end();
    }

    size_t size() const {
        return pages_.size();
    }

private:
    std::vector<IteratorRange<Iterator>> pages_;
};

template <typename Container>
auto Paginate(const Container& c, size_t page_size) {
    return Paginator(begin(c), end(c), page_size);
}
