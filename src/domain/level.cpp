#include "../../include/domain/level.h"
#include  "../../include/domain/Validation.h"

#include <stdexcept>
#include <utility>

Level::Level(const LevelId& id, std::string category, std::string name, int order, std::string description) 
    : id_(id), category_(std::move(category)), name_(std::move(name)), order_(order), description_(std::move(description)){
    validateTxtField(category_, "category");
    validateTxtField(name_, "name");
    validateTxtField(description_, "description", false);
    if(order_ <= 0){
        throw std::invalid_argument("Level order must be positive");
    };
    for(char& c : category_){
        if(c >= 'a' && c <= 'z') c = static_cast<char>(c-'a'+'A');
    }
}
const LevelId& Level::getId() const noexcept{
    return id_;
}
const std::string& Level::getCategory() const noexcept{
    return category_;
}
const std::string& Level::getName() const noexcept{
    return name_;
}
int Level::getOrder() const noexcept{
    return order_;
}
const std::string& Level::getDescription() const noexcept{
    return description_;
}