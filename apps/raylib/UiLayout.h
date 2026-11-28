#pragma once

#include "raylib.h"

#include <algorithm>

namespace UiLayout {
inline float sidebarWidth() {
    return std::clamp(static_cast<float>(GetScreenWidth()) * 0.275f, 220.0f, 264.0f);
}

inline Rectangle content() {
    const float width = static_cast<float>(GetScreenWidth());
    const float height = static_cast<float>(GetScreenHeight());
    const float left = std::min(width - 320.0f, sidebarWidth() + 36.0f);
    const float right = std::min(36.0f, std::max(16.0f, width * 0.025f));
    const float top = std::min(124.0f, std::max(104.0f, height * 0.172f));
    const float bottom = std::min(30.0f, std::max(16.0f, height * 0.045f));
    return {left, top, std::max(280.0f, width - left - right),
            std::max(220.0f, height - top - bottom)};
}

inline Rectangle centeredDialog(float preferredWidth, float preferredHeight, float margin = 24.0f) {
    const float width = std::min(preferredWidth, std::max(320.0f, static_cast<float>(GetScreenWidth()) - margin * 2.0f));
    const float height = std::min(preferredHeight, std::max(240.0f, static_cast<float>(GetScreenHeight()) - margin * 2.0f));
    return {(static_cast<float>(GetScreenWidth()) - width) * 0.5f,
            (static_cast<float>(GetScreenHeight()) - height) * 0.5f, width, height};
}

inline float margin() {
    return std::clamp(static_cast<float>(GetScreenWidth()) * 0.018f, 16.0f, 28.0f);
}
}
