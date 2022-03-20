#pragma once

#include <vector>
#include <set>
#include <string>


std::vector<std::string_view> SplitIntoWords(std::string_view text);

template <typename StringContainer>
std::set<std::string_view, std::less<>> MakeUniqueNonEmptyStrings(const StringContainer& strings) {
    std::set<std::string_view, std::less<>> non_empty_strings;
    for (const auto& str : strings) {
        if (!str.empty()) {
            non_empty_strings.insert(str);
        }
    }
    return non_empty_strings;


}


