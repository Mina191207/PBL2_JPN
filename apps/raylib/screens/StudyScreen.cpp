#include "BackendScreen.h"
#include "UITheme.h"

#include <algorithm>
#include <sstream>
#include <type_traits>

namespace {
constexpr Color kBorder = UITheme::border;
constexpr Color kText = UITheme::text;
constexpr Color kMuted = UITheme::muted;
constexpr float kActionButtonWidth = 170.0f;
constexpr float kQuizAnswerRowHeight = 128.0f;

Rectangle action(float x, float y, float width = kActionButtonWidth, float height = 40.0f) {
    return {x, y, width, height};
}

using Event = BackendScreenEvent::Type;
struct QuizUiLayout {
    Rectangle level;
    Rectangle options[4];
    Rectangle previous;
    Rectangle next;
};

QuizUiLayout quizLayout(Rectangle list) {
    QuizUiLayout result{};
    result.level = {list.x + 12.0f, list.y + 10.0f, std::min(250.0f, list.width * 0.48f), 38.0f};
    const float optionGap = 10.0f;
    const float optionWidth = (list.width - 36.0f - optionGap) * 0.5f;
    const float optionY = list.y + (list.height < 350.0f ? 136.0f : 162.0f);
    const float navY = list.y + list.height - 48.0f;
    const float optionHeight = std::clamp((navY - optionY - 2.0f * optionGap) * 0.5f, 24.0f, 64.0f);
    const float secondRowY = optionY + optionHeight + optionGap;
    const float left = list.x + 12.0f;
    const float right = left + optionWidth + optionGap;
    result.options[0] = {left, optionY, optionWidth, optionHeight};
    result.options[1] = {right, optionY, optionWidth, optionHeight};
    result.options[2] = {left, secondRowY, optionWidth, optionHeight};
    result.options[3] = {right, secondRowY, optionWidth, optionHeight};
    result.previous = {left, navY, 112.0f, 36.0f};
    result.next = {list.x + list.width - 124.0f, navY, 112.0f, 36.0f};
    return result;
}

bool quizHasWrongAnswers(const BackendActivityState& activity) {
    const bool answerDetailsContainWrong = std::any_of(activity.quizAnswers.begin(), activity.quizAnswers.end(),
        [](const QuizAnswerDetail& answer) { return answer.selectedAnswer != answer.correctAnswer; });
    const bool submittedScoreContainsWrong = activity.quizResult &&
        activity.quizResult->getCorrectAnswers() < activity.quizResult->getTotalQuestions();
    return answerDetailsContainWrong || submittedScoreContainsWrong;
}

std::size_t visibleQuizAnswers(Rectangle list) {
    return static_cast<std::size_t>(std::max(1.0f, (list.height - 112.0f) / kQuizAnswerRowHeight));
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

std::size_t wrappedLineCount(const std::string& value, int width, int fontSize) {
    std::size_t lines = 0;
    std::string line;
    std::istringstream words(value);
    std::string token;
    while (words >> token) {
        const std::string candidate = line.empty() ? token : line + " " + token;
        if (UITheme::measureText(candidate.c_str(), fontSize) <= width) {
            line = candidate;
            continue;
        }
        if (!line.empty()) { ++lines; line.clear(); }
        if (UITheme::measureText(token.c_str(), fontSize) <= width) { line = token; continue; }
        for (const auto& unit : Utf8::decode(token)) {
            const std::string character = token.substr(unit.begin, unit.end - unit.begin);
            if (!line.empty() && UITheme::measureText((line + character).c_str(), fontSize) > width) {
                ++lines;
                line.clear();
            }
            line += character;
        }
    }
    if (!line.empty()) ++lines;
    return std::max<std::size_t>(1, lines);
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
        const auto lines = wrappedLineCount(details.fields[index].second,
            std::max(1, static_cast<int>(tileWidth - 20.0f)), 15);
        heights[index / 2] = std::max(heights[index / 2], 27.0f + static_cast<float>(lines) * 20.0f);
    }
    return heights;
}

float reviewDetailsContentHeight(const ReviewDisplayDetails& details, float width) {
    const auto rowHeights = reviewFieldRowHeights(details, width);
    float height = 0.0f;
    for (const float rowHeight : rowHeights) height += rowHeight;
    if (rowHeights.size() > 1) height += static_cast<float>(rowHeights.size() - 1) * 8.0f;
    if (!details.example.empty()) {
        const auto lines = wrappedLineCount(details.example, std::max(1, static_cast<int>(width - 24.0f)), 16);
        height += (height > 0.0f ? 8.0f : 0.0f) + 36.0f + static_cast<float>(lines) * 21.0f;
    }
    return height;
}

} // namespace

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
    if ((!state || state->finished) && click(action(x, y))) event = Event::START_REVIEW;
    else if (active && !revealed && click(action(x, y + 48))) event = Event::REVEAL_REVIEW;
    else if (revealed && click(action(x, y + 96, 80))) event = Event::RATE_KNOWN;
    else if (revealed && click(action(x + 90, y + 96, 80))) event = Event::RATE_UNKNOWN;
    else if (active && click(action(x, y + 144))) event = Event::STOP_REVIEW;
    if (event != Event::NONE) reviewDetailsScrollOffset_ = 0.0f;
    return {event};
}

