#pragma once

#include "raylib.h"

#include <string>

namespace UiText {
bool initialize();
void shutdown();
void beginFrame();
void endFrame();
void draw(const char* text, int x, int y, int fontSize, Color tint);
int measure(const char* text, int fontSize);
const std::string& error();
}
