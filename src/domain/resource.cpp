#include "pbl2/domain/resource.h"
#include "domain/Validation.h"
#include "search/SearchText.h"

Resource::Resource(const ResourceId& id, const LevelId& levelId, const std::string& meaning)
    : id_(id),
      levelId_(levelId),
      meaning_(meaning) {
    validateTxtField(meaning_, "meaning");
}
void Resource::buildSearchKeys(std::string_view term, std::string_view first, std::string_view second) {
    searchKeys_ = {normalizeSearchText(term), normalizeSearchText(meaning_),
                   normalizeSearchText(first), normalizeSearchText(second)};
}
const ResourceId& Resource::getId() const noexcept {
    return id_;
}
const LevelId& Resource::getLevelId() const noexcept {
    return levelId_;
}
const std::string& Resource::getMeaning() const noexcept {
    return meaning_;
}