BackendScreenEvent BackendScreen::updateQuiz(Rectangle list, float x, float y,
                                             const BackendActivityState& activity) {
    const auto quizUi = quizLayout(list);
    const bool hasSession = activity.quiz.has_value();
    const bool active = hasSession && !activity.quiz->submitted;
    const bool submitted = hasSession && activity.quiz->submitted;

    if (!hasSession && activity.quizLevels.size() > 1 && click(quizUi.level)) {
        const auto current = std::find_if(activity.quizLevels.begin(), activity.quizLevels.end(),
            [&](const Level& level) { return activity.selectedQuizLevel && level.getId() == *activity.selectedQuizLevel; });
        const std::size_t currentIndex = current == activity.quizLevels.end()
            ? 0 : static_cast<std::size_t>(std::distance(activity.quizLevels.begin(), current));
        const Level& next = activity.quizLevels[(currentIndex + 1) % activity.quizLevels.size()];
        return {Event::SELECT_QUIZ_LEVEL, next.getId()};
    }
    if (active && !activity.quiz->questions.empty()) {
        quizQuestionIndex_ = std::min(quizQuestionIndex_, activity.quiz->questions.size() - 1);
        for (std::size_t option = 0; option < 4; ++option) {
            if (click(quizUi.options[option])) {
                BackendScreenEvent event(Event::ANSWER);
                event.questionIndex = quizQuestionIndex_;
                event.answer = static_cast<char>('A' + option);
                return event;
            }
        }
        if (quizQuestionIndex_ > 0 && click(quizUi.previous)) --quizQuestionIndex_;
        else if (quizQuestionIndex_ + 1 < activity.quiz->questions.size() && click(quizUi.next)) ++quizQuestionIndex_;
    }
    if (!hasSession && activity.activeQuestionCount > 0 && click(action(x, y))) {
        quizQuestionIndex_ = 0;
        return {BackendScreenEvent::Type::START_QUIZ};
    }
    if (active && click(action(x, y + 48.0f))) return {BackendScreenEvent::Type::SUBMIT_QUIZ};
    const bool hasWrongAnswers = quizHasWrongAnswers(activity);
    if (submitted && hasWrongAnswers && click(action(x, y + 48.0f))) {
        quizQuestionIndex_ = 0;
        return {BackendScreenEvent::Type::RETRY_WRONG};
    }
    if (hasSession && click(action(x, y + (submitted && !hasWrongAnswers ? 120.0f : 96.0f))))
        return {BackendScreenEvent::Type::STOP_QUIZ};
    if (submitted && !activity.quizAnswers.empty() && CheckCollisionPointRec(GetMousePosition(), list)) {
        const float wheel = GetMouseWheelMove();
        const std::size_t visible = visibleQuizAnswers(list);
        const std::size_t maxOffset = activity.quizAnswers.size() > visible
            ? activity.quizAnswers.size() - visible : 0;
        if (wheel > 0.0f && scrollOffset_ > 0) --scrollOffset_;
        if (wheel < 0.0f && scrollOffset_ < maxOffset) ++scrollOffset_;
        scrollOffset_ = std::min(scrollOffset_, maxOffset);
    }
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
                    static_cast<int>(tile.y + 25.0f), static_cast<int>(tile.width - 20.0f), 15, kText,
                    static_cast<int>(wrappedLineCount(details.fields[index].second,
                        std::max(1, static_cast<int>(tile.width - 20.0f)), 15)));
            }
            float exampleY = viewport.y;
            for (const float rowHeight : rowHeights) exampleY += rowHeight + rowGap;
            if (fieldRows > 0) exampleY -= rowGap;
            if (!details.example.empty()) {
                const std::size_t lines = wrappedLineCount(details.example,
                    std::max(1, static_cast<int>(viewport.width - 24.0f)), 16);
                const float exampleHeight = 36.0f + static_cast<float>(lines) * 21.0f;
                const Rectangle exampleBounds{viewport.x, exampleY - scroll, viewport.width - 8.0f, exampleHeight};
                DrawRectangleRounded(exampleBounds, 0.08f, 7, Color{255,239,245,255});
                UITheme::drawText(details.exampleLabel.c_str(), static_cast<int>(exampleBounds.x + 12.0f),
                    static_cast<int>(exampleBounds.y + 8.0f), 13, kMuted);
                UITheme::drawWrapped(details.example, static_cast<int>(exampleBounds.x + 12.0f),
                    static_cast<int>(exampleBounds.y + 28.0f), static_cast<int>(exampleBounds.width - 24.0f), 16, kText,
                    static_cast<int>(lines));
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

void BackendScreen::drawQuizPage(Rectangle list, const BackendActivityState& activity, std::size_t questionIndex,
                                 std::size_t scrollOffset) const {
    DrawRectangleRounded(list,0.04f,8,Color{255,252,253,255});
    DrawRectangleRoundedLinesEx(list,0.04f,8,1.0f,kBorder);
    const auto quizUi = quizLayout(list);
    const bool hasSession = activity.quiz.has_value();
    const bool active = hasSession && !activity.quiz->submitted;
    const bool submitted = hasSession && activity.quiz->submitted;
    std::string selectedLevelName = "Select JLPT level";
    if (activity.selectedQuizLevel) {
        const auto found = std::find_if(activity.quizLevels.begin(), activity.quizLevels.end(), [&](const Level& level) {
            return level.getId() == *activity.selectedQuizLevel;
        });
        selectedLevelName = found == activity.quizLevels.end() ? activity.selectedQuizLevel->getId() : found->getName();
    }
    button(quizUi.level, selectedLevelName.c_str(), false, !hasSession && activity.quizLevels.size() > 1);
    UITheme::drawText(TextFormat("%zu active questions", activity.activeQuestionCount),
        static_cast<int>(quizUi.level.x + quizUi.level.width + 14.0f),
        static_cast<int>(quizUi.level.y + 10.0f), 16, kMuted);
    if (active && !activity.quiz->questions.empty()) {
        const std::size_t index = std::min(questionIndex, activity.quiz->questions.size() - 1);
        const auto& question = activity.quiz->questions[index];
        const auto& selections = activity.quiz->selectedAnswers;
        const auto answered = static_cast<std::size_t>(selections.size() - std::count(selections.begin(), selections.end(), '-'));
        UITheme::drawText(TextFormat("Question %zu of %zu  |  Answered %zu", index + 1,
            activity.quiz->questions.size(), answered), static_cast<int>(list.x + 16.0f),
            static_cast<int>(list.y + 58.0f), 17, kMuted);
        UITheme::drawWrapped(question.prompt, static_cast<int>(list.x + 16.0f),
            static_cast<int>(list.y + 90.0f), static_cast<int>(list.width - 32.0f), 20, kText,
            list.height < 350.0f ? 2 : 3);
        for (std::size_t option = 0; option < 4; ++option) {
            const Rectangle bounds = quizUi.options[option];
            const bool chosen = selections[index] == static_cast<char>('A' + option);
            DrawRectangleRounded(bounds,0.12f,7,chosen?Color{255,224,234,255}:RAYWHITE);
            DrawRectangleRoundedLinesEx(bounds,0.12f,7,chosen?2.0f:1.0f,chosen?UITheme::primary:kBorder);
            BeginScissorMode(static_cast<int>(bounds.x+10.0f),static_cast<int>(bounds.y+4.0f),
                std::max(1,static_cast<int>(bounds.width-20.0f)),static_cast<int>(bounds.height-8.0f));
            UITheme::drawWrapped(std::string(1,static_cast<char>('A'+option))+". "+question.options[option],
                static_cast<int>(bounds.x+12.0f),static_cast<int>(bounds.y+9.0f),
                static_cast<int>(bounds.width-24.0f),17,kText,2);
            EndScissorMode();
        }
        button(quizUi.previous,"Previous",false,index>0);
        button(quizUi.next,"Next",false,index+1<activity.quiz->questions.size());
    } else if (submitted) {
        if (activity.quizResult) {
            UITheme::drawText(TextFormat("Score: %d / %d  (%.0f%%)", activity.quizResult->getCorrectAnswers(),
                activity.quizResult->getTotalQuestions(), activity.quizResult->percentage()),
                static_cast<int>(list.x+16.0f),static_cast<int>(list.y+58.0f),22,UITheme::strongText);
        } else {
            UITheme::drawText("Quiz submitted",static_cast<int>(list.x+16.0f),static_cast<int>(list.y+58.0f),22,UITheme::strongText);
        }
        const float rowHeight = kQuizAnswerRowHeight;
        const float rowsTop = list.y + 96.0f;
        const std::size_t visible = visibleQuizAnswers(list);
        const std::size_t maxOffset = activity.quizAnswers.size()>visible?activity.quizAnswers.size()-visible:0;
        const std::size_t first = std::min(scrollOffset,maxOffset);
        BeginScissorMode(static_cast<int>(list.x+6.0f),static_cast<int>(rowsTop),
            static_cast<int>(list.width-18.0f),static_cast<int>(list.height-108.0f));
        for(std::size_t visibleIndex=0;visibleIndex<visible&&first+visibleIndex<activity.quizAnswers.size();++visibleIndex){
            const auto& answer=activity.quizAnswers[first+visibleIndex];
            const float y=rowsTop+static_cast<float>(visibleIndex)*rowHeight;
            DrawRectangleRounded({list.x+10.0f,y+2.0f,list.width-30.0f,rowHeight-8.0f},0.06f,6,
                answer.selectedAnswer==answer.correctAnswer?Color{241,250,244,255}:Color{255,244,246,255});
            UITheme::drawText(TextFormat("Question %zu",first+visibleIndex+1),static_cast<int>(list.x+20.0f),static_cast<int>(y+8.0f),16,kMuted);
            std::string selectedText=answer.selectedAnswer=='-'?"Not answered":std::string(1,answer.selectedAnswer);
            std::string correctText(1,answer.correctAnswer);
            if(first+visibleIndex<activity.quiz->questions.size()){
                const auto& options=activity.quiz->questions[first+visibleIndex].options;
                const int selectedIndex=answer.selectedAnswer-'A';
                const int correctIndex=answer.correctAnswer-'A';
                if(selectedIndex>=0&&selectedIndex<static_cast<int>(options.size()))selectedText+=" - "+options[static_cast<std::size_t>(selectedIndex)];
                if(correctIndex>=0&&correctIndex<static_cast<int>(options.size()))correctText+=" - "+options[static_cast<std::size_t>(correctIndex)];
            }
            UITheme::drawWrapped("Your answer: "+selectedText,static_cast<int>(list.x+20.0f),static_cast<int>(y+29.0f),
                static_cast<int>(list.width-58.0f),14,kText,2);
            UITheme::drawWrapped("Correct answer: "+correctText,static_cast<int>(list.x+20.0f),static_cast<int>(y+58.0f),
                static_cast<int>(list.width-58.0f),14,kText,2);
            UITheme::drawWrapped("Explanation: "+answer.explanation,static_cast<int>(list.x+20.0f),static_cast<int>(y+88.0f),
                static_cast<int>(list.width-58.0f),14,kMuted,2);
        }
        EndScissorMode();
    } else if (activity.quizLevels.empty()) {
        UITheme::drawWrapped("No JLPT levels are available for a quiz.",static_cast<int>(list.x+18.0f),
            static_cast<int>(list.y+88.0f),static_cast<int>(list.width-36.0f),18,kMuted,3);
    } else if (activity.activeQuestionCount == 0) {
        UITheme::drawWrapped("This level has no active questions. Choose another level or ask an administrator to activate questions.",
            static_cast<int>(list.x+18.0f),static_cast<int>(list.y+88.0f),static_cast<int>(list.width-36.0f),18,kMuted,4);
    } else {
        UITheme::drawWrapped("This level is ready. Start the quiz when you are ready.",static_cast<int>(list.x+18.0f),
            static_cast<int>(list.y+88.0f),static_cast<int>(list.width-36.0f),18,kMuted,3);
    }
    const std::size_t visible = visibleQuizAnswers(list);
    if (submitted && activity.quizAnswers.size() > visible) {
        const Rectangle track{list.x+list.width-7.0f,list.y+98.0f,3.0f,list.height-112.0f};
        DrawRectangleRounded(track,0.5f,3,Color{239,224,230,255});
        const std::size_t maxOffset=activity.quizAnswers.size()-visible;
        const float thumb=std::max(22.0f,track.height*static_cast<float>(visible)/static_cast<float>(activity.quizAnswers.size()));
        DrawRectangleRounded({track.x,track.y+(track.height-thumb)*(maxOffset?static_cast<float>(std::min(scrollOffset,maxOffset))/maxOffset:0.0f),track.width,thumb},0.5f,3,kBorder);
    }

}

void BackendScreen::drawStudyActions(Section section, float x, float y, const BackendActivityState& activity) const {
    const bool reviewPage = section == Section::Review;
    const bool quizPage = section == Section::Quiz;
    if (reviewPage) {
        const bool active = activity.review && !activity.review->finished && activity.review->current.has_value();
        const bool revealed = active && activity.review->current->back.has_value();
        button(action(x,y),activity.review&& !activity.review->finished ? "Review active" : "Start review",
               !activity.review || activity.review->finished, !activity.review || activity.review->finished);
        if (active && !revealed) button(action(x,y+48),"Reveal answer",true,true);
        if (active && revealed) {
            button(action(x,y+96,80,40),"Known",true,true);
            button(action(x+90,y+96,80,40),"Again",false,true);
        }
        if (active) button(action(x,y+144),"Stop review",false,true);
    } else if (quizPage) {
        const bool hasSession = activity.quiz.has_value();
        const bool active = hasSession && !activity.quiz->submitted;
        const bool submitted = hasSession && activity.quiz->submitted;
        const bool hasWrongAnswers = quizHasWrongAnswers(activity);
        if (!hasSession) {
            button(action(x,y),activity.activeQuestionCount>0
                ? TextFormat("Start quiz (%zu)",std::min<std::size_t>(10,activity.activeQuestionCount))
                : "No active questions",true,activity.activeQuestionCount>0);
        } else if (active) {
            button(action(x,y),"Quiz in progress",false,false);
            button(action(x,y+48),"Submit quiz",true,true);
            button(action(x,y+96),"Stop quiz",false,true);
        } else if (submitted) {
            button(action(x,y),"Quiz submitted",false,false);
            if (hasWrongAnswers) button(action(x,y+48),"Retry wrong",true,true);
            else UITheme::drawWrapped("Great job! You got all questions correct.", static_cast<int>(x), static_cast<int>(y + 52.0f),
                                      static_cast<int>(kActionButtonWidth), 14, UITheme::strongText, 3);
            button(action(x,y+(hasWrongAnswers?96.0f:120.0f)),"Stop quiz",false,true);
        }
    }
}
