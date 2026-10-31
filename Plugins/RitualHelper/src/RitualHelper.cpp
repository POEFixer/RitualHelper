// Ritual Helper - a POEFixer plugin that highlights the Ritual rewards you want and
// defers them with one click.
//
// Originally created by Omer Faruk ARPA (https://github.com/omrfarukarpa/poefixer-ritual-helper).
// Maintained by POEFixer since October 2026.
//
// This program is free software: you can redistribute it and/or modify it under the
// terms of the GNU General Public License as published by the Free Software
// Foundation, either version 3 of the License, or (at your option) any later version.
// It is distributed WITHOUT ANY WARRANTY; see the LICENSE file for details.
//
// Texts follow the program language (PluginI18n.h): lang/<code>.json next to the DLL, English
// compiled in through lang_en.generated.h (python tools/plugin_i18n/gen.py Plugins/RitualHelper
// after editing lang/en.json). Reward names and the game's button texts stay as the game writes them.
#include "../../../POEFixer/plugin_sdk/PluginSDK.h"
#include "lang_en.generated.h"   // the embedded English: once per DLL, after PluginSDK.h

#include "RitualHelperPlugin.h"
#include "ui/UiText.h"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <iterator>
#include <string>

// Plugin info: the host checks it before CreatePlugin; the publish tool reads it.
POEFIXER_PLUGIN_INFO(
    "RitualHelper",
    "Ritual Helper",
    "1.6.0",
    "Omer Faruk ARPA, POEFixer",
    "Highlights the Ritual rewards you want in the Favours window, shows their prices and defers them all with one click.")

// Fills Dear ImGui's word-wrap character classes; declared in imgui_internal.h,
// which this plugin does not otherwise need.
IMGUI_API void ImTextInitClassifiers();

namespace RitualHelper {

namespace {

int64_t NowMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now().time_since_epoch()).count();
}

// A LocText in English, for the file log (logs stay English whatever the program language):
// the embedded English of lang_en.generated.h, sorted by key.
const char* EnglishOf(const char* key) {
    const auto* first = std::begin(kPluginLangEn);
    const auto* last = std::end(kPluginLangEn);
    const auto* it = std::lower_bound(first, last, key, [](const PluginSDK::I18n::Entry& e, const char* k) {
        return std::strcmp(e.key, k) < 0;
    });
    return it != last && std::strcmp(it->key, key) == 0 ? it->text : key;
}

std::string EnglishText(const LocText& t) {
    return RenderLocText(t, [](const char* key) { return EnglishOf(key); });
}

}  // namespace

RitualHelperPlugin::RitualHelperPlugin() = default;

RitualHelperPlugin::~RitualHelperPlugin() {
    m_scanner.Stop();
}

// Binds the host's ImGui context. This DLL carries its own copy of Dear ImGui,
// whose word-wrap character classes are filled only where a font atlas is built
// (in the host's copy): without the second call text wrapped by this DLL breaks
// in the middle of words. Cheap after the first call.
void RitualHelperPlugin::BindImGui() const {
    if (!ctx()->ImGuiContext) return;
    ImGui::SetCurrentContext(static_cast<ImGuiContext*>(ctx()->ImGuiContext));
    ::ImTextInitClassifiers();
}

void RitualHelperPlugin::OnEnable(bool) {
    if (!HostCompatible()) {
        ctx()->Log.Error("Ritual Helper: this POEFixer version is too old for the plugin - update POEFixer");
        return;
    }
    BindImGui();
    if (!m_loaded) {
        const LoadResult r = m_settings.Load(DirectoryPath());
        m_loaded = true;
        if (r == LoadResult::Unreadable)
            ctx()->Log.Warn("Ritual Helper: config/settings.json could not be read; defaults are used and the file is "
                            "left alone until you change a setting (a copy was kept as settings.json.corrupt if possible).");
        else if (r == LoadResult::Unparsable)
            ctx()->Log.Warn("Ritual Helper: config/settings.json is damaged; defaults are used and the file is left "
                            "alone until you change a setting (a copy was kept as settings.json.corrupt).");
    }
    m_prices.Clear();
    m_matches.clear();
    m_matchSnapSeq = 0;
    RulesChanged();
    m_scanner.SetIntervalMs(m_settings.scanIntervalMs);
    m_scanner.Start(ctx());
    ctx()->Log.Info("Ritual Helper enabled");
}

