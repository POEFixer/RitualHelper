// Ritual Helper - plugin class.
// Copyright (C) Omer Faruk ARPA and POEFixer contributors. Licensed under GPL-3.0 (see LICENSE).
#pragma once

#include "PriceBook.h"
#include "Settings.h"
#include "game/DeferPlanner.h"
#include "game/DeferSequence.h"
#include "game/LocText.h"
#include "game/RitualScanner.h"
#include "input/Win32Input.h"

#include "../../../POEFixer/imgui/imgui.h"
#include "../../../POEFixer/plugin_sdk/PluginSDK.h"

#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

namespace RitualHelper {

inline constexpr const char* kPluginName = "Ritual Helper";

class RitualHelperPlugin final : public PluginSDK::Plugin {
public:
    RitualHelperPlugin();
    ~RitualHelperPlugin() override;

    const char* GetName() const override { return kPluginName; }
    bool WantsOverlay() const override { return m_settings.showOverlay; }

    void OnEnable(bool attached) override;
    void OnDisable() override;
    void SaveSettings() override;
    void DrawUI() override;
    void DrawSettings() override;

private:
    // RitualHelper.cpp
    void BindImGui() const;
    void TickSequence(int64_t nowMs);
    void UpdateMatches(const ScanSnapshot& snap);
    void StartDefer(const ScanSnapshot& snap, int64_t nowMs);
    void RulesChanged();
    void EnsureSelectedKeys();
    void ToggleItem(const std::string& name, bool on);
    void MarkDirty() { m_dirty = true; }
    void SaveIfDue(int64_t nowMs);
    PriceInfo PriceFor(const std::string& name);
    // "12 ex" / "1.5 div" in the unit the settings show prices in (drawing thread).
    std::string FormatValue(double totalExalt, double totalDivine) const;
    bool HasDivineRate() const;

    // ui/Overlay.cpp
    void DrawOverlay(const ScanSnapshot& snap, int64_t nowMs);
    void DrawHighlights(const ScanSnapshot& snap, int64_t nowMs);
    void DrawDeferButton(const ScanSnapshot& snap, int64_t nowMs);
    void DrawPill(const ImVec2& anchor, const char* text, ImU32 textColor, bool alignRight);
    void DrawScanStatusLine(const ScanSnapshot& snap);
    void SetCapture(bool on);
    bool HardwareClickOn(const ImVec2& p0, const ImVec2& p1);

    // ui/SettingsUi.cpp
    void DrawSectionOverlay();
    void DrawSectionValue();
    void DrawSectionItems(const CatalogPrices* catalog, const ScanSnapshot& snap);
    void DrawSelectedList();
    void DrawWindowRewards(const ScanSnapshot& snap);
    void DrawSearchResults(const CatalogPrices* catalog);
    void DrawCategories(const CatalogPrices* catalog);
    // `name` is the item list's (English) name the tick goes by; `shown`, when given, the
    // name the game shows for it (a reward of the open window in another game language).
    bool DrawItemRow(const char* name, const PriceInfo& price, int id, const char* shown = nullptr);
    void DrawSectionClicking();
    void DrawSectionDiagnostics(const ScanSnapshot& snap);
    void EnsureCatalogKeys();
    void RecountTicked();

    Settings m_settings;
    bool m_loaded = false;
    bool m_dirty = false;
    int64_t m_nextSaveMs = 0;

    RitualScanner m_scanner;
    PriceBook m_prices;
    Win32Input m_input;
    DeferSequence m_sequence;

    // Matches for the latest snapshot, recomputed only when something they depend on changes.
    std::vector<DeferMatch> m_matches;
    uint64_t m_matchSnapSeq = 0;
    uint64_t m_matchPriceGen = 0;
    uint64_t m_matchRulesRev = 0;
    uint64_t m_rulesRev = 1;
    std::unordered_set<std::string> m_selectedKeys;
    uint64_t m_selectedKeysRev = 0;

    // Overlay input and feedback.
    bool m_captureOn = false;
    bool m_pressActive = false;
    bool m_hwWasDown = false;
    bool m_hwPressedOn = false;
    int64_t m_lastStartMs = 0;
    int64_t m_dryFlashUntil = 0;
    LocText m_notice;                // short result shown next to the button (translated when drawn)
    int64_t m_noticeUntil = 0;

    // Settings UI state.
    std::string m_search;
    std::vector<std::string> m_catalogKeys;               // NameKey per catalog entry
    std::array<int, kCategoryCount> m_ticked{};
    uint64_t m_tickedRev = 0;
};

}  // namespace RitualHelper
