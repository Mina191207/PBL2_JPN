#pragma once

#include "pbl2/Application.h"
#include "Forms.h"
#include "raylib.h"

#include <array>
#include <optional>
#include <string>
#include <utility>
#include <vector>

struct ResourceForm {
    ResourceType type = ResourceType::VOCABULARY;
    LevelId levelId{"L005"};
    FormFields fields;
};

struct ResourcesEvent {
    enum class Type { NONE, SUBMIT_SEARCH, PREVIEW_SEARCH, INPUT_LIMIT, SELECT, OPEN_EDIT,
                      CREATE, UPDATE, REMOVE, FAVORITE, CANCEL_FORM } type = Type::NONE;
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
    void setSearch(const SearchOptions& options, bool resetSelection = true);
    void closeForm() noexcept;
    void clearSelection() noexcept;
    void reset() noexcept;

  private:
    SearchOptions search_;
    bool formOpen_ = false;
    bool editing_ = false;
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
    ResourcesEvent updateDeleteConfirmation();
    ResourcesEvent updateResourceForm(const std::vector<Level>& levels);
    std::optional<ResourcesEvent> updateSearchControls(const std::vector<Level>& levels, bool empty,
                                                       const ResourceUiLayout& ui);
    std::optional<ResourcesEvent> updateResourceSelection(const std::vector<SearchRow>& rows,
                                                          const ResourceUiLayout& ui);
    ResourcesEvent updateResourceActions(const AccountSummary& account, const std::vector<Level>& levels,
                                         const std::optional<bool>& selectedFavorited,
                                         const ResourceUiLayout& ui);
    void drawSearchSection(const ResourceUiLayout& ui, const std::vector<Level>& levels) const;
    void drawPanelHeaders(const ResourceUiLayout& ui) const;
    void drawResourceList(const ResourceUiLayout& ui, const std::vector<SearchRow>& rows,
                          const std::vector<Level>& levels, bool searchError) const;
    void drawResourceDetails(const ResourceUiLayout& ui, const std::optional<ResourceDetail>& detail,
                             const AccountSummary& account, const std::string& message) const;
    void drawResourceActions(const ResourceUiLayout& ui, const AccountSummary& account,
                             const std::optional<bool>& selectedFavorited, const std::string& message,
                             bool messageError) const;
    void drawResultRange(const ResourceUiLayout& ui, const std::vector<SearchRow>& rows, bool searchError) const;
    void drawResourceForm(const std::vector<Level>& levels, const std::string& error) const;
    void drawDeleteConfirmation() const;
};
