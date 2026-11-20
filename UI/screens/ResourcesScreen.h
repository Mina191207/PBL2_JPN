#pragma once

#include "Application.h"
#include "raylib.h"

#include <array>
#include <optional>
#include <string>
#include <utility>
#include <vector>

struct ResourceForm {
    ResourceType type = ResourceType::VOCABULARY;
    LevelId levelId{"L005"};
    std::array<std::string, 8> fields{};
    int strokeCount = 1;
};

struct ResourcesEvent {
    enum class Type { NONE, SEARCH, SELECT, OPEN_EDIT, CREATE, UPDATE, REMOVE, FAVORITE, CANCEL_FORM } type = Type::NONE;
    SearchOptions search;
    ResourceId id{"R001"};
    ResourceForm form;
    ResourcesEvent() = default;
    ResourcesEvent(Type eventType, SearchOptions options = {}, ResourceId resourceId = ResourceId("R001"), ResourceForm resourceForm = {})
        : type(eventType), search(std::move(options)), id(std::move(resourceId)), form(std::move(resourceForm)) {}
};

struct ResourceUiLayout;

class ResourcesScreen {
  public:
    ResourcesEvent update(const std::vector<SearchRow>& rows,
                          const std::vector<Level>& levels,
                          const AccountSummary& account,
                          const std::optional<bool>& selectedFavorited);
    void draw(const std::vector<SearchRow>& rows,
              const std::vector<Level>& levels,
              const std::optional<ResourceDetail>& detail,
              const AccountSummary& account,
              const std::string& message,
              bool messageError,
              const std::optional<bool>& selectedFavorited) const;
    void setEditing(const ResourceDetail& detail, const std::vector<Level>& levels);
    void reset() noexcept;

  private:
    std::string query_;
    ResourceType type_ = ResourceType::VOCABULARY;
    bool allTypes_ = true;
    std::size_t levelIndex_ = 0;
    SearchSort sort_ = SearchSort::RELEVANCE;
    bool formOpen_ = false;
    bool editing_ = false;
    bool partOfSpeechOpen_ = false;
    bool confirmDelete_ = false;
    bool hasSelected_ = false;
    bool queryFocused_ = false;
    ResourceId selectedId_{"R001"};
    ResourceForm form_;
    std::size_t formLevelIndex_ = 0;
    std::size_t focusedField_ = 0;
    std::size_t rowOffset_ = 0;

    static Rectangle bounds(float x, float y, float w, float h);
    static bool click(Rectangle rectangle);
    static void button(Rectangle rectangle, const char* label, bool primary = false, bool enabled = true);
    static void field(Rectangle rectangle, const std::string& label,
                      const std::string& value, bool focused = false);
    static void appendInput(std::string& value, int limit);
    ResourcesEvent searchEvent(const std::vector<Level>& levels) const;
    ResourcesEvent updateDeleteConfirmation();
    ResourcesEvent updateResourceForm(const std::vector<Level>& levels);
    std::optional<ResourcesEvent> updateSearchControls(const std::vector<Level>& levels,
                                                       const ResourceUiLayout& ui);
    std::optional<ResourcesEvent> updateResourceSelection(const std::vector<SearchRow>& rows,
                                                          const ResourceUiLayout& ui);
    ResourcesEvent updateResourceActions(const AccountSummary& account, const std::vector<Level>& levels,
                                         const std::optional<bool>& selectedFavorited,
                                         const ResourceUiLayout& ui);
    void drawSearchSection(const ResourceUiLayout& ui, const std::vector<Level>& levels) const;
    void drawPanelHeaders(const ResourceUiLayout& ui) const;
    void drawResourceList(const ResourceUiLayout& ui, const std::vector<SearchRow>& rows,
                          const std::vector<Level>& levels) const;
    void drawResourceDetails(const ResourceUiLayout& ui, const std::optional<ResourceDetail>& detail,
                             const AccountSummary& account, const std::string& message) const;
    void drawResourceActions(const ResourceUiLayout& ui, const AccountSummary& account,
                             const std::optional<bool>& selectedFavorited, const std::string& message,
                             bool messageError) const;
    void drawResultRange(const ResourceUiLayout& ui, const std::vector<SearchRow>& rows) const;
    void drawResourceForm(const std::vector<Level>& levels) const;
    void drawDeleteConfirmation() const;
};
