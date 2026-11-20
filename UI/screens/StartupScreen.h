#pragma once

#include "raylib.h"

#include <string>

enum class StartupAction {
    NONE,
    REQUEST_CLOSE,
    RETRY_SAVE,
    DISCARD_CLOSE,
    CANCEL_CLOSE
};

class StartupScreen {
  public:
    StartupAction poll(bool exitDialog) const;
    void draw(bool backendReady,
              const std::string& startupMessage,
              bool exitDialog,
              const std::string& exitMessage) const;
    void drawExitDialog(const std::string& exitMessage) const;

  private:
    Rectangle closeButton() const;
    Rectangle exitChoiceButton(int index) const;
    static bool clicked(Rectangle bounds);
    static void drawButton(Rectangle bounds, const char* label, bool primary);
    static void drawWrapped(const std::string& message,
                            int x,
                            int y,
                            int maxWidth,
                            int fontSize,
                            Color color);
};