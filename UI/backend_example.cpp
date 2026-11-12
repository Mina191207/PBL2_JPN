#include "Application.h"
#include <filesystem>
#include <iostream>
#include <random>
#include <stdexcept>
#include <string>
#include <utility>

namespace {
void accept(Application& app, const Status& status) {
    if (!status.ok()) throw std::runtime_error(status.message);
    if (status.save == SaveState::PENDING) {
        const auto retry = app.retrySave();
        if (!retry.ok()) throw std::runtime_error(retry.message);
    }
}
template<class T> T value(Application& app, Result<T> result) {
    accept(app, result.status);
    if (!result.value) throw std::runtime_error("missing backend result");
    return std::move(*result.value);
}
}

int main() {
    // Every run creates its own test data; no path points to repository/database.
    const auto root = std::filesystem::temp_directory_path() /
                      ("pbl2_example_" + std::to_string(std::random_device{}()));
    try {
        if (!std::filesystem::create_directory(root))
            throw std::runtime_error("cannot create example directory");
        Application backend(42);
        accept(backend, backend.initialize((root / "data").string(), "admin", "demo123"));
        accept(backend, backend.login("admin", "demo123"));
        const auto level = value(backend, backend.createLevel({"JLPT", "N5", 5, "Ví dụ"}));
        const auto word = value(backend, backend.createVocabulary(
            {level, "学校", "がっこう", "trường học", PartOfSpeech::NOUN, ""}));
        const auto learner = value(backend, backend.createAccount("learner", "demo456", UserRole::LEARNER));
        (void)learner;
        value(backend, backend.createQuestion(
            {level, word, "学校 nghĩa là gì?", {"trường học", "nhà", "sách", "tàu"},
             'A', "学校: trường học", QuestionStatus::ACTIVE}));
        accept(backend, backend.exportPackage((root / "package").string()));

        accept(backend, backend.login("learner", "demo456"));
        SearchOptions options;
        options.query = "学校";
        const auto rows = value(backend, backend.search(options));
        std::cout << "Kết quả: " << rows.size() << '\n';
        accept(backend, backend.addFavorite(word));
        accept(backend, backend.startReview());
        std::cout << value(backend, backend.review()).current->front << '\n';
        accept(backend, backend.revealReview());
        std::cout << *value(backend, backend.review()).current->back << '\n';
        accept(backend, backend.rateReview(true));
        accept(backend, backend.startQuiz(level, 1));
        const auto quiz = value(backend, backend.quiz());
        std::cout << quiz.questions.front().prompt << '\n';
        accept(backend, backend.selectAnswer(0, 'A'));
        // Submit computes the score once; accept only retries persistence if needed.
        const auto attempt = value(backend, backend.submitQuiz());
        std::cout << "Điểm: " << attempt.percentage() << "%\n";
        accept(backend, backend.close());

        Application imported;
        accept(imported, imported.initialize((root / "imported").string(), "admin", "demo123"));
        accept(imported, imported.login("admin", "demo123"));
        accept(imported, imported.importPackage((root / "package").string()));
        std::cout << "Đã nhập: " << value(imported, imported.statistics()).vocabulary << " từ vựng\n";
        accept(imported, imported.close());
        std::filesystem::remove_all(root);
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << "\nDữ liệu thử được giữ ở: " << root.string() << '\n';
        return 1;
    }
}
