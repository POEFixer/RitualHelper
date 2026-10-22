// Ritual Helper - the plugin's settings tab, drawn with the plugin UI kit (PluginUi.h):
// one padded card per topic, a measured label column per card, wrapped tooltips,
// dependent controls dimmed, diagnostics in one collapsed card at the bottom.
// Copyright (C) Omer Faruk ARPA and POEFixer contributors. Licensed under GPL-3.0 (see LICENSE).
#include "../RitualHelperPlugin.h"
#include "../data/ItemCatalog.h"
#include "../../../../POEFixer/plugin_sdk/PluginUi.h"

#include "../../../../POEFixer/imgui/IconsFontAwesome6.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <string>
#include <vector>

namespace RitualHelper {

namespace Ui = ::PluginUi;

namespace {

int64_t NowMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now().time_since_epoch()).count();
}

const char* const kUnits[] = {"Exalted", "Divine"};
constexpr int kUnitCount = 2;

// Height of a scrolling list showing up to `rows` rows.
float ListHeight(size_t rows, size_t maxRows) {
    const size_t shown = std::max<size_t>(1, std::min(rows, maxRows));
    const ImGuiStyle& st = ImGui::GetStyle();
    return static_cast<float>(shown) * (ImGui::GetFrameHeight() + st.CellPadding.y * 2.0f) + st.CellPadding.y * 2.0f + 2.0f;
}

// A quiet disclosure row (arrow and text, hover highlight) that stays inside the
// card's padding; a framed CollapsingHeader reaches past it. Nothing to pop.
bool DisclosureRow(const char* label) {
    return ImGui::TreeNodeEx(label, ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_FramePadding |
                                        ImGuiTreeNodeFlags_NoTreePushOnOpen);
}

// The kit's warning amber, darkened on a light theme where the plain amber is
// too faint to read on the pale window.
ImVec4 ReadableWarning() {
    ImVec4 w = Ui::Colors().Warning;
    const ImVec4 bg = ImGui::GetStyle().Colors[ImGuiCol_WindowBg];
    if (0.299f * bg.x + 0.587f * bg.y + 0.114f * bg.z > 0.6f) w = ImVec4(w.x * 0.72f, w.y * 0.62f, w.z * 0.4f, 1.0f);
    return w;
}

// The Diagnostics card's own row, in the heading colour.
bool DiagnosticsRow(const char* label) {
    ImGui::PushStyleColor(ImGuiCol_Text, Ui::Colors().Accent);
    const bool open = DisclosureRow(label);
    ImGui::PopStyleColor();
    return open;
}

std::string Trimmed(const std::string& s) {
    size_t b = 0, e = s.size();
    while (b < e && (s[b] == ' ' || s[b] == '\t')) ++b;
    while (e > b && (s[e - 1] == ' ' || s[e - 1] == '\t')) --e;
    return s.substr(b, e - b);
}

}  // namespace

void RitualHelperPlugin::EnsureCatalogKeys() {
    if (m_catalogKeys.size() == kItemCatalogCount) return;
    m_catalogKeys.clear();
    m_catalogKeys.reserve(kItemCatalogCount);
    for (size_t i = 0; i < kItemCatalogCount; ++i) m_catalogKeys.push_back(NameKey(kItemCatalog[i].name));
}

void RitualHelperPlugin::RecountTicked() {
    EnsureSelectedKeys();
    EnsureCatalogKeys();
    if (m_tickedRev == m_rulesRev) return;
    m_ticked.fill(0);
    for (size_t i = 0; i < kItemCatalogCount; ++i) {
        const int c = static_cast<int>(kItemCatalog[i].category);
        if (c >= 0 && c < kCategoryCount && m_selectedKeys.count(m_catalogKeys[i])) ++m_ticked[static_cast<size_t>(c)];
    }
    m_tickedRev = m_rulesRev;
}

