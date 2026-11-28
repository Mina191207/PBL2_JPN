#include "BackendScreen.h"
#include "UITheme.h"

#include <algorithm>
#include <type_traits>

namespace {
constexpr Color kBorder = UITheme::border;
constexpr Color kText = UITheme::text;
constexpr Color kMuted = UITheme::muted;
constexpr float kActionButtonWidth = 170.0f;

Rectangle action(float x, float y, float width = kActionButtonWidth, float height = 40.0f) {
    return {x, y, width, height};
}

using Event = BackendScreenEvent::Type;
Rectangle quizLevelBounds(Rectangle list) {
    return {list.x + 12.0f, list.y + 10.0f, std::min(250.0f, list.width * 0.48f), 38.0f};
}

Rectangle quizViewport(Rectangle list, bool submitted) {
    return {list.x + 12.0f, list.y + 48.0f, list.width - 24.0f,
            std::max(1.0f, list.height - (submitted ? 60.0f : 100.0f))};
}

Rectangle quizNav(Rectangle list, bool next) {
    return {next ? list.x + list.width - 124.0f : list.x + 12.0f,
            list.y + list.height - 44.0f, 112.0f, 36.0f};
}

struct QuizQuestionLayout {
    UITheme::WrappedText prompt;
    std::array<UITheme::WrappedText, 4> labels;
    std::array<Rectangle, 4> options;
    float height;
};

QuizQuestionLayout quizQuestionLayout(Rectangle viewport, const QuizQuestionView& question) {
    QuizQuestionLayout ui{UITheme::wrap(question.prompt, viewport.width - 12.0f, 20), {}, {}, 0};
    float y = viewport.y + ui.prompt.height() + 10.0f;
    for (std::size_t i = 0; i < ui.labels.size(); ++i) {
        ui.labels[i] = UITheme::wrap(std::string(1, static_cast<char>('A' + i)) + ". " + question.options[i],
                                     viewport.width - 36.0f, 17);
        ui.options[i] = {viewport.x + 2.0f, y, viewport.width - 12.0f, ui.labels[i].height() + 14.0f};
        y += ui.options[i].height + 8.0f;
    }
    ui.height = y - viewport.y;
    return ui;
}

struct QuizResultCard {
    bool correct;
    std::vector<UITheme::WrappedText> fields;
    float height;
};

std::vector<QuizResultCard> quizResultCards(Rectangle viewport, const BackendActivityState& activity) {
    std::vector<QuizResultCard> cards;
    for (std::size_t i = 0; i < activity.quizAnswers.size(); ++i) {
        const auto& answer = activity.quizAnswers[i];
        const auto* question = i < activity.quiz->questions.size() ? &activity.quiz->questions[i] : nullptr;
        const auto answerText = [&](char letter) {
            if (letter == '-') return std::string("Not answered");
            std::string value(1, letter);
            if (question && letter >= 'A' && letter <= 'D') value += ". " + question->options[letter - 'A'];
            return value;
        };
        QuizResultCard card{answer.selectedAnswer == answer.correctAnswer, {}, 12.0f};
        const auto field = [&](const std::string& value, int size) {
            card.fields.push_back(UITheme::wrap(value, viewport.width - 40.0f, size));
            card.height += card.fields.back().height() + 8.0f;
        };
        field("Question " + std::to_string(i + 1), 15);
        if (question) field(question->prompt, 19);
        field("Your answer: " + answerText(answer.selectedAnswer), 17);
        field("Correct answer: " + answerText(answer.correctAnswer), 17);
        field("Explanation: " + answer.explanation, 17);
        cards.push_back(std::move(card));
    }
    return cards;
}

float quizResultHeight(const std::vector<QuizResultCard>& cards) {
    float height = 0;
    for (const auto& card : cards) height += card.height + 8.0f;
    return height;
}

bool quizHasWrongAnswers(const BackendActivityState& activity) {
    const bool answerDetailsContainWrong = std::any_of(activity.quizAnswers.begin(), activity.quizAnswers.end(),
        [](const QuizAnswerDetail& answer) { return answer.selectedAnswer != answer.correctAnswer; });
    const bool submittedScoreContainsWrong = activity.quizResult &&
        activity.quizResult->getCorrectAnswers() < activity.quizResult->getTotalQuestions();
    return answerDetailsContainWrong || submittedScoreContainsWrong;
}

std::string partOfSpeechLabel(PartOfSpeech value) {
    switch (value) {
    case PartOfSpeech::NOUN: return "Noun";
    case PartOfSpeech::VERB: return "Verb";
    case PartOfSpeech::IADJECTIVE: return "i-adjective";
    case PartOfSpeech::NAADJECTIVE: return "na-adjective";
    case PartOfSpeech::ADVERB: return "Adverb";
    case PartOfSpeech::OTHER: return "Other";
    }
    return {};
}

void addReviewField(std::vector<std::pair<std::string, std::string>>& fields,
                    const std::string& label, const std::string& value) {
    if (!value.empty()) fields.emplace_back(label, value);
}

struct ReviewDisplayDetails {
    std::vector<std::pair<std::string, std::string>> fields;
    std::string example;
    std::string exampleLabel = "Example";
};

ReviewDisplayDetails reviewDisplayDetails(const BackendActivityState& activity, const ReviewView& card) {
    ReviewDisplayDetails details;
    if (activity.reviewResource) {
        std::visit([&](const auto& resource) {
            addReviewField(details.fields, "Meaning", resource.getMeaning());
            using T = std::decay_t<decltype(resource)>;
            if constexpr (std::is_same_v<T, Vocabulary>) {
                addReviewField(details.fields, "Reading", resource.getReading());
                addReviewField(details.fields, "Part of speech", partOfSpeechLabel(resource.getPartOfSpeech()));
                details.example = resource.getExample();
            } else if constexpr (std::is_same_v<T, Grammar>) {
                addReviewField(details.fields, "Structure", resource.getStructure());
                addReviewField(details.fields, "Usage", resource.getUsage());
                details.example = resource.getExample();
            } else {
                addReviewField(details.fields, "Onyomi", resource.getOnyomi());
                addReviewField(details.fields, "Kunyomi", resource.getKunyomi());
                details.exampleLabel = "Examples";
                details.example = resource.getExamples();
            }
        }, *activity.reviewResource);
        return details;
    }

    if (!card.back) return details;
    std::size_t start = 0;
    while (start <= card.back->size()) {
        const std::size_t end = card.back->find('\n', start);
        const std::string line = card.back->substr(start,
            end == std::string::npos ? std::string::npos : end - start);
        const std::size_t separator = line.find(':');
        if (separator == std::string::npos) addReviewField(details.fields, "Meaning", line);
        else {
            const std::string label = line.substr(0, separator);
            const std::string value = line.substr(separator + 1);
            if (label == "Example" || label == "Examples") { details.exampleLabel = label; details.example = value; }
            else addReviewField(details.fields, label, value);
        }
        if (end == std::string::npos) break;
        start = end + 1;
    }
    return details;
}

Rectangle reviewDetailsViewport(Rectangle list) {
    const float top = list.y + 154.0f;
    const float bottom = list.y + list.height - 52.0f;
    return {list.x + 18.0f, top, list.width - 36.0f, std::max(18.0f, bottom - top)};
}

std::vector<float> reviewFieldRowHeights(const ReviewDisplayDetails& details, float width) {
    const float tileWidth = (width - 10.0f) * 0.5f;
    const std::size_t rows = (details.fields.size() + 1) / 2;
    std::vector<float> heights(rows, 62.0f);
    for (std::size_t index = 0; index < details.fields.size(); ++index) {
        const auto value = UITheme::wrap(details.fields[index].second, tileWidth - 20.0f, 15);
        heights[index / 2] = std::max(heights[index / 2], 27.0f + value.height());
    }
    return heights;
}

float reviewDetailsContentHeight(const ReviewDisplayDetails& details, float width) {
    const auto rowHeights = reviewFieldRowHeights(details, width);
    float height = 0.0f;
    for (const float rowHeight : rowHeights) height += rowHeight;
    if (rowHeights.size() > 1) height += static_cast<float>(rowHeights.size() - 1) * 8.0f;
    if (!details.example.empty()) {
        const auto example = UITheme::wrap(details.example, width - 32.0f, 16);
        height += (height > 0.0f ? 8.0f : 0.0f) + 36.0f + example.height();
    }
    return height;
}

} // namespace

