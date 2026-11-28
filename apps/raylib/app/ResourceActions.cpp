#include "AppController.h"
#include "pbl2/InputLimits.h"
#include <algorithm>
#include <utility>

void AppController::refreshResources() {
    auto levels = application_.levels();
    if (levels.value) resourceLevels_ = std::move(*levels.value);
    if (resourceSearch_.levelId && std::none_of(resourceLevels_.begin(), resourceLevels_.end(), [&](const Level& level) {
            return level.getId() == *resourceSearch_.levelId;
        })) resourceSearch_.levelId.reset();
    resourcesScreen_.setSearch(resourceSearch_, false);
    showResourceResults(application_.search(resourceSearch_), true);
}

void AppController::showResourceResults(Result<std::vector<SearchRow>> rows, bool preserveSelection) {
    auto clear = [&] {
        resourcesScreen_.clearSelection();
        selectedResource_.reset();
        selectedResourceFavorited_.reset();
    };
    if (!rows.ok() || !rows.value) {
        clear();
        resourcesMessage_ = rows.status.message.empty() ? "Could not load resources." : rows.status.message;
        resourcesMessageError_ = true;
        resourceRows_.clear();
        return;
    }
    resourceRows_ = std::move(*rows.value);
    if (!preserveSelection) clear();
    else if (selectedResource_) {
        const auto id = std::visit([](const auto& item) { return item.getId(); }, *selectedResource_);
        const bool present = std::any_of(resourceRows_.begin(), resourceRows_.end(),
            [&](const SearchRow& row) { return row.id == id; });
        if (present) {
            auto detail = application_.resource(id);
            if (!detail.value) clear();
            else {
                selectedResource_ = std::move(*detail.value);
                refreshSelectedFavoriteState();
            }
        } else clear();
    }
    resourcesMessage_ = rows.status.save == SaveState::PENDING
                            ? (rows.status.message.empty() ? "Results loaded; saving search history is pending." : "Results loaded; history save pending: " + rows.status.message)
                            : rows.status.message;
    resourcesMessageError_ = false;
}