void RitualHelperPlugin::DrawSettings() {
    if (!m_loaded || !ctx()->ImGuiContext) {
        ImGui::TextDisabled("Ritual Helper is not active (this POEFixer version may be too old for it).");
        return;
    }
    BindImGui();
    m_prices.Refresh(ctx());
    const auto catalog = m_scanner.Catalog();
    auto snap = m_scanner.Latest();
    if (!snap) snap = std::make_shared<const ScanSnapshot>();
    UpdateMatches(*snap);
    RecountTicked();

    ImGui::PushID("RitualHelper");
    ImGui::Spacing();
    Ui::Hint("Highlights the Ritual rewards you want in the Favours window and defers them with one click. "
             "Tick items below, or let a minimum value pick them for you.");
    ImGui::Spacing();

    if (Ui::WideEnoughForTwoColumns() &&
        ImGui::BeginTable("##topCards", 2, ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_NoPadOuterX)) {
        ImGui::TableNextColumn();
        DrawSectionOverlay();
        ImGui::Spacing();
        DrawSectionClicking();
        ImGui::TableNextColumn();
        DrawSectionValue();
        ImGui::EndTable();
    } else {
        DrawSectionOverlay();
        ImGui::Spacing();
        DrawSectionValue();
        ImGui::Spacing();
        DrawSectionClicking();
    }
    ImGui::Spacing();
    DrawSectionItems(catalog.get(), *snap);
    ImGui::Spacing();
    DrawSectionDiagnostics(*snap);
    ImGui::Spacing();

    char footer[160];
    std::snprintf(footer, sizeof(footer),
                  "Ritual Helper %s - originally created by Omer Faruk ARPA, maintained by POEFixer. GPL-3.0.",
                  PluginSDK_GetInfo()->version);
    Ui::Hint(footer);
    ImGui::Spacing();
    ImGui::PopID();
    SaveIfDue(NowMs());
}

void RitualHelperPlugin::DrawSectionOverlay() {
    Settings& s = m_settings;
    bool changed = false;
    Ui::BeginCard("##overlayCard");
    Ui::Heading(ICON_FA_EYE, "Overlay");
    changed |= Ui::Check("Show the overlay", "showOverlay", &s.showOverlay,
                         "Draws the highlights, the value labels and the DEFER button on the Favours window.\n"
                         "Turn it off to pause the plugin without disabling it.");
    ImGui::BeginDisabled(!s.showOverlay);
    changed |= Ui::Check("Show values on highlighted rewards", "showValues", &s.showValues,
                         "Writes each highlighted reward's price in its corner (the whole stack).");
    ImGui::Spacing();
    {
        const Ui::LabelColumn col({"Highlight colour", "Re-read the window every"});
        col.RowNoWidth("Highlight colour");
        changed |= Ui::Swatch("##highlight", s.highlight);
        Ui::Tip("Colour of the frame around each reward that will be deferred.");
        changed |= Ui::SliderIntRow(col, "Re-read the window every", "##scanInterval", &s.scanIntervalMs,
                                    kScanIntervalMinMs, kScanIntervalMaxMs, "%d ms",
                                    "How often the open Favours window is read again. Lower reacts faster after a "
                                    "reroll; higher costs less.\n"
                                    "While no Favours window is open the plugin looks about once a second.");
    }
    ImGui::EndDisabled();
    Ui::EndCard();
    if (changed) MarkDirty();
}