void BackendScreen::addStudyControls(std::vector<int>& order, Section section, const BackendActivityState& activity) const {
    const auto add = [&](Event event) { order.push_back(actionControl(event)); };
    if (section == Section::Review) {
        const auto& state = activity.review;
        if (!state || state->finished) add(Event::START_REVIEW);
        else if (state->current) {
            if (state->current->back) { add(Event::RATE_KNOWN); add(Event::RATE_UNKNOWN); }
            else add(Event::REVEAL_REVIEW);
            add(Event::STOP_REVIEW);
        }
    } else if (section == Section::Quiz) {
        if (!activity.quiz) {
            if (activity.quizLevels.size() > 1) order.push_back(QuizLevel);
            if (activity.activeQuestionCount > 0) add(Event::START_QUIZ);
        } else {
            if (!activity.quiz->submitted) {
                if (!activity.quiz->questions.empty()) {
                    order.insert(order.end(), {QuizText, QuizA, QuizB, QuizC, QuizD});
                    if (quizQuestionIndex_ > 0) order.push_back(QuizPrevious);
                    if (quizQuestionIndex_ + 1 < activity.quiz->questions.size()) order.push_back(QuizNext);
                }
                add(Event::SUBMIT_QUIZ);
            } else {
                order.push_back(QuizText);
                if (quizHasWrongAnswers(activity)) add(Event::RETRY_WRONG);
            }
            add(Event::STOP_QUIZ);
        }
    }
}