void AppController::handleResourcesEvent(ResourcesEvent event) {
    auto report = [&](const Status& status, const char* success) {
        if (!status.ok()) { resourcesMessage_ = status.message.empty() ? "The operation failed." : status.message; resourcesMessageError_ = true; }
        else if (status.save == SaveState::PENDING)
            { resourcesMessage_ = status.message.empty() ? "Change applied; saving is pending." : "Change applied; save pending: " + status.message; resourcesMessageError_ = true; }
        else { resourcesMessage_ = success; resourcesMessageError_ = false; }
    };
    if (event.type != ResourcesEvent::Type::NONE) {
        resourcesMessage_.clear();
        resourcesMessageError_ = false;
    }
    switch (event.type) {
    case ResourcesEvent::Type::NONE: case ResourcesEvent::Type::CANCEL_FORM: case ResourcesEvent::Type::EDITED: return;
    case ResourcesEvent::Type::INPUT_LIMIT:
        resourcesMessage_ = "Search input exceeds " + std::to_string(maxTxtFieldBytes) + " bytes.";
        resourcesMessageError_ = true;
        return;
    case ResourcesEvent::Type::OPEN_EDIT: {
        const auto detail = application_.resource(event.id);
        if (detail.ok() && detail.value) {
            resourcesScreen_.setEditing(*detail.value, resourceLevels_);
        }
        else { resourcesMessage_ = detail.status.message; resourcesMessageError_ = true; }
        return;
    }
    case ResourcesEvent::Type::PREVIEW_SEARCH:
    case ResourcesEvent::Type::SUBMIT_SEARCH:
        resourceSearch_ = std::move(event.search);
        showResourceResults(event.type == ResourcesEvent::Type::SUBMIT_SEARCH
            ? application_.submitSearch(resourceSearch_) : application_.search(resourceSearch_));
        return;
    case ResourcesEvent::Type::SELECT: {
        selectedResource_.reset();
        selectedResourceFavorited_.reset();
        auto detail = application_.resource(event.id);
        if (detail.ok() && detail.value) {
            selectedResource_ = std::move(*detail.value);
            refreshSelectedFavoriteState();
        }
        else {
            resourcesScreen_.clearSelection();
            resourcesMessage_ = detail.status.message;
            resourcesMessageError_ = true;
        }
        return;
    }
    case ResourcesEvent::Type::FAVORITE: {
        if (!selectedResourceFavorited_.has_value()) {
            resourcesMessage_ = "Favorite status is unavailable.";
            resourcesMessageError_ = true;
            return;
        }
        const bool wasFavorited = *selectedResourceFavorited_;
        const Status status = wasFavorited ? application_.removeFavorite(event.id) : application_.addFavorite(event.id);
        report(status, wasFavorited ? "Removed from favorites." : "Added to favorites.");
        if (status.ok()) refreshSelectedFavoriteState();
        return;
    }
    case ResourcesEvent::Type::REMOVE: {
        const Status status = application_.removeResource(event.id);
        report(status, "Resource removed.");
        if(status.ok()){
            resourcesScreen_.clearSelection();
            selectedResource_.reset();selectedResourceFavorited_.reset();
            const std::string message=resourcesMessage_;const bool messageError=resourcesMessageError_;
            refreshResources();
            if(!resourcesMessageError_){resourcesMessage_=message;resourcesMessageError_=messageError;}
        }
        return;
    }
    case ResourcesEvent::Type::CREATE: case ResourcesEvent::Type::UPDATE: {
        using Field = FieldId;
        const auto& draft = event.form;
        const auto value = [&](Field id) -> const std::string& { return fieldValue(draft.fields, id); };
        const bool create = event.type == ResourcesEvent::Type::CREATE;
        Status status;
        try {
            if (draft.type == ResourceType::VOCABULARY) {
                const auto& token = value(Field::PartOfSpeech);
                constexpr std::array choices{PartOfSpeech::NOUN, PartOfSpeech::VERB, PartOfSpeech::IADJECTIVE,
                    PartOfSpeech::NAADJECTIVE, PartOfSpeech::ADVERB, PartOfSpeech::OTHER};
                const auto found = std::find_if(choices.begin(), choices.end(),
                    [&](PartOfSpeech part) { return partOfSpeechToken(part) == token; });
                if (found == choices.end()) throw std::invalid_argument("Choose a part of speech.");
                VocabularyInput input{draft.levelId, value(Field::Word), value(Field::Reading),
                    value(Field::Meaning), *found, value(Field::Example)};
                status = create ? application_.createVocabulary(input).status : application_.updateVocabulary(event.id, input);
            } else if (draft.type == ResourceType::GRAMMAR) {
                GrammarInput input{draft.levelId, value(Field::Pattern), value(Field::Meaning), value(Field::Structure),
                    value(Field::Usage), value(Field::Example), value(Field::Note)};
                status = create ? application_.createGrammar(input).status : application_.updateGrammar(event.id, input);
            } else {
                KanjiInput input{draft.levelId, value(Field::Character), value(Field::Meaning),
                    wholeNumber(value(Field::Strokes), "Stroke count"), value(Field::Onyomi),
                    value(Field::Kunyomi), value(Field::Example)};
                status = create ? application_.createKanji(input).status : application_.updateKanji(event.id, input);
            }
        } catch (const std::invalid_argument& error) {
            status = {BackendError::INVALID_INPUT, SaveState::UNCHANGED, error.what()};
        }
        report(status, create ? "Resource created." : "Resource updated.");
        if (status.ok()) {
            resourcesScreen_.closeForm();
            const std::string message=resourcesMessage_;const bool messageError=resourcesMessageError_;
            refreshResources();
            if(!resourcesMessageError_){resourcesMessage_=message;resourcesMessageError_=messageError;}
        }
        return;
    }
    }
}

void AppController::refreshSelectedFavoriteState() {
    selectedResourceFavorited_.reset();
    if (!activeAccount_ || !selectedResource_) return;
    const auto favoriteRows = application_.favorites();
    if (!favoriteRows.ok() || !favoriteRows.value) return;
    const ResourceId selectedId = std::visit([](const auto& item) { return item.getId(); }, *selectedResource_);
    selectedResourceFavorited_ = std::any_of(favoriteRows.value->begin(), favoriteRows.value->end(), [&](const auto& detail) {
        return std::visit([&](const auto& item) { return item.getId() == selectedId; }, detail);
    });
}