void RitualHelperPlugin::DrawSectionValue() {
    Settings& s = m_settings;
    bool changed = false;
    Ui::BeginCard("##valueCard");
    Ui::Heading(ICON_FA_COINS, "Defer by value");
    Ui::HelpMarker("Also highlights every revealed reward worth at least this much, ticked or not.\n"
                   "Values count the whole stack. Prices come from the overlay's price service: "
                   "Settings > Prices chooses the league and the source.");
    const float fs = ImGui::GetFontSize();
    const ImGuiStyle& st = ImGui::GetStyle();
    const Ui::LabelColumn col({"Minimum value", "Show prices in"});

    col.RowNoWidth("Minimum value");
    const float unitW = ImGui::CalcTextSize("Exalted").x + st.FramePadding.x * 2.0f + ImGui::GetFrameHeight();
    const float inputW = std::clamp(ImGui::GetContentRegionAvail().x - unitW - st.ItemSpacing.x, fs * 4.0f, fs * 8.0f);
    ImGui::SetNextItemWidth(inputW);
    if (ImGui::InputDouble("##minValue", &s.minValue, 0.0, 0.0, "%.2f")) {
        s.minValue = std::clamp(s.minValue, 0.0, kMinValueMax);
        changed = true;
    }
    Ui::Tip("Rewards worth at least this much are deferred too.\n0 turns the rule off.");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(unitW);
    if (Ui::Combo("##minUnit", s.minValueUnit, kUnits, kUnitCount)) changed = true;
    Ui::Tip("Unit of the minimum value.");

    const PluginSDK::PriceRates rates = ctx()->Prices.GetRates();
    if (s.minValue > 0.0 && rates.divineInChaos > 0.0f && rates.exaltedInChaos > 0.0f) {
        const double exPerDiv = static_cast<double>(rates.divineInChaos) / rates.exaltedInChaos;
        char eq[64];
        if (s.minValueUnit == 1) std::snprintf(eq, sizeof(eq), "= about %.0f Exalted", s.minValue * exPerDiv);
        else std::snprintf(eq, sizeof(eq), "= about %.2f Divine", s.minValue / exPerDiv);
        col.RowNoWidth("");
        ImGui::PushStyleColor(ImGuiCol_Text, Ui::Colors().TextSecondary);
        ImGui::TextUnformatted(eq);
        ImGui::PopStyleColor();
    }

    col.Row("Show prices in");
    if (Ui::Combo("##displayUnit", s.priceDisplayUnit, kUnits, kUnitCount)) changed = true;
    Ui::Tip("Unit of the value labels on the Favours window and of the prices in the item list.");

    ImGui::Spacing();
    const PluginSDK::PriceStatus ps = ctx()->Prices.GetStatus();
    if (ps.loaded) {
        char line[128];
        if (rates.divineInChaos > 0.0f && rates.exaltedInChaos > 0.0f)
            std::snprintf(line, sizeof(line), "%d prices loaded - 1 Divine = %.0f Exalted", ps.totalItems,
                          static_cast<double>(rates.divineInChaos) / rates.exaltedInChaos);
        else
            std::snprintf(line, sizeof(line), "%d prices loaded", ps.totalItems);
        Ui::Hint(line);
    } else {
        ImGui::PushStyleColor(ImGuiCol_Text, ReadableWarning());
        ImGui::PushTextWrapPos(0.0f);
        ImGui::TextUnformatted("Prices are not loaded yet: check Settings > Prices. Ticked items work without prices.");
        ImGui::PopTextWrapPos();
        ImGui::PopStyleColor();
    }
    Ui::EndCard();
    if (changed) {
        RulesChanged();
        MarkDirty();
    }
}

void RitualHelperPlugin::DrawSectionClicking() {
    Settings& s = m_settings;
    bool changed = false;
    Ui::BeginCard("##clickCard");
    Ui::Heading(ICON_FA_HAND_POINTER, "Clicking");
    {
        const Ui::LabelColumn col({"Time per click"});
        changed |= Ui::SliderIntRow(col, "Time per click", "##clickPause", &s.clickPauseMs, kClickPauseMinMs,
                                    kClickPauseMaxMs, "%d ms",
                                    "How long the cursor rests on each reward around its click.\n"
                                    "Raise it if rewards are skipped on a slow connection.");
    }
    ImGui::Spacing();
    Ui::Hint("Rewards are only clicked after the game shows defer mode is on. Right-click, Esc or moving the mouse "
             "stops a run; so do losing focus, opening the overlay menu or closing the window. The cursor returns "
             "where it was.");
    Ui::EndCard();
    if (changed) MarkDirty();
}