BackendScreenEvent BackendScreen::updateReview(Rectangle list, float x, float y, const BackendActivityState& activity) {
    const auto& state = activity.review;
    const bool active = state && !state->finished && state->current.has_value();
    const bool revealed = active && state->current->back.has_value();
    if (revealed) {
        const auto details = reviewDisplayDetails(activity, *state->current);
        const Rectangle viewport = reviewDetailsViewport(list);
        const float maxScroll = std::max(0.0f, reviewDetailsContentHeight(details, viewport.width) - viewport.height);
        const float wheel = CheckCollisionPointRec(GetMousePosition(), viewport) ? GetMouseWheelMove() : 0.0f;
        reviewDetailsScrollOffset_ = std::clamp(reviewDetailsScrollOffset_ - wheel * 48.0f, 0.0f, maxScroll);
    } else {
        reviewDetailsScrollOffset_ = 0.0f;
    }

    Event event = Event::NONE;
    if ((!state || state->finished) && focus_.activate(actionControl(Event::START_REVIEW), action(x, y))) event = Event::START_REVIEW;
    else if (active && !revealed && focus_.activate(actionControl(Event::REVEAL_REVIEW), action(x, y + 48))) event = Event::REVEAL_REVIEW;
    else if (revealed && focus_.activate(actionControl(Event::RATE_KNOWN), action(x, y + 96, 80))) event = Event::RATE_KNOWN;
    else if (revealed && focus_.activate(actionControl(Event::RATE_UNKNOWN), action(x + 90, y + 96, 80))) event = Event::RATE_UNKNOWN;
    else if (active && focus_.activate(actionControl(Event::STOP_REVIEW), action(x, y + 144))) event = Event::STOP_REVIEW;
    if (event != Event::NONE) reviewDetailsScrollOffset_ = 0.0f;
    return {event};
}

void BackendScreen::revealQuizOption(Rectangle list, const BackendActivityState& activity) {
    if (!activity.quiz || activity.quiz->submitted || activity.quiz->questions.empty() ||
        focus_.id < QuizA || focus_.id > QuizD) return;
    const auto viewport = quizViewport(list, false);
    const auto ui = quizQuestionLayout(viewport, activity.quiz->questions[
        std::min(quizQuestionIndex_, activity.quiz->questions.size() - 1)]);
    const auto option = ui.options[focus_.id - QuizA];
    const float top = option.y - viewport.y;
    const float bottom = top + option.height;
    if (top < quizOffset_ || option.height > viewport.height) quizOffset_ = top;
    else if (bottom > quizOffset_ + viewport.height) quizOffset_ = bottom - viewport.height;
    quizOffset_ = std::min(quizOffset_, std::max(0.0f, ui.height - viewport.height));
}

