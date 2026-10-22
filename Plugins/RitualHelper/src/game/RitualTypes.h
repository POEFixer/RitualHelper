// Ritual Helper - plain data shared by the scanner, the planner, the click sequence and the UI.
// Copyright (C) Omer Faruk ARPA and POEFixer contributors. Licensed under GPL-3.0 (see LICENSE).
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace RitualHelper {

// A rectangle in game client coordinates (the overlay's ImGui coordinates).
struct ScreenRect {
    float x = 0.f, y = 0.f, w = 0.f, h = 0.f;
    float Right() const { return x + w; }
    float Bottom() const { return y + h; }
    float CenterX() const { return x + w * 0.5f; }
    float CenterY() const { return y + h * 0.5f; }
    bool Contains(float px, float py) const { return px >= x && px < x + w && py >= y && py < y + h; }
};

// One reward in the Favours window.
struct RitualReward {
    std::string name;      // unique name for uniques, else the base type (localized, UTF-8)
    bool  unique = false;  // Rarity == Unique
    int   stack = 1;
    int   slotX = 0, slotY = 0;
    ScreenRect rect;
};

// The open Favours window: its reward grid and the rewards on it.
struct RitualWindow {
    int   boxesX = 0, boxesY = 0;
    float gridX = 0.f, gridY = 0.f, cellSize = 0.f;
    std::vector<RitualReward> rewards;

    float GridW() const { return static_cast<float>(boxesX) * cellSize; }
    float GridH() const { return static_cast<float>(boxesY) * cellSize; }
};

// What the button under the reward grid currently says.
enum class BottomMode { None, OfferTribute, DeferItem };

// A visible game UI element seen by the scanner.
struct UiCandidate {
    uintptr_t addr = 0;
    std::string text;      // localized text (empty for icons)
    ScreenRect rect;
    int childCount = 0;
};

}  // namespace RitualHelper
