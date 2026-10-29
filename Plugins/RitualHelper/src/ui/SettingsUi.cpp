// Ritual Helper - the plugin's settings tab, drawn with the plugin UI kit (PluginUi.h):
// one padded card per topic, a measured label column per card, wrapped tooltips,
// dependent controls dimmed, diagnostics in one collapsed card at the bottom.
// Every text a user reads is a key of lang/en.json (ui/UiText.h); the reward names in the
// lists are the game's own and stay as the catalog spells them.
// Copyright (C) Omer Faruk ARPA and POEFixer contributors. Licensed under GPL-3.0 (see LICENSE).
#include "../RitualHelperPlugin.h"
#include "../data/ItemCatalog.h"
#include "../../../../POEFixer/plugin_sdk/PluginUi.h"
#include "UiText.h"

#include "../../../../POEFixer/imgui/IconsFontAwesome6.h"

#include <algorithm>
#include <chrono>
#include <string>
#include <vector>

namespace RitualHelper {

namespace Ui = ::PluginUi;

namespace {

int64_t NowMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now().time_since_epoch()).count();
}

constexpr int kUnitCount = 2;

// Height of a scrolling list showing up to `rows` rows.
float ListHeight(size_t rows, size_t maxRows) {
    const size_t shown = std::max<size_t>(1, std::min(rows, maxRows));
    const ImGuiStyle& st = ImGui::GetStyle();
    return static_cast<float>(shown) * (ImGui::GetFrameHeight() + st.CellPadding.y * 2.0f) + st.CellPadding.y * 2.0f + 2.0f;
}