BackendScreenEvent BackendScreen::updateQuiz(Rectangle list, float x, float y,
                                             const BackendActivityState& activity) {
    const bool hasSession = activity.quiz.has_value();
    const bool active = hasSession && !activity.quiz->submitted;
    const bool submitted = hasSession && activity.quiz->submitted;
    const auto viewport = quizViewport(list, submitted);
    if (!hasSession && activity.quizLevels.size() > 1 && focus_.activate(QuizLevel, quizLevelBounds(list))) {
        const auto current = std::find_if(activity.quizLevels.begin(), activity.quizLevels.end(),
            [&](const Level& level) { return activity.selectedQuizLevel && level.getId() == *activity.selectedQuizLevel; });
        const auto index = current == activity.quizLevels.end() ? 0 : current - activity.quizLevels.begin();
        return {Event::SELECT_QUIZ_LEVEL, activity.quizLevels[(index + 1) % activity.quizLevels.size()].getId()};
    }
    if (active && !activity.quiz->questions.empty()) {
        quizQuestionIndex_ = std::min(quizQuestionIndex_, activity.quiz->questions.size() - 1);
        if (quizViewportSize_.x != viewport.width || quizViewportSize_.y != viewport.height) {
            quizViewportSize_ = {viewport.width, viewport.height};
            revealQuizOption(list, activity);
        }
        const auto ui = quizQuestionLayout(viewport, activity.quiz->questions[quizQuestionIndex_]);
        quizOffset_ = UiInput::scroll(viewport, ui.height, quizOffset_,
            focus_.on(QuizText) || (focus_.id >= QuizA && focus_.id <= QuizD));
        for (std::size_t option = 0; option < ui.options.size(); ++option) {
            auto bounds = ui.options[option];
            bounds.y -= quizOffset_;
            const auto visible = GetCollisionRec(bounds, viewport);
            if (visible.height > 0 && focus_.activate(QuizA + static_cast<int>(option), visible)) {
                BackendScreenEvent event(Event::ANSWER);
                event.questionIndex = quizQuestionIndex_;
                event.answer = static_cast<char>('A' + option);
                return event;
            }
        }
        if (click(viewport)) focus_.id = QuizText;
        if (quizQuestionIndex_ > 0 && focus_.activate(QuizPrevious, quizNav(list, false))) {
            --quizQuestionIndex_;
            quizOffset_ = 0;
            focus_.id = QuizText;
        } else if (quizQuestionIndex_ + 1 < activity.quiz->questions.size() &&
                   focus_.activate(QuizNext, quizNav(list, true))) {
            ++quizQuestionIndex_;
            quizOffset_ = 0;
            focus_.id = QuizText;
        }
    }
    if (!hasSession && focus_.activate(actionControl(Event::START_QUIZ), action(x, y), activity.activeQuestionCount > 0)) {
        quizQuestionIndex_ = 0;
        quizOffset_ = 0;
        focus_.id = QuizText;
        return {Event::START_QUIZ};
    }
    if (active && focus_.activate(actionControl(Event::SUBMIT_QUIZ), action(x, y))) {
        quizResultOffset_ = 0;
        const auto& answers = activity.quiz->selectedAnswers;
        const auto blank = std::count(answers.begin(), answers.end(), '-');
        if (blank == 0) {
            focus_.id = QuizText;
            return {Event::SUBMIT_QUIZ};
        }
        confirm({Event::SUBMIT_QUIZ}, "Submit quiz?",
                std::to_string(blank) + (blank == 1 ? " question is" : " questions are") +
                    " unanswered. Submit this attempt?");
        return {};
    }
    if (submitted) {
        if (click(viewport)) focus_.id = QuizText;
        const auto cards = quizResultCards(viewport, activity);
        quizResultOffset_ = UiInput::scroll(viewport, quizResultHeight(cards), quizResultOffset_, focus_.on(QuizText));
        if (focus_.activate(actionControl(Event::RETRY_WRONG), action(x, y), quizHasWrongAnswers(activity))) {
            quizQuestionIndex_ = 0;
            quizOffset_ = 0;
            focus_.id = QuizText;
            return {Event::RETRY_WRONG};
        }
    }
    if (hasSession && focus_.activate(actionControl(Event::STOP_QUIZ), action(x + 180, y)))
        return {Event::STOP_QUIZ};
    return {};
}