bool RitualHelperPlugin::DrawItemRow(const char* name, const PriceInfo& price, int id) {
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    bool on = m_selectedKeys.count(NameKey(name)) != 0;
    char label[200];
    std::snprintf(label, sizeof(label), "%s###row%d", name, id);
    // A 1 px frame keeps the box visible on the striped rows (its fill matches the
    // alternate row colour on some themes).
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
    const bool toggled = ImGui::Checkbox(label, &on);
    ImGui::PopStyleVar();
    ImGui::TableNextColumn();
    if (price.found) {
        char val[32];
        FormatValue(val, sizeof(val), price.exalt, price.divine);
        Ui::AlignRight(ImGui::CalcTextSize(val).x);
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(val);
    } else {
        Ui::AlignRight(ImGui::CalcTextSize("no price").x);
        ImGui::AlignTextToFramePadding();
        ImGui::TextDisabled("no price");
    }
    if (toggled) ToggleItem(name, on);
    return toggled;
}

void RitualHelperPlugin::DrawSectionItems(const CatalogPrices* catalog, const ScanSnapshot& snap) {
    Ui::BeginCard("##itemsCard");
    char title[96];
    std::snprintf(title, sizeof(title), "Items to defer (%zu ticked)###itemsHeading", m_settings.selectedItems.size());
    Ui::Heading(ICON_FA_LIST_CHECK, title);
    Ui::HelpMarker("Ticked items are highlighted whenever they appear in the Favours window, priced or not.\n"
                   "Names are matched as the game writes them. The list below is in English; with another game "
                   "language, tick rewards under \"In the open Favours window\" instead.");

    // One field searches every category; a name the list does not know can be added as typed.
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
    Ui::InputString("##search", ICON_FA_MAGNIFYING_GLASS "  Search every item, or type a name to add", m_search);
    Ui::Tip("Filters all categories at once.\nEvent rewards missing from the list can be added by typing the exact name.");
    const std::string typed = Trimmed(m_search);
    if (!typed.empty() && typed.size() <= kMaxNameBytes && !m_selectedKeys.count(NameKey(typed))) {
        char add[200];
        std::snprintf(add, sizeof(add), "%s  Tick \"%.60s\"###addTyped", ICON_FA_PLUS, typed.c_str());
        if (Ui::SubtleButton(add)) {
            ToggleItem(typed, true);
            m_search.clear();
        }
        Ui::Tip("Adds this name as typed. Use it for event rewards the list does not contain.");
    }
    ImGui::Spacing();

    DrawSelectedList();
    DrawWindowRewards(snap);
    ImGui::Spacing();
    if (!typed.empty()) DrawSearchResults(catalog);
    else DrawCategories(catalog);
    Ui::EndCard();
}

