// Ritual Helper - settings file.
// Copyright (C) Omer Faruk ARPA and POEFixer contributors. Licensed under GPL-3.0 (see LICENSE).
#include "Settings.h"

#include "game/Text.h"

#include "../lib/nlohmann/json.hpp"
#include "../../../POEFixer/core/FileIO.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iterator>
#include <system_error>
#include <unordered_set>

namespace RitualHelper {

namespace {

using json = nlohmann::json;

// Type-checked reads: json::value() throws on a type mismatch, these never do.
bool ReadBool(const json& j, const char* key, bool& out) {
    auto it = j.find(key);
    if (it == j.end() || !it->is_boolean()) return false;
    out = it->get<bool>();
    return true;
}

bool ReadNumber(const json& j, const char* key, double& out) {
    auto it = j.find(key);
    if (it == j.end() || !it->is_number()) return false;
    const double v = it->get<double>();
    if (!std::isfinite(v)) return false;
    out = v;
    return true;
}

void ReadInt(const json& j, const char* key, int& out, int lo, int hi) {
    double v = 0.0;
    if (ReadNumber(j, key, v)) out = std::clamp(static_cast<int>(std::lround(std::clamp(v, -1e9, 1e9))), lo, hi);
}

}  // namespace

std::filesystem::path Settings::FilePath(const std::filesystem::path& pluginDir) {
    return pluginDir / L"config" / L"settings.json";
}

LoadResult Settings::Load(const std::filesystem::path& pluginDir) {
    LoadResult result = LoadResult::Missing;
    try {
        const auto path = FilePath(pluginDir);
        std::error_code ec;
        if (!std::filesystem::exists(path, ec)) {
            // Missing is a first start; an error asking (no access) may hide a real file.
            if (!ec) return result;
            result = LoadResult::Unreadable;
            KeepUnusableFile(path);
            return result;
        }
        std::ifstream in(path, std::ios::binary);
        if (!in.is_open()) {
            result = LoadResult::Unreadable;   // locked by another program, no access
            KeepUnusableFile(path);
            return result;
        }
        const std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        const bool readOk = !in.bad();
        in.close();
        const json j = readOk ? json::parse(text, nullptr, false) : json(json::value_t::discarded);
        if (!readOk || j.is_discarded() || !j.is_object()) {
            result = readOk ? LoadResult::Unparsable : LoadResult::Unreadable;
            KeepUnusableFile(path);
            return result;
        }
        result = LoadResult::Loaded;

        ReadBool(j, "show_overlay", showOverlay);
        ReadBool(j, "show_values", showValues);
        if (auto it = j.find("highlight_color"); it != j.end() && it->is_array() && it->size() == 4) {
            float c[4] = {};
            bool ok = true;
            for (size_t i = 0; i < 4 && ok; ++i) {
                ok = (*it)[i].is_number();
                if (ok) c[i] = std::clamp((*it)[i].get<float>(), 0.0f, 1.0f);
            }
            if (ok) std::copy(std::begin(c), std::end(c), highlight);
        }
        ReadInt(j, "scan_interval_ms", scanIntervalMs, kScanIntervalMinMs, kScanIntervalMaxMs);

        double mv = 0.0;
        if (ReadNumber(j, "min_value", mv) || ReadNumber(j, "min_value_exalted", mv))  // 1.0 spelling
            minValue = std::clamp(mv, 0.0, kMinValueMax);
        ReadInt(j, "min_value_unit", minValueUnit, 0, 1);
        ReadInt(j, "price_display_unit", priceDisplayUnit, 0, 1);
        ReadInt(j, "click_pause_ms", clickPauseMs, kClickPauseMinMs, kClickPauseMaxMs);

        ReadBool(j, "dry_run", dryRun);
        if (!ReadBool(j, "show_scan_status", showScanStatus))
            ReadBool(j, "debug_mode", showScanStatus);  // 1.5.4 "Debug mode" drew the status line

        if (auto it = j.find("selected_items"); it != j.end() && it->is_array()) {
            selectedItems.clear();
            std::unordered_set<std::string> seen;
            for (const auto& e : *it) {
                if (!e.is_string()) continue;
                std::string s = e.get<std::string>();
                if (s.empty() || s.size() > kMaxNameBytes || selectedItems.size() >= kMaxSelected) continue;
                if (seen.insert(NameKey(s)).second) selectedItems.push_back(std::move(s));
            }
        }
        // The 1.5.4 keys "enabled", "league" and "price_refresh_minutes" are not read any
        // more: the plugin switch, the Settings > Prices league and its refresh replace them.
    } catch (...) {
        // Keep whatever was read; defaults for the rest.
    }
    return result;
}

// The file exists but gave nothing usable: keep a copy for the player and count the
// defaults as already written, so Save() leaves the file alone until a setting changes.
void Settings::KeepUnusableFile(const std::filesystem::path& path) {
    try {
        std::error_code ec;
        auto corrupt = path;
        corrupt += L".corrupt";
        std::filesystem::copy_file(path, corrupt, std::filesystem::copy_options::overwrite_existing, ec);
        m_lastWritten = ToText();
    } catch (...) {
    }
}

std::string Settings::ToText() const {
    json j;
    j["settings_version"] = 2;
    j["show_overlay"] = showOverlay;
    j["show_values"] = showValues;
    j["highlight_color"] = {highlight[0], highlight[1], highlight[2], highlight[3]};
    j["scan_interval_ms"] = scanIntervalMs;
    j["selected_items"] = selectedItems;
    j["min_value"] = minValue;
    j["min_value_unit"] = minValueUnit;
    j["price_display_unit"] = priceDisplayUnit;
    j["click_pause_ms"] = clickPauseMs;
    j["dry_run"] = dryRun;
    j["show_scan_status"] = showScanStatus;
    return j.dump(2, ' ', false, json::error_handler_t::replace);
}

bool Settings::Save(const std::filesystem::path& pluginDir) {
    try {
        const std::string text = ToText();
        if (text == m_lastWritten) return true;

        std::error_code ec;
        const auto path = FilePath(pluginDir);
        std::filesystem::create_directories(path.parent_path(), ec);
        if (!FileIO::AtomicWrite(path, text)) return false;
        m_lastWritten = text;
        return true;
    } catch (...) {
        return false;
    }
}

bool Settings::IsSelected(const std::string& name) const {
    const std::string key = NameKey(name);
    for (const auto& s : selectedItems)
        if (NameKey(s) == key) return true;
    return false;
}

void Settings::Select(const std::string& name, bool on) {
    const std::string key = NameKey(name);
    if (key.empty() || name.size() > kMaxNameBytes) return;
    auto it = std::find_if(selectedItems.begin(), selectedItems.end(),
                           [&](const std::string& s) { return NameKey(s) == key; });
    if (on && it == selectedItems.end() && selectedItems.size() < kMaxSelected) selectedItems.push_back(name);
    if (!on && it != selectedItems.end()) selectedItems.erase(it);
}

void Settings::SelectReward(const std::string& english, const std::string& shown, bool on) {
    Select(english, on);
    if (!on) Select(shown, false);
}

}  // namespace RitualHelper