void BackendScreen::drawReviewPage(Rectangle list, const BackendActivityState& activity, float scrollOffset) const {
    DrawRectangleRounded(list,0.04f,8,Color{255,252,253,255});
    DrawRectangleRoundedLinesEx(list,0.04f,8,1.0f,kBorder);
    if (!activity.review) {
        UITheme::drawWrapped("No review session is active. Start a review to load cards from your favorites.",
            static_cast<int>(list.x + 24.0f), static_cast<int>(list.y + 34.0f),
            static_cast<int>(list.width - 48.0f), 19, kMuted, 4);
    } else if (activity.review->finished) {
        const char* endText = activity.reviewStopped ? "Review stopped" : "Review complete";
        UITheme::drawText(endText, static_cast<int>(list.x + 24.0f), static_cast<int>(list.y + 34.0f), 26, kText);
        UITheme::drawWrapped(TextFormat("Ratings recorded: %zu", activity.review->ratingCount),
            static_cast<int>(list.x + 24.0f), static_cast<int>(list.y + 82.0f),
            static_cast<int>(list.width - 48.0f), 18, kMuted, 3);
    } else if (activity.review->current) {
        const auto& card = *activity.review->current;
        UITheme::drawText(TextFormat("Review step %zu", activity.review->ratingCount + 1),
            static_cast<int>(list.x + 24.0f), static_cast<int>(list.y + 22.0f), 16, kMuted);
        UITheme::drawWrapped(card.front, static_cast<int>(list.x + 24.0f), static_cast<int>(list.y + 64.0f),
            static_cast<int>(list.width - 48.0f), 34, UITheme::strongText, 2);
        UITheme::drawText(TextFormat("%zu remaining", activity.review->remaining),
            static_cast<int>(list.x + 24.0f), static_cast<int>(list.y + list.height - 38.0f), 16, kMuted);
        if (card.back) {
            const auto details = reviewDisplayDetails(activity, card);
            const Rectangle viewport = reviewDetailsViewport(list);
            const float contentHeight = reviewDetailsContentHeight(details, viewport.width);
            const float maxScroll = std::max(0.0f, contentHeight - viewport.height);
            const float scroll = std::clamp(scrollOffset, 0.0f, maxScroll);
            const float columnGap = 10.0f;
            const float rowGap = 8.0f;
            const float tileWidth = (viewport.width - columnGap) * 0.5f;
            const std::size_t fieldRows = (details.fields.size() + 1) / 2;
            const auto rowHeights = reviewFieldRowHeights(details, viewport.width);
            BeginScissorMode(static_cast<int>(viewport.x), static_cast<int>(viewport.y),
                std::max(1, static_cast<int>(viewport.width - 8.0f)), std::max(1, static_cast<int>(viewport.height)));
            float rowY = viewport.y;
            for (std::size_t index = 0; index < details.fields.size(); ++index) {
                const std::size_t row = index / 2;
                const std::size_t column = index % 2;
                if (column == 0 && row > 0) rowY += rowHeights[row - 1] + rowGap;
                const Rectangle tile{viewport.x + static_cast<float>(column) * (tileWidth + columnGap),
                    rowY - scroll, tileWidth, rowHeights[row]};
                DrawRectangleRounded(tile, 0.08f, 7, RAYWHITE);
                DrawRectangleRoundedLinesEx(tile, 0.08f, 7, 1.0f, kBorder);
                UITheme::drawText(details.fields[index].first.c_str(), static_cast<int>(tile.x + 10.0f),
                    static_cast<int>(tile.y + 6.0f), 13, kMuted);
                UITheme::drawWrapped(details.fields[index].second, static_cast<int>(tile.x + 10.0f),
                    static_cast<int>(tile.y + 25.0f), static_cast<int>(tile.width - 20.0f), 15, kText);
            }
            float exampleY = viewport.y;
            for (const float rowHeight : rowHeights) exampleY += rowHeight + rowGap;
            if (fieldRows > 0) exampleY -= rowGap;
            if (!details.example.empty()) {
                const auto example = UITheme::wrap(details.example, viewport.width - 32.0f, 16);
                const float exampleHeight = 36.0f + example.height();
                const Rectangle exampleBounds{viewport.x, exampleY - scroll, viewport.width - 8.0f, exampleHeight};
                DrawRectangleRounded(exampleBounds, 0.08f, 7, Color{255,239,245,255});
                UITheme::drawText(details.exampleLabel.c_str(), static_cast<int>(exampleBounds.x + 12.0f),
                    static_cast<int>(exampleBounds.y + 8.0f), 13, kMuted);
                example.draw(static_cast<int>(exampleBounds.x + 12.0f),
                    static_cast<int>(exampleBounds.y + 28.0f), kText);
            }
            EndScissorMode();
            if (maxScroll > 0.0f) {
                const Rectangle track{viewport.x + viewport.width - 4.0f, viewport.y, 3.0f, viewport.height};
                DrawRectangleRounded(track, 0.5f, 3, Color{239,224,230,255});
                const float thumbHeight = std::max(20.0f, track.height * viewport.height / contentHeight);
                DrawRectangleRounded({track.x, track.y + (track.height - thumbHeight) * scroll / maxScroll,
                    track.width, thumbHeight}, 0.5f, 3, kBorder);
            }
        } else {
            UITheme::drawWrapped("Reveal the answer before marking this card Known or Again.",
                static_cast<int>(list.x + 24.0f), static_cast<int>(list.y + list.height * 0.60f),
                static_cast<int>(list.width - 48.0f), 16, kMuted, 3);
        }
    }

}

