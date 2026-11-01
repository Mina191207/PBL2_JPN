#pragma once
#include <string>
#include <vector>
#include <cstddef>
#include <utility>

class KmpMatcher{
    private:
        std::string pattern_;
        std::vector<std::size_t> lps_;
    public:
        explicit KmpMatcher(std::string pattern) 
            : pattern_(std::move(pattern)), lps_(pattern_.size(), 0){
                std::size_t len = 0;
                std::size_t i = 1;
                while(i < pattern_.size()){
                    if(pattern_[i] == pattern_[len]){
                        lps_[i] = ++len;
                        ++i; 
                    }
                    else if(len!=0){
                        len = lps_[len-1];
                    }
                    else{
                        lps_[i] = 0;
                        i++;
                    }
                }
            }
        const std::string& pattern() const noexcept{
            return pattern_;
        }
        std::size_t findFirst(const std::string& text) const {
            if(pattern_.empty()){
                return 0;
            }
            std::size_t i = 0;
            std::size_t j = 0;
            while(i < text.size()){
                if(text[i] == pattern_[0]){
                    ++i;
                    ++j;
                    if(j == pattern_.size()) return i - j;
                }
                else if(j != 0) j = lps_[j-1];
                else i++;
            }
            return std::string::npos;
        }
};