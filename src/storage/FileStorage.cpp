#include "../../include/storage/FileStorage.h"

#include <exception>
#include <fstream>
#include <stdexcept>

namespace{
    std::string trim(const std::string& text){
        std::size_t first = text.find_first_not_of(" \t");
        if (first == std::string::npos) return "";
        std::size_t last = text.find_last_not_of(" \t");
        return text.substr(first, last - first +1);
    };

    int sliptFields(
        const std::string& line,
        std::string fields[],
        int capacity
    ){
        int count = 0;
        std::string field;

        for(std::size_t i=0; i <= line.size();i++){
            if(i == line.size() || line[i] == '|'){
                if(count == capacity) return -1;
                fields[count++] = trim(field);
                field.clear();
            }
            else{
                field += line[i];
            }
        }
        return count;
    };
    bool parsePartOfSpeech(
        const std::string& text,
        PartOfSpeech& result
    ) {
        if (text == "NOUN")
            result = PartOfSpeech::NOUN;
        else if (text == "VERB")
            result = PartOfSpeech::VERB;
        else if (text == "I_ADJ")
            result = PartOfSpeech::IADJECTIVE;
        else if (text == "NA_ADJ")
            result = PartOfSpeech::NAADJECTIVE;
        else if (text == "ADV")
            result = PartOfSpeech::ADVERB;
        else if (text == "OTHER")
            result = PartOfSpeech::OTHER;
        else
            return false;

        return true;
    };
};

namespace storage{
    vocabulary parseVocabularyLine(const std::string& line){
        std::string fields[6];
        int count = sliptFields(line,fields, 6);
        if(count == -1){
            throw std::invalid_argument("thua cot (can dung 6 truong)");
        }
        if(count != 6){
            throw std::invalid_argument("can 6 turong, nhan" + std::to_string(count));
        }
        PartOfSpeech pos = PartOfSpeech::OTHER;
        if(!parsePartOfSpeech(fields[4],pos)){
            throw std::invalid_argument(  "Tu loai khong hop le: " + fields[4]);
        }
        return vocabulary(ResourceId(fields[0]), fields[1], fields[2], fields[3], pos, fields[5]);
    }

    bool loadVocabularyFile(const std::string& path, LinkedList<vocabulary>& output, std::string& error){
        error.clear();
        std::ifstream file(path);
        
        if(!file){
            error = "Khong mo duoc file" + path;
            return false;
        }
        LinkedList<vocabulary> loaded;
        std::string line;
        int lineNumber = 0;
        while(std::getline(file,line)){
            ++lineNumber;
            // moi file txt duoc gan san dau dong la BOM 
            if(lineNumber == 1 && line.compare(0,3," \xEF\xBB\xBF") == 0) line.erase(0,3);

            if(!line.empty() && line.back()=='\r') line.pop_back();
            if(trim(line).empty()) continue;
            
            try{
                vocabulary item = parseVocabularyLine(line);
                loaded.pushBack(item);
            }

            catch(const std::exception& e){
                error = path +  ", doing " + std::to_string(lineNumber) + ": " + e.what();
                return false;
            }
        }
        if(file.bad()){
            error = "Loi khi doc file" + path;
            return false;
        }
        output = loaded;
        return true;
    }
}