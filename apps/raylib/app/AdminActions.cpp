#include "AppController.h"
#include "pbl2/InputLimits.h"
#include <algorithm>
#include <utility>

namespace {
using Field = FieldId;
using Event = BackendScreenEvent::Type;
Status missingRecord() {
    return {BackendError::NOT_FOUND, SaveState::UNCHANGED, "That record is no longer available."};
}
}

void AppController::refreshAdminBackendSection(Section section) {
    if (section == Section::Levels) {
        const auto levels = application_.levels();
        if (!levels.value) { reportBackend(levels.status, ""); return; }
        backendTable_.headers = {"ID", "Category", "Level", "Order", "Description"};
        for (const auto& level : *levels.value)
            backendTable_.rows.push_back({level.getId(), {level.getId().getId(), level.getCategory(),
                level.getName(), std::to_string(level.getOrder()), level.getDescription()}});
    } else if (section == Section::Accounts) {
        const auto accounts = application_.accounts();
        if (!accounts.value) { reportBackend(accounts.status, ""); return; }
        backendTable_.headers = {"ID", "Username", "Role", "Status"};
        for (const auto& account : *accounts.value)
            backendTable_.rows.push_back({account.id, {account.id.getId(), account.username,
                account.role == UserRole::ADMIN ? "Admin" : "Learner",
                account.status == UserStatus::ACTIVE ? "Active" : "Locked"}, account.role});
    } else if (section == Section::Questions) {
        const auto questions = application_.questions();
        if (!questions.value) { reportBackend(questions.status, ""); return; }
        backendTable_.headers = {"ID", "Prompt", "Level", "Status"};
        for (const auto& question : *questions.value) {
            const auto found = std::find_if(resourceLevels_.begin(), resourceLevels_.end(), [&](const Level& level) {
                return level.getId() == question.getLevelId();
            });
            backendTable_.rows.push_back({question.getId(), {question.getId().getId(), question.getPrompt(),
                found == resourceLevels_.end() ? question.getLevelId().getId() : found->getName(),
                question.getStatus() == QuestionStatus::ACTIVE ? "Active" : "Inactive"}});
        }
    }
}