void RitualHelperPlugin::DrawSelectedList() {
    const size_t n = m_settings.selectedItems.size();
    char hdr[64];
    std::snprintf(hdr, sizeof(hdr), "Ticked (%zu)###ticked", n);
    if (!DisclosureRow(hdr)) return;
    if (n == 0) {
        Ui::Hint("Nothing ticked yet.");
        return;
    }
    const float fs = ImGui::GetFontSize();
    std::string remove;
    if (ImGui::BeginTable("##tickedList", 3,
                          ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_BordersOuter | ImGuiTableFlags_PadOuterX,
                          ImVec2(0.0f, ListHeight(n, 8)))) {
        ImGui::TableSetupColumn("##x", ImGuiTableColumnFlags_WidthFixed, ImGui::GetFrameHeight());
        ImGui::TableSetupColumn("##name", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("##price", ImGuiTableColumnFlags_WidthFixed, fs * 6.0f);
        ImGuiListClipper clip;
        clip.Begin(static_cast<int>(n));
        while (clip.Step()) {
            for (int i = clip.DisplayStart; i < clip.DisplayEnd; ++i) {
                const std::string& name = m_settings.selectedItems[static_cast<size_t>(i)];
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::PushID(i);
                // A quiet square button on the row's frame height, level with the name.
                if (Ui::SubtleButton(ICON_FA_XMARK "###untick", ImVec2(ImGui::GetFrameHeight(), 0.0f))) remove = name;
                Ui::Tip("Untick");
                ImGui::PopID();
                ImGui::TableNextColumn();
                ImGui::AlignTextToFramePadding();
                const bool clipped = ImGui::CalcTextSize(name.c_str()).x > ImGui::GetContentRegionAvail().x;
                ImGui::TextUnformatted(name.c_str());
                if (clipped) Ui::Tip(name.c_str());
                ImGui::TableNextColumn();
                const PriceInfo p = PriceFor(name);
                ImGui::AlignTextToFramePadding();
                if (p.found) {
                    char val[32];
                    FormatValue(val, sizeof(val), p.exalt, p.divine);
                    Ui::AlignRight(ImGui::CalcTextSize(val).x);
                    ImGui::TextUnformatted(val);
                } else {
                    Ui::AlignRight(ImGui::CalcTextSize("no price").x);
                    ImGui::TextDisabled("no price");
                }
            }
        }
        ImGui::EndTable();
    }
    if (!remove.empty()) ToggleItem(remove, false);

    if (Ui::SubtleButton(ICON_FA_TRASH "  Untick all###clearAll")) ImGui::OpenPopup("##confirmClear");
    Ui::Tip("Removes every ticked item. Asks first.");
    if (Ui::ConfirmPopup("##confirmClear", "Untick all items?", "Untick all", "Keep")) {
        m_settings.selectedItems.clear();
        RulesChanged();
        MarkDirty();
    }
    ImGui::Spacing();
}

void RitualHelperPlugin::DrawWindowRewards(const ScanSnapshot& snap) {
    if (!DisclosureRow("In the open Favours window###windowRewards")) return;
    if (!snap.windowOpen) {
        Ui::Hint("Open a Favours window in game to tick its rewards here.");
        return;
    }
    std::vector<const RitualReward*> unique;
    for (const auto& r : snap.window.rewards) {
        if (IsHiddenReward(r)) continue;
        const std::string key = NameKey(r.name);
        if (std::none_of(unique.begin(), unique.end(), [&](const RitualReward* u) { return NameKey(u->name) == key; }))
            unique.push_back(&r);
    }
    if (unique.empty()) {
        Ui::Hint("No revealed rewards in this window.");
        return;
    }
    const float fs = ImGui::GetFontSize();
    if (ImGui::BeginTable("##windowList", 2,
                          ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_BordersOuter | ImGuiTableFlags_PadOuterX,
                          ImVec2(0.0f, ListHeight(unique.size(), 8)))) {
        ImGui::TableSetupColumn("##name", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("##price", ImGuiTableColumnFlags_WidthFixed, fs * 6.0f);
        for (size_t i = 0; i < unique.size(); ++i) {
            const RitualReward& r = *unique[i];
            DrawItemRow(r.name.c_str(), UsablePrice(r, PriceFor(r.name)), 100000 + static_cast<int>(i));
        }
        ImGui::EndTable();
    }
    ImGui::Spacing();
}

void RitualHelperPlugin::DrawSearchResults(const CatalogPrices* catalog) {
    const std::string typed = Trimmed(m_search);
    std::vector<size_t> hits;
    for (size_t i = 0; i < kItemCatalogCount && hits.size() < 400; ++i)
        if (ContainsCI(kItemCatalog[i].name, typed)) hits.push_back(i);
    if (hits.empty()) {
        Ui::Hint("No item in the list matches. Use the button above to tick the name as typed.");
        return;
    }
    const float fs = ImGui::GetFontSize();
    if (ImGui::BeginTable("##searchList", 3,
                          ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_BordersOuter | ImGuiTableFlags_PadOuterX,
                          ImVec2(0.0f, ListHeight(hits.size(), 12)))) {
        ImGui::TableSetupColumn("##name", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("##price", ImGuiTableColumnFlags_WidthFixed, fs * 6.0f);
        ImGui::TableSetupColumn("##cat", ImGuiTableColumnFlags_WidthFixed, fs * 8.0f);
        ImGuiListClipper clip;
        clip.Begin(static_cast<int>(hits.size()));
        while (clip.Step()) {
            for (int row = clip.DisplayStart; row < clip.DisplayEnd; ++row) {
                const size_t i = hits[static_cast<size_t>(row)];
                const PriceInfo p = catalog && i < catalog->perEntry.size() ? catalog->perEntry[i] : PriceInfo{};
                DrawItemRow(kItemCatalog[i].name, p, static_cast<int>(i));
                ImGui::TableNextColumn();
                ImGui::AlignTextToFramePadding();
                ImGui::TextDisabled("%s", CategoryName(kItemCatalog[i].category));
            }
        }
        ImGui::EndTable();
    }
}

void RitualHelperPlugin::DrawCategories(const CatalogPrices* catalog) {
    if (!catalog) {
        Ui::Hint("Loading the item list...");
        return;
    }
    if (!catalog->loaded) Ui::Hint("Prices are not loaded yet, so the lists are alphabetical.");
    const float fs = ImGui::GetFontSize();
    for (int c = 0; c < kCategoryCount; ++c) {
        const auto& list = catalog->sorted[static_cast<size_t>(c)];
        if (list.empty()) continue;
        char hdr[96];
        std::snprintf(hdr, sizeof(hdr), "%s  (%d/%zu)###cat%d", CategoryName(static_cast<Category>(c)),
                      m_ticked[static_cast<size_t>(c)], list.size(), c);
        ImGui::PushID(c);
        if (DisclosureRow(hdr)) {
            if (Ui::SubtleButton("Tick all###all")) {
                for (uint16_t i : list) m_settings.Select(kItemCatalog[i].name, true);
                RulesChanged();
                MarkDirty();
            }
            Ui::Tip("Ticks every item of this category.");
            ImGui::SameLine();
            if (Ui::SubtleButton("Untick all###none")) {
                for (uint16_t i : list) m_settings.Select(kItemCatalog[i].name, false);
                RulesChanged();
                MarkDirty();
            }
            Ui::Tip("Unticks every item of this category.");
            if (ImGui::BeginTable("##list", 2,
                                  ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_BordersOuter |
                                      ImGuiTableFlags_PadOuterX,
                                  ImVec2(0.0f, ListHeight(list.size(), 10)))) {
                ImGui::TableSetupColumn("##name", ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableSetupColumn("##price", ImGuiTableColumnFlags_WidthFixed, fs * 6.0f);
                ImGuiListClipper clip;
                clip.Begin(static_cast<int>(list.size()));
                while (clip.Step()) {
                    for (int row = clip.DisplayStart; row < clip.DisplayEnd; ++row) {
                        const uint16_t i = list[static_cast<size_t>(row)];
                        DrawItemRow(kItemCatalog[i].name, catalog->perEntry[i], static_cast<int>(i));
                    }
                }
                ImGui::EndTable();
            }
            ImGui::Spacing();
        }
        ImGui::PopID();
    }
}

void RitualHelperPlugin::DrawSectionDiagnostics(const ScanSnapshot& snap) {
    Ui::BeginCard("##diagCard");
    const bool open = DiagnosticsRow(ICON_FA_BUG "  Diagnostics###diag");
    Ui::Tip("Dry run, on-screen scan status and what the plugin sees of the Favours window.");
    if (!open) {
        Ui::EndCard();
        return;
    }
    Settings& s = m_settings;
    bool changed = false;
    changed |= Ui::Check("Dry run (never click)", "dryRun", &s.dryRun,
                         "The DEFER button only writes the rewards it would defer to the log and flashes their "
                         "frames.\nNo clicks are sent.");
    changed |= Ui::Check("Show scan status on screen", "scanStatus", &s.showScanStatus,
                         "One line in the top-left corner: rewards found, rewards highlighted and whether the "
                         "game's buttons were recognised.");
    if (changed) MarkDirty();
    ImGui::Spacing();

    {
        const Ui::LabelColumn col({"Favours window", "Bottom button", "Hourglass", "Last UI search", "Price list"});
        char buf[160];
        col.RowNoWidth("Favours window");
        if (snap.windowOpen)
            std::snprintf(buf, sizeof(buf), "open, %dx%d grid, %zu rewards, %zu highlighted", snap.window.boxesX,
                          snap.window.boxesY, snap.window.rewards.size(), m_matches.size());
        else
            std::snprintf(buf, sizeof(buf), "not open");
        ImGui::TextUnformatted(buf);
        col.RowNoWidth("Bottom button");
        ImGui::TextUnformatted(!snap.bottomFound ? "not found"
                               : snap.bottom == BottomMode::DeferItem ? "Defer Item (defer mode on)" : "Offer Tribute");
        col.RowNoWidth("Hourglass");
        ImGui::TextUnformatted(snap.toggleFound ? "found" : "not found");
        col.RowNoWidth("Last UI search");
        std::snprintf(buf, sizeof(buf), "%d elements in %.0f ms (%d searches)", snap.lastWalkNodes,
                      static_cast<double>(snap.lastWalkMs), snap.walks);
        ImGui::TextUnformatted(buf);
        col.RowNoWidth("Price list");
        const auto catalog = m_scanner.Catalog();
        size_t priced = 0;
        if (catalog)
            for (const auto& p : catalog->perEntry) priced += p.found ? 1 : 0;
        std::snprintf(buf, sizeof(buf), "%zu names, %zu priced", kItemCatalogCount, priced);
        ImGui::TextUnformatted(buf);
    }
    if (!snap.note.empty()) Ui::Hint(snap.note.c_str());

    if (snap.windowOpen && !snap.window.rewards.empty()) {
        ImGui::Spacing();
        ImGui::TextUnformatted("Defer preview");
        const float fs = ImGui::GetFontSize();
        if (ImGui::BeginTable("##preview", 3,
                              ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_BordersOuter |
                                  ImGuiTableFlags_PadOuterX,
                              ImVec2(0.0f, ListHeight(snap.window.rewards.size(), 10)))) {
            ImGui::TableSetupColumn("##state", ImGuiTableColumnFlags_WidthFixed, ImGui::CalcTextSize("by value").x);
            ImGui::TableSetupColumn("##name", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("##price", ImGuiTableColumnFlags_WidthFixed, fs * 6.0f);
            for (size_t i = 0; i < snap.window.rewards.size(); ++i) {
                const RitualReward& r = snap.window.rewards[i];
                const DeferMatch* m = nullptr;
                for (const auto& x : m_matches)
                    if (x.reward == i) { m = &x; break; }
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                if (!m) ImGui::TextDisabled("skip");
                else ImGui::TextColored(Ui::Colors().Accent, "%s", m->selected ? "ticked" : "by value");
                ImGui::TableNextColumn();
                if (r.stack > 1) ImGui::Text("%s x%d", r.name.c_str(), r.stack);
                else ImGui::TextUnformatted(r.name.c_str());
                ImGui::TableNextColumn();
                const PriceInfo p = UsablePrice(r, PriceFor(r.name));
                if (p.found) {
                    char val[32];
                    const double k = r.stack > 1 ? r.stack : 1;
                    FormatValue(val, sizeof(val), p.exalt * k, p.divine * k);
                    ImGui::TextUnformatted(val);
                } else {
                    ImGui::TextDisabled("no price");
                }
            }
            ImGui::EndTable();
        }
    }

    ImGui::Spacing();
    if (Ui::SubtleButton(ICON_FA_FILE_EXPORT "  Write discovery dump###dump"))
        m_scanner.RequestDump(DirectoryPath() / L"debug" / L"ritual-dump.txt");
    Ui::Tip("Writes what the plugin sees of the Favours window and the game UI around it to debug\\ritual-dump.txt "
            "in the plugin folder. Useful when the window or its buttons stop being recognised after a game patch.");
    const std::string dump = m_scanner.DumpResult();
    if (!dump.empty()) Ui::Hint(dump.c_str());
    Ui::EndCard();
}

}  // namespace RitualHelper