// Width of a list's price column: 6 em, or the translated "no price" when that is wider.
float PriceColumnWidth() {
    return std::max(ImGui::GetFontSize() * 6.0f,
                    ImGui::CalcTextSize(I18n::Tr("no_price")).x + ImGui::GetStyle().CellPadding.x * 2.0f);
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

std::string Trimmed(const std::string& s) {
    size_t b = 0, e = s.size();
    while (b < e && (s[b] == ' ' || s[b] == '\t')) ++b;
    while (e > b && (s[e - 1] == ' ' || s[e - 1] == '\t')) --e;
    return s.substr(b, e - b);
}

// The section's name in the program language; the cases follow the enum's order.
const char* CategoryText(Category c) {
    switch (c) {
        case Category::Currency:        return I18n::Tr("category_currency");
        case Category::Essence:         return I18n::Tr("category_essences");
        case Category::Rune:            return I18n::Tr("category_runes");
        case Category::SoulCore:        return I18n::Tr("category_soul_cores");
        case Category::Catalyst:        return I18n::Tr("category_catalysts");
        case Category::Delirium:        return I18n::Tr("category_delirium");
        case Category::Incursion:       return I18n::Tr("category_incursion");
        case Category::Idol:            return I18n::Tr("category_idols");
        case Category::Ritual:          return I18n::Tr("category_omens");
        case Category::VaultKey:        return I18n::Tr("category_vault_keys");
        case Category::Fragment:        return I18n::Tr("category_fragments");
        case Category::Abyss:           return I18n::Tr("category_abyss");
        case Category::UncutGem:        return I18n::Tr("category_uncut_gems");
        case Category::LineageGem:      return I18n::Tr("category_lineage_gems");
        case Category::Verisium:        return I18n::Tr("category_verisium");
        case Category::Vaal:            return I18n::Tr("category_vaal");
        case Category::Expedition:      return I18n::Tr("category_expedition");
        case Category::UniqueWeapon:    return I18n::Tr("category_unique_weapons");
        case Category::UniqueArmour:    return I18n::Tr("category_unique_armour");
        case Category::UniqueAccessory: return I18n::Tr("category_unique_accessories");
        case Category::UniqueJewel:     return I18n::Tr("category_unique_jewels");
        case Category::UniqueFlask:     return I18n::Tr("category_unique_flasks");
        case Category::UniqueMap:       return I18n::Tr("category_unique_maps");
        case Category::UniqueSanctum:   return I18n::Tr("category_unique_relics");
        default:                        return "";
    }
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
        ImGui::TextDisabled("%s", I18n::Tr("settings_inactive"));
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
    Ui::Hint(I18n::Tr("settings_intro"));
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

    Ui::Hint("Originally created by Omer Faruk ARPA. Maintained by POEFixer. GPL-3.0.");
    ImGui::Spacing();
    ImGui::PopID();
    SaveIfDue(NowMs());
}

void RitualHelperPlugin::DrawSectionOverlay() {
    Settings& s = m_settings;
    bool changed = false;
    Ui::BeginCard("##overlayCard");
    Ui::Heading(ICON_FA_EYE, I18n::Tr("overlay_heading"));
    const std::string showTip = I18n::TrF("overlay_show_tip", { { "button", I18n::Tr("button_defer") } });
    changed |= Ui::Check(I18n::Tr("overlay_show"), "showOverlay", &s.showOverlay, showTip.c_str());
    ImGui::BeginDisabled(!s.showOverlay);
    changed |= Ui::Check(I18n::Tr("overlay_show_values"), "showValues", &s.showValues,
                         I18n::Tr("overlay_show_values_tip"));
    ImGui::Spacing();
    {
        const char* colourLabel = I18n::Tr("overlay_highlight_colour");
        const char* rescanLabel = I18n::Tr("overlay_rescan");
        const Ui::LabelColumn col({colourLabel, rescanLabel});
        col.RowNoWidth(colourLabel);
        changed |= Ui::Swatch("##highlight", s.highlight);
        Ui::Tip(I18n::Tr("overlay_highlight_colour_tip"));
        const std::string ms = SliderFormat(I18n::Tr("unit_ms"));
        changed |= Ui::SliderIntRow(col, rescanLabel, "##scanInterval", &s.scanIntervalMs, kScanIntervalMinMs,
                                    kScanIntervalMaxMs, ms.c_str(), I18n::Tr("overlay_rescan_tip"));
    }
    ImGui::EndDisabled();
    Ui::EndCard();
    if (changed) MarkDirty();
}

void RitualHelperPlugin::DrawSectionValue() {
    Settings& s = m_settings;
    bool changed = false;
    Ui::BeginCard("##valueCard");
    Ui::Heading(ICON_FA_COINS, I18n::Tr("value_heading"), I18n::Tr("value_heading_help"));
    const float fs = ImGui::GetFontSize();
    const ImGuiStyle& st = ImGui::GetStyle();
    const char* minLabel = I18n::Tr("value_minimum");
    const char* showInLabel = I18n::Tr("value_show_in");
    const Ui::LabelColumn col({minLabel, showInLabel});
    // Built every frame: the unit names follow the program language.
    const char* const units[kUnitCount] = {I18n::Tr("unit_exalted"), I18n::Tr("unit_divine")};

    col.RowNoWidth(minLabel);
    const float unitW = std::max(ImGui::CalcTextSize(units[0]).x, ImGui::CalcTextSize(units[1]).x) +
                        st.FramePadding.x * 2.0f + ImGui::GetFrameHeight();
    const float inputW = std::clamp(ImGui::GetContentRegionAvail().x - unitW - st.ItemSpacing.x, fs * 4.0f, fs * 8.0f);
    ImGui::SetNextItemWidth(inputW);
    if (ImGui::InputDouble("##minValue", &s.minValue, 0.0, 0.0, "%.2f")) {
        s.minValue = std::clamp(s.minValue, 0.0, kMinValueMax);
        changed = true;
    }
    Ui::Tip(I18n::Tr("value_minimum_tip"));
    ImGui::SameLine();
    ImGui::SetNextItemWidth(unitW);
    if (Ui::Combo("##minUnit", s.minValueUnit, units, kUnitCount)) changed = true;
    Ui::Tip(I18n::Tr("value_unit_tip"));

    const PluginSDK::PriceRates rates = ctx()->Prices.GetRates();
    if (s.minValue > 0.0 && rates.divineInChaos > 0.0f && rates.exaltedInChaos > 0.0f) {
        const double exPerDiv = static_cast<double>(rates.divineInChaos) / rates.exaltedInChaos;
        const std::string eq =
            s.minValueUnit == 1
                ? I18n::TrF("value_about_exalted", { { "amount", Num("%.0f", s.minValue * exPerDiv) } })
                : I18n::TrF("value_about_divine", { { "amount", Num("%.2f", s.minValue / exPerDiv) } });
        col.RowNoWidth("");
        ImGui::PushStyleColor(ImGuiCol_Text, Ui::Colors().TextSecondary);
        ImGui::TextUnformatted(eq.c_str());
        ImGui::PopStyleColor();
    }

    col.Row(showInLabel);
    if (Ui::Combo("##displayUnit", s.priceDisplayUnit, units, kUnitCount)) changed = true;
    Ui::Tip(I18n::Tr("value_display_unit_tip"));

    ImGui::Spacing();
    const PluginSDK::PriceStatus ps = ctx()->Prices.GetStatus();
    if (ps.loaded) {
        const std::string count = std::to_string(ps.totalItems);
        const std::string line =
            rates.divineInChaos > 0.0f && rates.exaltedInChaos > 0.0f
                ? I18n::TrF("value_prices_loaded_rate",
                            { { "count", count },
                              { "rate", Num("%.0f", static_cast<double>(rates.divineInChaos) / rates.exaltedInChaos) } })
                : I18n::TrF("value_prices_loaded", { { "count", count } });
        Ui::Hint(line.c_str());
    } else {
        ImGui::PushStyleColor(ImGuiCol_Text, ReadableWarning());
        ImGui::PushTextWrapPos(0.0f);
        ImGui::TextUnformatted(I18n::Tr("value_prices_missing"));
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
    Ui::Heading(ICON_FA_HAND_POINTER, I18n::Tr("click_heading"));
    {
        const char* timeLabel = I18n::Tr("click_time");
        const Ui::LabelColumn col({timeLabel});
        const std::string ms = SliderFormat(I18n::Tr("unit_ms"));
        changed |= Ui::SliderIntRow(col, timeLabel, "##clickPause", &s.clickPauseMs, kClickPauseMinMs,
                                    kClickPauseMaxMs, ms.c_str(), I18n::Tr("click_time_tip"));
    }
    ImGui::Spacing();
    Ui::Hint(I18n::Tr("click_hint"));
    Ui::EndCard();
    if (changed) MarkDirty();
}

bool RitualHelperPlugin::DrawItemRow(const char* name, const PriceInfo& price, int id, const char* shown) {
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    // A reward of the open window is ticked under its English name or under the name the game
    // shows (1.5.8 stored that one on a non-English client): the box shows either.
    bool on = m_selectedKeys.count(NameKey(name)) != 0 || (shown && m_selectedKeys.count(NameKey(shown)) != 0);
    // The game's reward name; the id stays the row's in every language.
    const std::string label = std::string(shown ? shown : name) + "###row" + std::to_string(id);
    // A 1 px frame keeps the box visible on the striped rows (its fill matches the
    // alternate row colour on some themes).
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
    const bool toggled = ImGui::Checkbox(label.c_str(), &on);
    ImGui::PopStyleVar();
    ImGui::TableNextColumn();
    if (price.found) {
        const std::string val = FormatValue(price.exalt, price.divine);
        Ui::AlignRight(ImGui::CalcTextSize(val.c_str()).x);
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(val.c_str());
    } else {
        const char* none = I18n::Tr("no_price");
        Ui::AlignRight(ImGui::CalcTextSize(none).x);
        ImGui::AlignTextToFramePadding();
        ImGui::TextDisabled("%s", none);
    }
    if (toggled) {
        if (shown) {
            // Unticking clears both names; ticking stores the English one.
            m_settings.SelectReward(name, shown, on);
            RulesChanged();
            MarkDirty();
        } else {
            ToggleItem(name, on);
        }
    }
    return toggled;
}

void RitualHelperPlugin::DrawSectionItems(const CatalogPrices* catalog, const ScanSnapshot& snap) {
    Ui::BeginCard("##itemsCard");
    const std::string title =
        I18n::TrF("items_heading", { { "count", std::to_string(m_settings.selectedItems.size()) } });
    const std::string help = I18n::TrF("items_heading_help", { { "section", I18n::Tr("items_window_rewards") } });
    Ui::Heading(ICON_FA_LIST_CHECK, title.c_str(), help.c_str());

    // One field searches every category; a name the list does not know can be added as typed.
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
    const std::string searchHint = std::string(ICON_FA_MAGNIFYING_GLASS "  ") + I18n::Tr("items_search_hint");
    Ui::InputString("##search", searchHint.c_str(), m_search);
    Ui::Tip(I18n::Tr("items_search_tip"));
    const std::string typed = Trimmed(m_search);
    if (!typed.empty() && typed.size() <= kMaxNameBytes && !m_selectedKeys.count(NameKey(typed))) {
        const std::string add = std::string(ICON_FA_PLUS "  ") +
                                I18n::TrF("items_add_typed", { { "name", ShortenChars(typed, 60) } }) + "###addTyped";
        if (Ui::SubtleButton(add.c_str())) {
            ToggleItem(typed, true);
            m_search.clear();
        }
        Ui::Tip(I18n::Tr("items_add_typed_tip"));
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
    const std::string hdr = I18n::TrF("items_ticked", { { "count", std::to_string(n) } }) + "###ticked";
    if (!DisclosureRow(hdr.c_str())) return;
    if (n == 0) {
        Ui::Hint(I18n::Tr("items_nothing_ticked"));
        return;
    }
    std::string remove;
    if (ImGui::BeginTable("##tickedList", 3,
                          ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_BordersOuter | ImGuiTableFlags_PadOuterX,
                          ImVec2(0.0f, ListHeight(n, 8)))) {
        ImGui::TableSetupColumn("##x", ImGuiTableColumnFlags_WidthFixed, ImGui::GetFrameHeight());
        ImGui::TableSetupColumn("##name", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("##price", ImGuiTableColumnFlags_WidthFixed, PriceColumnWidth());
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
                Ui::Tip(I18n::Tr("items_untick_tip"));
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
                    const std::string val = FormatValue(p.exalt, p.divine);
                    Ui::AlignRight(ImGui::CalcTextSize(val.c_str()).x);
                    ImGui::TextUnformatted(val.c_str());
                } else {
                    const char* none = I18n::Tr("no_price");
                    Ui::AlignRight(ImGui::CalcTextSize(none).x);
                    ImGui::TextDisabled("%s", none);
                }
            }
        }
        ImGui::EndTable();
    }
    if (!remove.empty()) ToggleItem(remove, false);

    const std::string clearAll = std::string(ICON_FA_TRASH "  ") + I18n::Tr("items_untick_all") + "###clearAll";
    if (Ui::SubtleButton(clearAll.c_str())) ImGui::OpenPopup("##confirmClear");
    Ui::Tip(I18n::Tr("items_untick_all_tip"));
    if (Ui::ConfirmPopup("##confirmClear", I18n::Tr("items_untick_confirm"), I18n::Tr("items_untick_all"),
                         I18n::Tr("items_keep"))) {
        m_settings.selectedItems.clear();
        RulesChanged();
        MarkDirty();
    }
    ImGui::Spacing();
}

void RitualHelperPlugin::DrawWindowRewards(const ScanSnapshot& snap) {
    if (!DisclosureRow(I18n::TrId("items_window_rewards"))) return;
    if (!snap.windowOpen) {
        Ui::Hint(I18n::Tr("items_window_closed"));
        return;
    }
    std::vector<const RitualReward*> unique;
    for (const auto& r : snap.window.rewards) {
        if (IsHiddenReward(r)) continue;
        // One row per item: by its English identity name, which the tick and the price go by.
        const std::string key = NameKey(IdentityName(r));
        if (std::none_of(unique.begin(), unique.end(),
                         [&](const RitualReward* u) { return NameKey(IdentityName(*u)) == key; }))
            unique.push_back(&r);
    }
    if (unique.empty()) {
        Ui::Hint(I18n::Tr("items_window_empty"));
        return;
    }
    if (ImGui::BeginTable("##windowList", 2,
                          ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_BordersOuter | ImGuiTableFlags_PadOuterX,
                          ImVec2(0.0f, ListHeight(unique.size(), 8)))) {
        ImGui::TableSetupColumn("##name", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("##price", ImGuiTableColumnFlags_WidthFixed, PriceColumnWidth());
        for (size_t i = 0; i < unique.size(); ++i) {
            const RitualReward& r = *unique[i];
            DrawItemRow(IdentityName(r).c_str(), UsablePrice(r, PriceFor(IdentityName(r))), 100000 + static_cast<int>(i),
                        r.name.c_str());
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
        Ui::Hint(I18n::Tr("items_search_none"));
        return;
    }
    if (ImGui::BeginTable("##searchList", 3,
                          ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_BordersOuter | ImGuiTableFlags_PadOuterX,
                          ImVec2(0.0f, ListHeight(hits.size(), 12)))) {
        ImGui::TableSetupColumn("##name", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("##price", ImGuiTableColumnFlags_WidthFixed, PriceColumnWidth());
        // Width 0: the column fits the translated category names it shows (no Resizable flag).
        ImGui::TableSetupColumn("##cat", ImGuiTableColumnFlags_WidthFixed, 0.0f);
        ImGuiListClipper clip;
        clip.Begin(static_cast<int>(hits.size()));
        while (clip.Step()) {
            for (int row = clip.DisplayStart; row < clip.DisplayEnd; ++row) {
                const size_t i = hits[static_cast<size_t>(row)];
                const PriceInfo p = catalog && i < catalog->perEntry.size() ? catalog->perEntry[i] : PriceInfo{};
                DrawItemRow(kItemCatalog[i].name, p, static_cast<int>(i));
                ImGui::TableNextColumn();
                ImGui::AlignTextToFramePadding();
                ImGui::TextDisabled("%s", CategoryText(kItemCatalog[i].category));
            }
        }
        ImGui::EndTable();
    }
}

void RitualHelperPlugin::DrawCategories(const CatalogPrices* catalog) {
    if (!catalog) {
        Ui::Hint(I18n::Tr("items_loading"));
        return;
    }
    if (!catalog->loaded) Ui::Hint(I18n::Tr("items_alphabetical"));
    for (int c = 0; c < kCategoryCount; ++c) {
        const auto& list = catalog->sorted[static_cast<size_t>(c)];
        if (list.empty()) continue;
        // The name, then "(ticked/all)"; the id stays the category's in every language.
        const std::string hdr = std::string(CategoryText(static_cast<Category>(c))) + "  (" +
                                std::to_string(m_ticked[static_cast<size_t>(c)]) + "/" + std::to_string(list.size()) +
                                ")###cat" + std::to_string(c);
        ImGui::PushID(c);
        if (DisclosureRow(hdr.c_str())) {
            if (Ui::SubtleButton(I18n::TrId("items_tick_all"))) {
                for (uint16_t i : list) m_settings.Select(kItemCatalog[i].name, true);
                RulesChanged();
                MarkDirty();
            }
            Ui::Tip(I18n::Tr("items_tick_all_tip"));
            ImGui::SameLine();
            const std::string untick = std::string(I18n::Tr("items_untick_all")) + "###none";
            if (Ui::SubtleButton(untick.c_str())) {
                for (uint16_t i : list) m_settings.Select(kItemCatalog[i].name, false);
                RulesChanged();
                MarkDirty();
            }
            Ui::Tip(I18n::Tr("items_untick_category_tip"));
            if (ImGui::BeginTable("##list", 2,
                                  ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_BordersOuter |
                                      ImGuiTableFlags_PadOuterX,
                                  ImVec2(0.0f, ListHeight(list.size(), 10)))) {
                ImGui::TableSetupColumn("##name", ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableSetupColumn("##price", ImGuiTableColumnFlags_WidthFixed, PriceColumnWidth());
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
    if (!Ui::BeginDiagnostics(I18n::Tr("diag_tip"))) return;
    Settings& s = m_settings;
    bool changed = false;
    const std::string dryRunTip = I18n::TrF("diag_dry_run_tip", { { "button", I18n::Tr("button_defer") } });
    changed |= Ui::Check(I18n::Tr("diag_dry_run"), "dryRun", &s.dryRun, dryRunTip.c_str());
    changed |= Ui::Check(I18n::Tr("diag_scan_status"), "scanStatus", &s.showScanStatus, I18n::Tr("diag_scan_status_tip"));
    if (changed) MarkDirty();
    ImGui::Spacing();

    {
        const char* windowLabel = I18n::Tr("diag_window");
        const char* bottomLabel = I18n::Tr("diag_bottom");
        const char* hourglassLabel = I18n::Tr("diag_hourglass");
        const char* searchLabel = I18n::Tr("diag_last_search");
        const char* pricesLabel = I18n::Tr("diag_price_list");
        const Ui::LabelColumn col({windowLabel, bottomLabel, hourglassLabel, searchLabel, pricesLabel});
        col.RowNoWidth(windowLabel);
        const std::string window =
            snap.windowOpen ? I18n::TrF("diag_window_open", { { "cols", std::to_string(snap.window.boxesX) },
                                                              { "rows", std::to_string(snap.window.boxesY) },
                                                              { "rewards", std::to_string(snap.window.rewards.size()) },
                                                              { "highlighted", std::to_string(m_matches.size()) } })
                            : std::string(I18n::Tr("diag_window_not_open"));
        ImGui::TextUnformatted(window.c_str());
        col.RowNoWidth(bottomLabel);
        // The game's own button text as the game writes it (what the scanner matched).
        const std::string gameText = !snap.bottomText.empty()                ? snap.bottomText
                                     : snap.bottom == BottomMode::DeferItem ? std::string("Defer Item")
                                                                            : std::string("Offer Tribute");
        const std::string bottom = !snap.bottomFound ? std::string(I18n::Tr("state_not_found"))
                                   : snap.bottom == BottomMode::DeferItem
                                       ? I18n::TrF("diag_bottom_defer", { { "button", gameText } })
                                       : gameText;
        ImGui::TextUnformatted(bottom.c_str());
        col.RowNoWidth(hourglassLabel);
        ImGui::TextUnformatted(snap.toggleFound ? I18n::Tr("state_found") : I18n::Tr("state_not_found"));
        col.RowNoWidth(searchLabel);
        const std::string search =
            I18n::TrF("diag_last_search_value", { { "elements", std::to_string(snap.lastWalkNodes) },
                                                  { "ms", Num("%.0f", static_cast<double>(snap.lastWalkMs)) },
                                                  { "searches", std::to_string(snap.walks) } });
        ImGui::TextUnformatted(search.c_str());
        col.RowNoWidth(pricesLabel);
        const auto catalog = m_scanner.Catalog();
        size_t priced = 0;
        if (catalog)
            for (const auto& p : catalog->perEntry) priced += p.found ? 1 : 0;
        const std::string prices = I18n::TrF("diag_price_list_value", { { "names", std::to_string(kItemCatalogCount) },
                                                                        { "priced", std::to_string(priced) } });
        ImGui::TextUnformatted(prices.c_str());
    }
    if (!snap.note.empty()) Ui::Hint(ToText(snap.note).c_str());

    if (snap.windowOpen && !snap.window.rewards.empty()) {
        ImGui::Spacing();
        ImGui::TextUnformatted(I18n::Tr("diag_preview"));
        const char* skipText = I18n::Tr("preview_skip");
        const char* tickedText = I18n::Tr("preview_ticked");
        const char* byValueText = I18n::Tr("preview_by_value");
        const float stateW = std::max({ImGui::CalcTextSize(skipText).x, ImGui::CalcTextSize(tickedText).x,
                                       ImGui::CalcTextSize(byValueText).x});
        if (ImGui::BeginTable("##preview", 3,
                              ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_BordersOuter |
                                  ImGuiTableFlags_PadOuterX,
                              ImVec2(0.0f, ListHeight(snap.window.rewards.size(), 10)))) {
            ImGui::TableSetupColumn("##state", ImGuiTableColumnFlags_WidthFixed, stateW);
            ImGui::TableSetupColumn("##name", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("##price", ImGuiTableColumnFlags_WidthFixed, PriceColumnWidth());
            for (size_t i = 0; i < snap.window.rewards.size(); ++i) {
                const RitualReward& r = snap.window.rewards[i];
                const DeferMatch* m = nullptr;
                for (const auto& x : m_matches)
                    if (x.reward == i) { m = &x; break; }
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                if (!m) ImGui::TextDisabled("%s", skipText);
                else ImGui::TextColored(Ui::Colors().Accent, "%s", m->selected ? tickedText : byValueText);
                ImGui::TableNextColumn();
                if (r.stack > 1) ImGui::Text("%s x%d", r.name.c_str(), r.stack);
                else ImGui::TextUnformatted(r.name.c_str());
                ImGui::TableNextColumn();
                const PriceInfo p = UsablePrice(r, PriceFor(IdentityName(r)));
                if (p.found) {
                    const double k = r.stack > 1 ? r.stack : 1;
                    ImGui::TextUnformatted(FormatValue(p.exalt * k, p.divine * k).c_str());
                } else {
                    ImGui::TextDisabled("%s", I18n::Tr("no_price"));
                }
            }
            ImGui::EndTable();
        }
    }

    ImGui::Spacing();
    const std::string dumpLabel = std::string(ICON_FA_FILE_EXPORT "  ") + I18n::Tr("diag_dump") + "###dump";
    if (Ui::SubtleButton(dumpLabel.c_str()))
        m_scanner.RequestDump(DirectoryPath() / L"debug" / L"ritual-dump.txt");
    Ui::Tip(I18n::Tr("diag_dump_tip"));
    const LocText dump = m_scanner.DumpResult();
    if (!dump.empty()) Ui::Hint(ToText(dump).c_str());
    Ui::EndDiagnostics();
}

}  // namespace RitualHelper
