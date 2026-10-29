// Ritual Helper - plain data shared by the scanner, the planner, the click sequence and the UI.
// Copyright (C) Omer Faruk ARPA and POEFixer contributors. Licensed under GPL-3.0 (see LICENSE).
#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
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
    // The English name the host prices the item by (LookupPriceByItem, desktop 371+): the item
    // list and prices match by it in every game language. Empty on an older host or when the
    // host could not name it: `name` is used then.
    std::string englishName;
    std::string path;      // Metadata/Items/... (ASCII id)
    bool  unique = false;  // Rarity == Unique
    int   stack = 1;
    int   slotX = 0, slotY = 0;
    ScreenRect rect;
};

// The name a reward is matched and priced by: its English identity name, else the shown name.
inline const std::string& IdentityName(const RitualReward& r) {
    return r.englishName.empty() ? r.name : r.englishName;
}

// The English identity names of the open window's rewards, by item address (the same path
// and shown name: an address the game reused names another item). Only a name the host gave
// is kept: a reward it could not name (its item data still downloading) is asked again on
// the next read, never remembered unnamed.
class EnglishNameCache {
public:
    static constexpr size_t kMax = 512;

    const std::string* Find(uintptr_t address, const std::string& path, const std::string& shown) const {
        const auto it = m_names.find(address);
        if (it == m_names.end() || it->second.path != path || it->second.shown != shown) return nullptr;
        return &it->second.english;
    }
    void Store(uintptr_t address, const std::string& path, const std::string& shown, const std::string& english) {
        if (english.empty()) {
            m_names.erase(address);
            return;
        }
        if (m_names.size() >= kMax && m_names.find(address) == m_names.end()) m_names.clear();
        m_names[address] = Entry{ path, shown, english };
    }
    void Clear() { m_names.clear(); }
    size_t Size() const { return m_names.size(); }

private:
    struct Entry {
        std::string path, shown, english;
    };
    std::unordered_map<uintptr_t, Entry> m_names;
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
