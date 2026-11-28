#include "application/managers/ResourceManager.h"
#include "algorithms/KmpMatcher.h"
#include "algorithms/MergeSort.h"
#include "search/SearchText.h"
#include <string_view>
#include <unordered_set>

namespace {
struct ContentQuery {
    KmpMatcher matcher;
    std::unordered_set<std::string_view> ideographs;

    ContentQuery(std::string_view query, bool includeKanji) : matcher(normalizeSearchText(query)) {
        if (!includeKanji) return;
        const std::string_view text = matcher.pattern();
        std::size_t count = 0;
        // The Application validates the UTF-8 query before search.
        for (std::size_t i = 0; i < text.size();) {
            const auto lead = static_cast<unsigned char>(text[i]);
            const std::size_t width = lead < 0x80 ? 1 : lead < 0xE0 ? 2 : lead < 0xF0 ? 3 : 4;
            if (i + width > text.size()) break;
            if (width >= 3) {
                unsigned cp = lead & (width == 3 ? 0x0f : 0x07);
                for (std::size_t j = 1; j < width; ++j)
                    cp = (cp << 6) | (static_cast<unsigned char>(text[i + j]) & 0x3f);
                if ((cp >= 0x3400 && cp <= 0x4DBF) || (cp >= 0x4E00 && cp <= 0x9FFF) ||
                    (cp >= 0xF900 && cp <= 0xFAFF) || (cp >= 0x20000 && cp <= 0x3134F)) {
                    ++count;
                    ideographs.insert(text.substr(i, width));
                }
            }
            i += width;
        }
        if (count < 2) ideographs.clear();
    }

    int score(std::string_view term, std::string_view meaning, std::string_view first,
              std::string_view second, bool kanji) const {
        const auto position = matcher.findFirst(term);
        if (position == 0) return term.size() == matcher.pattern().size() ? 4 : 3;
        if (kanji && ideographs.count(term)) return 3;
        if (position != std::string_view::npos || matcher.findFirst(first) != std::string_view::npos ||
            matcher.findFirst(second) != std::string_view::npos) return 2;
        return matcher.findFirst(meaning) == std::string_view::npos ? 0 : 1;
    }
};

const std::string& reading(const Kanji& item) {
    return item.getOnyomi().empty() ? item.getKunyomi() : item.getOnyomi();
}
}

std::vector<SearchRow> ResourceManager::search(const SearchOptions& options) const {
    std::vector<SearchRow> rows;
    const auto matchesFilter = [&](const Resource& item) {
        return (!options.type || item.getType() == *options.type) &&
               (!options.levelId || item.getLevelId() == *options.levelId);
    };
    auto include = [&](const Resource& item, std::string_view term, std::string_view pronunciation, int score) {
        const Level* level = data_.findLevel(item.getLevelId());
        rows.push_back({item.getId(),
                        item.getType(),
                        std::string(term),
                        item.getMeaning(),
                        item.getLevelId(),
                        level ? level->getOrder() : 0,
                        score,
                        std::string(pronunciation)});
    };
    if (options.exactId && !options.query.empty()) {
        if (ResourceId::isValid(options.query)) {
            const Resource* item = data_.findResource(ResourceId(options.query));
            if (item && matchesFilter(*item)) {
                std::string_view pronunciation;
                if (item->getType() == ResourceType::VOCABULARY)
                    pronunciation = static_cast<const Vocabulary&>(*item).getReading();
                else if (item->getType() == ResourceType::KANJI)
                    pronunciation = reading(static_cast<const Kanji&>(*item));
                include(*item, item->getTerm(), pronunciation, 4);
            }
        }
    } else {
        const auto allows = [&](ResourceType type) { return !options.type || *options.type == type; };
        const bool browse = options.query.empty();
        const ContentQuery query(options.query, allows(ResourceType::KANJI));
        const auto match = [&](const Resource& item, std::string_view term, std::string_view pronunciation,
                               bool kanji = false) {
            const auto& keys = item.searchKeys_;
            const int score = browse ? 0 : query.score(keys.term, keys.meaning, keys.first, keys.second, kanji);
            if (browse || score > 0) include(item, term, pronunciation, score);
        };
        if (browse) rows.reserve(data_.resourceById.size());
        if (allows(ResourceType::VOCABULARY)) {
            for (const auto& item : data_.vocabularies)
                if (matchesFilter(item)) match(item, item.getWord(), item.getReading());
        }
        if (allows(ResourceType::GRAMMAR)) {
            for (const auto& item : data_.grammars)
                if (matchesFilter(item)) match(item, item.getPattern(), {});
        }
        if (allows(ResourceType::KANJI)) {
            for (const auto& item : data_.kanjis)
                if (matchesFilter(item)) match(item, item.getCharacter(), reading(item), true);
        }
    }
    mergeSort(rows, [&](const SearchRow& a, const SearchRow& b) {
        switch (options.sort) {
        case SearchSort::RELEVANCE:
            if (a.score != b.score)
                return a.score > b.score;
            if (a.levelOrder != b.levelOrder)
                return a.levelOrder < b.levelOrder;
            break;
        case SearchSort::LEVEL_ASC:
            if (a.levelOrder != b.levelOrder)
                return a.levelOrder < b.levelOrder;
            break;
        case SearchSort::LEVEL_DESC:
            if (a.levelOrder != b.levelOrder)
                return a.levelOrder > b.levelOrder;
            break;
        case SearchSort::TERM_ASC:
            if (a.term != b.term)
                return a.term < b.term;
            break;
        case SearchSort::TERM_DESC:
            if (a.term != b.term)
                return a.term > b.term;
            break;
        case SearchSort::ID:
            break;
        }
        return a.id.number() < b.id.number();
    });
    return rows;
}
