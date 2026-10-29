// Ritual Helper - settings model and its JSON file (config/settings.json next to the DLL).
// Copyright (C) Omer Faruk ARPA and POEFixer contributors. Licensed under GPL-3.0 (see LICENSE).
#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace RitualHelper {

inline constexpr int    kScanIntervalMinMs = 150;
inline constexpr int    kScanIntervalMaxMs = 2000;
inline constexpr int    kClickPauseMinMs = 120;
inline constexpr int    kClickPauseMaxMs = 800;
inline constexpr double kMinValueMax = 1000000.0;
inline constexpr size_t kMaxNameBytes = 128;
inline constexpr size_t kMaxSelected = 4000;

// What Settings::Load found.
enum class LoadResult {
    Loaded,      // the file was read and applied
    Missing,     // no file yet (first start): defaults
    Unreadable,  // the file exists but could not be opened or read (locked, no access): defaults
    Unparsable,  // the file is not a JSON object (hand edit, truncated): defaults
};

struct Settings {
    // Overlay
    bool  showOverlay = true;      // outlines, value labels and the DEFER button
    bool  showValues = true;       // value label on each highlighted reward
    float highlight[4] = {1.00f, 0.67f, 0.16f, 1.00f};
    int   scanIntervalMs = 400;    // how often the open Favours window is re-read

    // What to defer
    std::vector<std::string> selectedItems;  // display names as the player ticked them
    double minValue = 0.0;         // 0 = value rule off
    int    minValueUnit = 0;       // 0 Exalted, 1 Divine
    int    priceDisplayUnit = 0;   // 0 Exalted, 1 Divine

    // Clicking
    int clickPauseMs = 230;        // time per click of the sequence

    // Diagnostics
    bool dryRun = false;           // never click; show what would be deferred
    bool showScanStatus = false;   // one status line on screen

    static std::filesystem::path FilePath(const std::filesystem::path& pluginDir);

    // Reads the file (the 1.5.4 format included). A missing or broken file leaves
    // the defaults; nothing here throws. A file that exists but cannot be read or
    // parsed is copied to settings.json.corrupt (when it can be copied) and is not
    // overwritten by Save() until a setting differs from the defaults.
    LoadResult Load(const std::filesystem::path& pluginDir);

    // Writes the file atomically, only when its content changed. False on a write failure.
    bool Save(const std::filesystem::path& pluginDir);

    bool IsSelected(const std::string& name) const;
    void Select(const std::string& name, bool on);
    // A reward of the open Favours window by its English name and the name the game shows
    // (equal on an English client or an older host). Ticking stores the English name; unticking
    // clears both, so a tick 1.5.8 stored under the game's name goes too.
    void SelectReward(const std::string& english, const std::string& shown, bool on);

private:
    std::string ToText() const;
    void KeepUnusableFile(const std::filesystem::path& path);

    std::string m_lastWritten;
};

}  // namespace RitualHelper
