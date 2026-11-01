#include "../../include/domain/resource.h"
#include "../../include/domain/Validation.h"

Resource::Resource(const ResourceId& id, const LevelId& levelId, const std::string& meaning) 
    : id_(id), levelId_(levelId), meaning_(meaning){
        validateTxtField(meaning_, "meaning");
}
const ResourceId& Resource::getId() const noexcept{
    return id_;
}
const LevelId& Resource::getLevelId() const noexcept{
    return levelId_;
}
const std::string& Resource::getMeaning() const noexcept{
    return meaning_;
}
