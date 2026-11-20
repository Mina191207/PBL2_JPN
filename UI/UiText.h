#pragma once

#include "raylib.h"

#include <string>
#include <vector>

namespace UiText {
bool initialize();
void shutdown();
void suspend();
bool resume();
void beginFrame();
void endFrame();
void preload(const std::vector<std::string>& strings);
void draw(const char* text, int x, int y, int fontSize, Color tint);
int measure(const char* text, int fontSize);
const std::string& error();
}