void AppController::handleAdminEvent(const BackendScreenEvent& event) {
    auto refreshAndReport = [&](const Status& status, const char* success) {
        if (status.ok()) refreshBackendSection();
        reportBackend(status, success);
    };
    if (event.type == Event::IMPORT_PACKAGE) {
        refreshAndReport(application_.importPackage(event.text), "Package imported.");
        return;
    }
    if (event.type == Event::EXPORT_PACKAGE) {
        reportBackend(application_.exportPackage(event.text), "Package exported.");
        return;
    }
    if (event.type == Event::PASSWORD_TOO_LONG) {
        reportBackend({BackendError::INVALID_INPUT, SaveState::UNCHANGED,
            "Password exceeds " + std::to_string(maxPasswordBytes) + " bytes."}, "");
        return;
    }

    const auto* levelId = std::get_if<LevelId>(&event.id);
    const auto* userId = std::get_if<UserId>(&event.id);
    const auto* alphabetId = std::get_if<AlphabetId>(&event.id);
    const auto* questionId = std::get_if<QuestionId>(&event.id);
    if (event.type == Event::TOGGLE_ACCOUNT && userId) {
        const auto accounts = application_.accounts();
        if (!accounts.value) { reportBackend(accounts.status, ""); return; }
        const auto item = std::find_if(accounts.value->begin(), accounts.value->end(),
            [&](const AccountSummary& account) { return account.id == *userId; });
        if (item == accounts.value->end()) { reportBackend(missingRecord(), ""); return; }
        const auto status = item->status == UserStatus::ACTIVE ? UserStatus::LOCKED : UserStatus::ACTIVE;
        refreshAndReport(application_.setAccountStatus(*userId, status), "Account status updated.");
        return;
    }
    if (event.type == Event::TOGGLE_QUESTION && questionId) {
        const auto item = application_.question(*questionId);
        if (!item.value) { reportBackend(item.status, ""); return; }
        const auto status = item.value->getStatus() == QuestionStatus::ACTIVE ? QuestionStatus::INACTIVE : QuestionStatus::ACTIVE;
        refreshAndReport(application_.setQuestionStatus(*questionId, status), "Question status updated.");
        return;
    }
    if (event.type == Event::DELETE_ROW) {
        const auto status = levelId ? application_.removeLevel(*levelId) :
            alphabetId ? application_.removeAlphabet(*alphabetId) :
            questionId ? application_.removeQuestion(*questionId) : missingRecord();
        refreshAndReport(status, "Record removed.");
        return;
    }
    if (event.type == Event::EDIT_ROW) {
        auto fields = adminFormFields(selectedSection_);
        auto set = [&](Field id, std::string value) { fieldValue(fields, id) = std::move(value); };
        if (levelId && selectedSection_ == Section::Levels) {
            const auto levels = application_.levels();
            if (!levels.value) { reportBackend(levels.status, ""); return; }
            const auto item = std::find_if(levels.value->begin(), levels.value->end(),
                [&](const Level& level) { return level.getId() == *levelId; });
            if (item == levels.value->end()) { reportBackend(missingRecord(), ""); return; }
            set(Field::Category, item->getCategory());
            set(Field::Name, item->getName());
            set(Field::Order, std::to_string(item->getOrder()));
            set(Field::Description, item->getDescription());
        } else if (alphabetId && selectedSection_ == Section::Alphabet) {
            const auto item = std::find_if(backendAlphabet_.begin(), backendAlphabet_.end(),
                [&](const AlphabetEntry& entry) { return entry.getId() == *alphabetId; });
            if (item == backendAlphabet_.end()) { reportBackend(missingRecord(), ""); return; }
            set(Field::Script, item->getScript() == Script::HIRAGANA ? "HIRAGANA" : "KATAKANA");
            set(Field::Glyph, item->getGlyph());
            set(Field::Romaji, item->getRomaji());
            set(Field::Group, item->getGroupName());
            set(Field::Order, std::to_string(item->getDisplayOrder()));
            set(Field::Note, item->getNote());
        } else if (questionId && selectedSection_ == Section::Questions) {
            const auto result = application_.question(*questionId);
            if (!result.value) { reportBackend(result.status, ""); return; }
            const auto& item = *result.value;
            set(Field::Level, item.getLevelId().getId());
            if (item.getResourceId()) set(Field::Resource, item.getResourceId()->getId());
            set(Field::Prompt, item.getPrompt());
            const auto& options = item.getOptions();
            set(Field::OptionA, options[0]); set(Field::OptionB, options[1]);
            set(Field::OptionC, options[2]); set(Field::OptionD, options[3]);
            set(Field::Correct, std::string(1, item.getCorrectOption()));
            set(Field::Explanation, item.getExplanation());
            set(Field::Status, item.getStatus() == QuestionStatus::ACTIVE ? "ACTIVE" : "INACTIVE");
        } else if (userId && selectedSection_ == Section::Accounts) {
            backendScreen_.openResetPasswordForm(*userId);
            return;
        } else {
            reportBackend(missingRecord(), "");
            return;
        }
        backendScreen_.openEditForm(std::move(fields), event.id);
        return;
    }
    if (event.type != Event::SAVE_FORM && event.type != Event::RESET_PASSWORD) return;

    const bool reset = event.type == Event::RESET_PASSWORD;
    const bool edit = !std::holds_alternative<std::monostate>(event.id);
    const auto& fields = backendScreen_.formFields();
    const auto value = [&](Field field) -> const std::string& { return fieldValue(fields, field); };
    Status status;
    try {
        if (reset && userId) {
            status = application_.resetPassword(*userId, backendScreen_.password());
        } else if (!reset && selectedSection_ == Section::Accounts && !edit) {
            status = application_.createAccount(value(Field::Username), backendScreen_.password(),
                value(Field::Role) == "ADMIN" ? UserRole::ADMIN : UserRole::LEARNER).status;
        } else if (!reset && selectedSection_ == Section::Levels && (!edit || levelId)) {
            LevelInput input{value(Field::Category), value(Field::Name),
                wholeNumber(value(Field::Order), "Order"), value(Field::Description)};
            status = levelId ? application_.updateLevel(*levelId, input) : application_.createLevel(input).status;
        } else if (!reset && selectedSection_ == Section::Alphabet && (!edit || alphabetId)) {
            AlphabetInput input{value(Field::Script) == "HIRAGANA" ? Script::HIRAGANA : Script::KATAKANA,
                value(Field::Glyph), value(Field::Romaji), value(Field::Group),
                wholeNumber(value(Field::Order), "Display order"), value(Field::Note)};
            status = alphabetId ? application_.updateAlphabet(*alphabetId, input) : application_.createAlphabet(input).status;
        } else if (!reset && selectedSection_ == Section::Questions && (!edit || questionId)) {
            std::optional<ResourceId> resource;
            if (!value(Field::Resource).empty()) resource.emplace(value(Field::Resource));
            const auto& answer = value(Field::Correct);
            QuestionInput input{LevelId(value(Field::Level)), resource, value(Field::Prompt),
                {value(Field::OptionA), value(Field::OptionB), value(Field::OptionC), value(Field::OptionD)},
                answer.empty() ? '\0' : answer.front(), value(Field::Explanation),
                value(Field::Status) == "ACTIVE" ? QuestionStatus::ACTIVE : QuestionStatus::INACTIVE};
            status = questionId ? application_.updateQuestion(*questionId, input) : application_.createQuestion(input).status;
        } else {
            status = missingRecord();
        }
    } catch (const std::invalid_argument& error) {
        status = {BackendError::INVALID_INPUT, SaveState::UNCHANGED, error.what()};
    }
    backendScreen_.clearPassword();
    if (status.ok()) backendScreen_.closeForm();
    refreshAndReport(status, reset ? "Password reset." : edit ? "Record updated." : "Record created.");
}
