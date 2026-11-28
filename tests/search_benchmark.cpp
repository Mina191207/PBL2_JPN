#include "pbl2/Application.h"
#include "ReplacedFiles.h"
#include "application/managers/ResourceManager.h"
#include "storage/FileStorage.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <random>
#include <string_view>

namespace {
using Clock = std::chrono::steady_clock;
constexpr unsigned seed = 42;
constexpr int warmup = 5;
constexpr int samples = 32;
std::size_t observed = 0;

DataStore makeFixture(std::size_t size) {
    DataStore data;
    const LevelId first("L001"), second("L002");
    data.users.emplace_back(UserId("U001"), "bench-admin", "fixture-only", UserRole::ADMIN, UserStatus::ACTIVE);
    data.levels.emplace_back(first, "JLPT", "N5", 5, "");
    data.levels.emplace_back(second, "JLPT", "N4", 4, "");
    data.vocabularies.emplace_back(ResourceId("R001"), first, "学校", "がっこう", "trường học", PartOfSpeech::NOUN, "");
    data.vocabularies.emplace_back(ResourceId("R002"), second, "学校生活", "がっこうせいかつ", "school life", PartOfSpeech::NOUN, "");
    data.kanjis.emplace_back(ResourceId("R003"), first, "学", "study", 8, "ガク", "まなぶ", "");
    data.kanjis.emplace_back(ResourceId("R004"), second, "校", "school", 10, "コウ", "", "");
    std::mt19937 random(seed);
    for (std::size_t i = 5; i <= size; ++i) {
        const auto id = idAfter<ResourceId>(i - 1);
        const auto level = random() % 2 ? first : second;
        const auto term = "prefix-" + std::to_string(random() % 500) + "-" + id.getId();
        if (i % 10 == 0)
            data.kanjis.emplace_back(id, level, id.getId(), "fixture meaning", 8, "オン", "くん", "");
        else if (i % 5 == 0)
            data.grammars.emplace_back(id, level, term, "fixture meaning", "verb structure", "usage", "", "");
        else
            data.vocabularies.emplace_back(id, level, term, "fixture reading", "fixture meaning", PartOfSpeech::NOUN, "");
    }
    data.rebuildIndexes();
    return data;
}

// Step 1 combines submission and persistence in search(); the read baseline calls the same manager directly.
template<class A>
auto submit(A& app, const SearchOptions& options, int) -> decltype(app.submitSearch(options)) {
    return app.submitSearch(options);
}
template<class A>
auto submit(A& app, const SearchOptions& options, long) {
    return app.search(options);
}

template<class Action>
void measure(std::size_t size, const char* query, const char* mode, std::size_t replaced, Action action) {
    for (int i = 0; i < warmup; ++i) observed += action();
    std::array<double, samples> micros{};
    for (auto& elapsed : micros) {
        const auto start = Clock::now();
        observed += action();
        elapsed = std::chrono::duration<double, std::micro>(Clock::now() - start).count();
    }
    std::sort(micros.begin(), micros.end());
    const double median = (micros[samples / 2 - 1] + micros[samples / 2]) / 2;
    const auto p95 = micros[static_cast<std::size_t>(std::ceil(0.95 * samples)) - 1];
    std::cout << size << ',' << query << ',' << mode << ',' << median << ',' << p95 << ',' << replaced << '\n';
}

void run(std::size_t size) {
    TempDirectory temp;
    auto data = makeFixture(size);
    const auto folder = temp.path.string();
    std::string error;
    CHECK(FileStorage{}.saveAll(folder, data, error));
    Application app(seed);
    CHECK(app.load(folder).ok() && app.login("bench-admin", "fixture-only").ok());
    const UserManager users(data);
    const ResourceManager manager(data, users);
    const std::array<std::pair<const char*, const char*>, 6> queries{{
        {"hit", "学校"}, {"miss", "not-present"}, {"prefix", "prefix-12"},
        {"kanji", "学校学"}, {"id", "R001"}, {"empty", ""}
    }};
    for (const auto& [label, text] : queries) {
        CHECK(app.clearHistory().ok());
        SearchOptions options;
        options.query = text;
        options.exactId = std::string_view(label) == "id";
        const ReplacedFiles beforeRead(temp.path);
        const auto rows = manager.search(options);
        CHECK(beforeRead.names().empty());
        measure(size, label, "read", 0, [&] { return manager.search(options).size(); });
        const ReplacedFiles beforeSubmit(temp.path);
        auto submitted = submit(app, options, 0);
        CHECK(submitted.ok() && submitted.status.save != SaveState::PENDING);
        CHECK(submitted.value->size() == rows.size());
        for (std::size_t i = 0; i < rows.size(); ++i)
            CHECK(rows[i].id == (*submitted.value)[i].id && rows[i].score == (*submitted.value)[i].score);
        measure(size, label, "submit", beforeSubmit.names().size(), [&] {
            const auto result = submit(app, options, 0);
            CHECK(result.ok() && result.status.save != SaveState::PENDING);
            return result.value->size();
        });
    }
}
}

int main() {
    try {
        std::cout << std::fixed << std::setprecision(2) << "resources,query,mode,median_us,p95_us,replaced\n";
        run(1000);
        run(10000);
        CHECK(observed > 0);
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
