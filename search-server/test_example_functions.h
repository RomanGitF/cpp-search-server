#pragma once
#include "document.h"
#include "search_server.h"


#include <vector>


void AddDocument(SearchServer& search_server, int document_id, const  std::string& document, DocumentStatus status,
    const  std::vector<int>& ratings);



