#pragma once

#include <string>
#include "../domain/vocabulary.h"
#include "../dsa/LinkedList.h"

namespace storage{
    vocabulary parseVocabularyLine(const std::string& line);
    
    bool loadVocabularyFile(
        const std::string& path,
        LinkedList<vocabulary>& output,
        std::string& error
    );
}