void BackendScreen::drawQuizPage(Rectangle list, const BackendActivityState& activity) const {
    DrawRectangleRounded(list, 0.04f, 8, Color{255,252,253,255});
    DrawRectangleRoundedLinesEx(list, 0.04f, 8, 1.0f, kBorder);
    const bool submitted = activity.quiz && activity.quiz->submitted;
    const auto viewport = quizViewport(list, submitted);
    const auto x = static_cast<int>(list.x + 16.0f);
    const auto headingY = static_cast<int>(list.y + 12.0f);
    if (submitted) {
        const char* heading = activity.quizResult ? TextFormat("Score: %d / %d  (%.0f%%)",
            activity.quizResult->getCorrectAnswers(), activity.quizResult->getTotalQuestions(),
            activity.quizResult->percentage()) : "Quiz submitted";
        UITheme::drawText(heading, x, headingY, 22, UITheme::strongText);
        const auto cards = quizResultCards(viewport, activity);
        const float height = quizResultHeight(cards);
        const float offset = std::min(quizResultOffset_, std::max(0.0f, height - viewport.height));
        UITheme::beginClip(viewport);
        float y = viewport.y - offset;
        for (const auto& card : cards) {
            if (y + card.height > viewport.y && y < viewport.y + viewport.height) {
                DrawRectangleRounded({viewport.x + 2.0f, y, viewport.width - 12.0f, card.height}, 0.06f, 6,
                    card.correct ? Color{241,250,244,255} : Color{255,235,242,255});
                float textY = y + 10.0f;
                for (const auto& field : card.fields) {
                    field.draw(x + 8, static_cast<int>(textY), kText);
                    textY += field.height() + 8.0f;
                }
            }
            y += card.height + 8.0f;
        }
        EndScissorMode();
        UITheme::drawScrollbar(viewport, height, offset);
    } else if (activity.quiz && !activity.quiz->questions.empty()) {
        const auto index = std::min(quizQuestionIndex_, activity.quiz->questions.size() - 1);
        const auto& selections = activity.quiz->selectedAnswers;
        const auto answered = selections.size() - std::count(selections.begin(), selections.end(), '-');
        UITheme::drawText(TextFormat("Question %zu of %zu  |  Answered %zu", index + 1,
            activity.quiz->questions.size(), answered), x, headingY, 19, kMuted);
        const auto ui = quizQuestionLayout(viewport, activity.quiz->questions[index]);
        const float offset = std::min(quizOffset_, std::max(0.0f, ui.height - viewport.height));
        UITheme::beginClip(viewport);
        ui.prompt.draw(x, static_cast<int>(viewport.y - offset), kText);
        for (std::size_t option = 0; option < ui.options.size(); ++option) {
            auto bounds = ui.options[option];
            bounds.y -= offset;
            if (!CheckCollisionRecs(viewport, bounds)) continue;
            const bool chosen = index < selections.size() && selections[index] == static_cast<char>('A' + option);
            const bool hovered = CheckCollisionPointRec(GetMousePosition(), GetCollisionRec(bounds, viewport));
            DrawRectangleRounded(bounds, 0.12f, 7, chosen ? Color{255,224,234,255} : hovered ? UITheme::hover : RAYWHITE);
            DrawRectangleRoundedLinesEx(bounds, 0.12f, 7, chosen ? 2.0f : 1.0f, chosen ? UITheme::primary : kBorder);
            ui.labels[option].draw(static_cast<int>(bounds.x + 12.0f), static_cast<int>(bounds.y + 7.0f), kText);
            if (!modalOpen() && focus_.on(QuizA + static_cast<int>(option))) UITheme::drawFocus(bounds);
        }
        EndScissorMode();
        UITheme::drawScrollbar(viewport, ui.height, offset);
        controlButton(QuizPrevious, quizNav(list, false), "Previous", false, index > 0);
        controlButton(QuizNext, quizNav(list, true), "Next", false, index + 1 < activity.quiz->questions.size());
    } else {
        const auto level = quizLevelBounds(list);
        std::string name = "Select JLPT level";
        if (activity.selectedQuizLevel) {
            const auto found = std::find_if(activity.quizLevels.begin(), activity.quizLevels.end(), [&](const Level& value) {
                return value.getId() == *activity.selectedQuizLevel;
            });
            name = found == activity.quizLevels.end() ? activity.selectedQuizLevel->getId() : found->getName();
        }
        controlButton(QuizLevel, level, name.c_str(), false, activity.quizLevels.size() > 1);
        UITheme::drawText(TextFormat("%zu active questions", activity.activeQuestionCount),
            static_cast<int>(level.x + level.width + 14.0f), static_cast<int>(level.y + 10.0f), 16, kMuted);
        const char* hint = activity.quizLevels.empty() ? "No JLPT levels are available for a quiz." :
            activity.activeQuestionCount == 0 ? "This level has no active questions. Choose another level." :
            "This level is ready. Start the quiz when you are ready.";
        UITheme::drawWrapped(hint, x, static_cast<int>(list.y + 60.0f), static_cast<int>(list.width - 36.0f), 18, kMuted);
    }
    if (!modalOpen() && focus_.on(QuizText)) UITheme::drawFocus(viewport);
}