void RitualHelperPlugin::OnDisable() {
    m_sequence.Cancel(NowMs(), m_input, Loc::Key("status_disabled"));
    m_scanner.Stop();
    SetCapture(false);
    m_matches.clear();
    SaveSettings();
    ctx()->Log.Info("Ritual Helper disabled");
}

void RitualHelperPlugin::SaveSettings() {
    if (!m_loaded) return;
    if (!m_settings.Save(DirectoryPath())) ctx()->Log.Warn("Ritual Helper: could not save config/settings.json");
    m_dirty = false;
}

void RitualHelperPlugin::SaveIfDue(int64_t nowMs) {
    if (!m_dirty || nowMs < m_nextSaveMs) return;
    m_nextSaveMs = nowMs + 1000;  // a slider drag writes at most once a second
    SaveSettings();
}

void RitualHelperPlugin::RulesChanged() {
    ++m_rulesRev;
}

PriceInfo RitualHelperPlugin::PriceFor(const std::string& name) {
    return m_prices.Lookup(ctx(), name);
}

bool RitualHelperPlugin::HasDivineRate() const {
    return ctx()->Prices.GetRates().divineInChaos > 0.0f;
}

std::string RitualHelperPlugin::FormatValue(double ex, double div) const {
    if (m_settings.priceDisplayUnit == 1 && div > 0.0) {
        const std::string amount = div >= 100.0 ? Num("%.0f", div) : (div >= 10.0 ? Num("%.1f", div) : Num("%.2f", div));
        return I18n::TrF("value_div", { { "amount", amount } });
    }
    const std::string amount = ex >= 10.0 ? Num("%.0f", ex) : (ex >= 1.0 ? Num("%.1f", ex) : Num("%.2f", ex));
    return I18n::TrF("value_ex", { { "amount", amount } });
}

void RitualHelperPlugin::EnsureSelectedKeys() {
    if (m_selectedKeysRev == m_rulesRev) return;
    m_selectedKeys.clear();
    for (const auto& s : m_settings.selectedItems) m_selectedKeys.insert(NameKey(s));
    m_selectedKeysRev = m_rulesRev;
}

void RitualHelperPlugin::ToggleItem(const std::string& name, bool on) {
    m_settings.Select(name, on);
    RulesChanged();
    MarkDirty();
}

void RitualHelperPlugin::UpdateMatches(const ScanSnapshot& snap) {
    EnsureSelectedKeys();
    if (snap.seq == m_matchSnapSeq && m_prices.Generation() == m_matchPriceGen && m_rulesRev == m_matchRulesRev)
        return;
    m_matchSnapSeq = snap.seq;
    m_matchPriceGen = m_prices.Generation();
    m_matchRulesRev = m_rulesRev;
    m_matches.clear();
    if (!snap.windowOpen) return;

    DeferRules rules;
    rules.selectedKeys = &m_selectedKeys;
    rules.minValue = m_settings.minValue;
    rules.unit = m_settings.minValueUnit == 1 ? ValueUnit::Divine : ValueUnit::Exalted;
    m_matches = MatchDeferItems(snap.window, rules, [this](const std::string& name) { return PriceFor(name); });
}

void RitualHelperPlugin::TickSequence(int64_t nowMs) {
    if (!m_sequence.IsRunning()) return;
    const auto snap = m_scanner.Latest();
    SequenceView v;
    v.foreground = ctx()->Game.IsForeground();
    v.menuVisible = ctx()->Game.IsMenuVisible();
    if (snap) {
        v.windowOpen = snap->windowOpen;
        v.scanAgeMs = nowMs - snap->takenMs;
        v.buttonPollSeq = snap->buttonPollSeq;
        v.bottom = snap->bottom;
        v.bottomRect = snap->bottomRect;
    }
    m_sequence.Tick(nowMs, v, m_input);
    if (!m_sequence.IsRunning()) {
        m_scanner.SetFastPoll(false);
        m_scanner.RequestScanNow();
        m_notice = m_sequence.Status();
        m_noticeUntil = nowMs + 4000;
        ctx()->Log.Info(("[Ritual Helper] " + EnglishText(m_notice)).c_str());
    }
}