void BackendScreen::drawStudyActions(Section section, float x, float y, const BackendActivityState& activity) const {
    const bool reviewPage = section == Section::Review;
    const bool quizPage = section == Section::Quiz;
    if (reviewPage) {
        const bool active = activity.review && !activity.review->finished && activity.review->current.has_value();
        const bool revealed = active && activity.review->current->back.has_value();
        controlButton(actionControl(Event::START_REVIEW), action(x,y),activity.review&& !activity.review->finished ? "Review active" : "Start review",
               !activity.review || activity.review->finished, !activity.review || activity.review->finished);
        if (active && !revealed) controlButton(actionControl(Event::REVEAL_REVIEW), action(x,y+48),"Reveal answer",true,true);
        if (active && revealed) {
            controlButton(actionControl(Event::RATE_KNOWN), action(x,y+96,80,40),"Known",true,true);
            controlButton(actionControl(Event::RATE_UNKNOWN), action(x+90,y+96,80,40),"Again",false,true);
        }
        if (active) controlButton(actionControl(Event::STOP_REVIEW), action(x,y+144),"Stop review",false,true);
    } else if (quizPage) {
        if (!activity.quiz) {
            controlButton(actionControl(Event::START_QUIZ), action(x,y), activity.activeQuestionCount > 0
                ? TextFormat("Start quiz (%zu)", std::min<std::size_t>(10, activity.activeQuestionCount))
                : "No active questions", true, activity.activeQuestionCount > 0);
        } else {
            if (!activity.quiz->submitted)
                controlButton(actionControl(Event::SUBMIT_QUIZ), action(x,y), "Submit quiz", true);
            else if (quizHasWrongAnswers(activity))
                controlButton(actionControl(Event::RETRY_WRONG), action(x,y), "Retry wrong", true);
            controlButton(actionControl(Event::STOP_QUIZ), action(x + 180,y), "Stop quiz");
        }
    }
}