// `snap` is the snapshot the button was drawn from: m_matches index ITS rewards. The
// scanner may have published a newer window since (a reroll, a reward bought), whose
// reward list the same indices would misname, so a newer snapshot is never used here.
void RitualHelperPlugin::StartDefer(const ScanSnapshot& snapRef, int64_t nowMs) {
    if (m_sequence.IsRunning() || nowMs - m_lastStartMs < 400) return;
    if (snapRef.seq != m_matchSnapSeq) return;  // matches belong to another snapshot: the next click starts
    m_lastStartMs = nowMs;
    const ScanSnapshot* snap = &snapRef;
    if (!snap->windowOpen || m_matches.empty()) return;

    // Rows top to bottom, left to right: the cursor sweeps the grid once.
    std::vector<const DeferMatch*> order;
    for (const auto& m : m_matches)
        if (m.reward < snap->window.rewards.size()) order.push_back(&m);
    std::sort(order.begin(), order.end(), [&](const DeferMatch* a, const DeferMatch* b) {
        const auto& ra = snap->window.rewards[a->reward];
        const auto& rb = snap->window.rewards[b->reward];
        return ra.slotY != rb.slotY ? ra.slotY < rb.slotY : ra.slotX < rb.slotX;
    });

    if (m_settings.dryRun) {
        std::string names;
        for (const DeferMatch* m : order) {
            if (!names.empty()) names += ", ";
            names += IdentityName(snap->window.rewards[m->reward]);  // the log stays English
        }
        ctx()->Log.Info(("[Ritual Helper] Dry run - would defer: " + names).c_str());
        m_dryFlashUntil = nowMs + 3000;
        m_notice = order.size() == 1
                       ? LocText(Loc::Key("notice_dry_run_one"))
                       : LocText(Loc::Key("notice_dry_run"), { { "count", std::to_string(order.size()) } });
        m_noticeUntil = nowMs + 4000;
        return;
    }
    if (!snap->bottomFound) {
        m_notice = Loc::Key("notice_buttons_unknown");
        m_noticeUntil = nowMs + 4000;
        return;
    }
    const bool already = snap->bottom == BottomMode::DeferItem;
    if (!already && !snap->toggleFound) {
        // The button's word is a key: the notice is worded when drawn (it may outlive a
        // language switch).
        m_notice = LocText(Loc::Key("notice_no_hourglass"), {}, { { "button", Loc::Key("button_defer") } });
        m_noticeUntil = nowMs + 5000;
        return;
    }

    std::vector<ScreenRect> rects;
    rects.reserve(order.size());
    for (const DeferMatch* m : order) rects.push_back(snap->window.rewards[m->reward].rect);
    m_input.SetGameWindow(ctx()->Game.GetGameWindow());
    m_scanner.SetFastPoll(true);
    m_notice = {};
    m_sequence.Start(nowMs, already, snap->toggleRect, std::move(rects), m_settings.clickPauseMs, m_input);
}

void RitualHelperPlugin::DrawUI() {
    if (!m_loaded) return;
    BindImGui();
    const int64_t now = NowMs();

    // The click sequence runs first, whatever is drawn: it must be able to stop itself.
    TickSequence(now);
    SaveIfDue(now);

    const ImGuiIO& io = ImGui::GetIO();
    const bool active = m_settings.showOverlay && ctx()->Game.IsInGame() && ctx()->Game.IsForeground();
    m_scanner.SetIntervalMs(m_settings.scanIntervalMs);
    m_scanner.SetDisplaySize(io.DisplaySize.x, io.DisplaySize.y);
    m_scanner.SetActive(active || m_sequence.IsRunning());
    if (!active) {
        SetCapture(false);
        return;
    }

    m_prices.Refresh(ctx());
    const auto snap = m_scanner.Latest();
    if (!snap) {
        SetCapture(false);
        return;
    }
    UpdateMatches(*snap);
    if (m_settings.showScanStatus) DrawScanStatusLine(*snap);
    if (!snap->windowOpen) {
        SetCapture(false);
        return;
    }
    DrawOverlay(*snap, now);
}

}  // namespace RitualHelper

extern "C" PLUGIN_API PluginSDK::Plugin* CreatePlugin() { return new RitualHelper::RitualHelperPlugin(); }

extern "C" PLUGIN_API void DestroyPlugin(PluginSDK::Plugin* p) { delete p; }
