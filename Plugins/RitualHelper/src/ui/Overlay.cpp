// Ritual Helper - in-game drawing: highlights, value labels, the DEFER button and its
// feedback. Everything is sized from the font, snapped to whole pixels and drawn on
// the background list so the overlay's own windows stay on top. Status pills use the
// kit's in-game panel and outlined text (PluginUi.h).
// Copyright (C) Omer Faruk ARPA and POEFixer contributors. Licensed under GPL-3.0 (see LICENSE).
#include "../RitualHelperPlugin.h"
#include "../../../../POEFixer/plugin_sdk/PluginUi.h"

#include "../../../../POEFixer/imgui/IconsFontAwesome6.h"

#include <Windows.h>

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace RitualHelper {

namespace {

constexpr ImU32 kPanelBg = IM_COL32(14, 12, 10, 215);
constexpr float kPillOpacity = 0.84f;
constexpr ImU32 kTextMain = IM_COL32(236, 224, 196, 255);
constexpr ImU32 kTextGood = IM_COL32(140, 230, 140, 255);
constexpr ImU32 kTextWarn = IM_COL32(255, 196, 96, 255);
constexpr ImU32 kTextBad = IM_COL32(255, 128, 110, 255);

float Snap(float v) { return PluginUi::SnapPx(v); }

}  // namespace

void RitualHelperPlugin::SetCapture(bool on) {
    if (!on) m_pressActive = false;
    if (on == m_captureOn) return;
    m_captureOn = on;
    ctx()->Overlay.SetWantsOverlayInput(on);
}

// A physical left click released over the button, seen even while the overlay does
// not take mouse input yet (the click then also reaches the game, as before).
bool RitualHelperPlugin::HardwareClickOn(const ImVec2& p0, const ImVec2& p1) {
    const bool down = (::GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
    POINT pt{};
    bool over = false;
    if (::GetCursorPos(&pt)) {
        HWND hwnd = ctx()->Game.GetGameWindow();
        if (!hwnd || ::ScreenToClient(hwnd, &pt)) {
            const float x = static_cast<float>(pt.x), y = static_cast<float>(pt.y);
            over = x >= p0.x && x < p1.x && y >= p0.y && y < p1.y;
        }
    }
    if (down && !m_hwWasDown) m_hwPressedOn = over;
    const bool released = !down && m_hwWasDown;
    const bool clicked = released && m_hwPressedOn && over;
    if (released) m_hwPressedOn = false;
    m_hwWasDown = down;
    return clicked;
}

void RitualHelperPlugin::DrawPill(const ImVec2& anchor, const char* text, ImU32 textColor, bool alignRight) {
    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    const ImVec2 ts = ImGui::CalcTextSize(text);
    const ImVec2 pad = PluginUi::PanelPadding();
    const ImVec2 size(ts.x + pad.x * 2.f, ts.y + pad.y * 2.f);
    ImVec2 p0(alignRight ? anchor.x - size.x : anchor.x, anchor.y);
    const ImVec2 disp = ImGui::GetIO().DisplaySize;
    p0.x = std::clamp(p0.x, 2.f, std::max(2.f, disp.x - size.x - 2.f));
    p0.y = std::clamp(p0.y, 2.f, std::max(2.f, disp.y - size.y - 2.f));
    p0 = ImVec2(Snap(p0.x), Snap(p0.y));
    const ImVec2 p1(p0.x + Snap(size.x), p0.y + Snap(size.y));
    PluginUi::Panel(dl, p0, p1, kPillOpacity);
    PluginUi::TextOutlined(dl, ImVec2(p0.x + pad.x, p0.y + pad.y), textColor, text);
}

void RitualHelperPlugin::DrawOverlay(const ScanSnapshot& snap, int64_t nowMs) {
    DrawHighlights(snap, nowMs);
    DrawDeferButton(snap, nowMs);
}

void RitualHelperPlugin::DrawHighlights(const ScanSnapshot& snap, int64_t nowMs) {
    if (m_matches.empty()) return;
    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    const float fs = ImGui::GetFontSize();
    const bool flash = nowMs < m_dryFlashUntil;
    const ImU32 col = ImGui::ColorConvertFloat4ToU32(
        ImVec4(m_settings.highlight[0], m_settings.highlight[1], m_settings.highlight[2], m_settings.highlight[3]));
    const float thick = flash ? 3.0f : 2.0f;
    constexpr float kGap = 2.0f;  // between the reward's cell and the frame
    char val[32];
    for (const auto& m : m_matches) {
        if (m.reward >= snap.window.rewards.size()) continue;
        const ScreenRect& r = snap.window.rewards[m.reward].rect;
        // Outer edge of the frame; ImGui 1.93 strokes closed shapes inside it.
        const ImVec2 o0(Snap(r.x) + kGap, Snap(r.y) + kGap);
        const ImVec2 o1(Snap(r.Right()) - kGap, Snap(r.Bottom()) - kGap);
        if (o1.x - o0.x <= thick * 2.0f || o1.y - o0.y <= thick * 2.0f) continue;
        // A 1 px dark rim just outside the frame keeps it visible on bright art.
        dl->AddRect(ImVec2(o0.x - 1, o0.y - 1), ImVec2(o1.x + 1, o1.y + 1), IM_COL32(0, 0, 0, 160), 3.0f, 1.0f);
        dl->AddRect(o0, o1, col, 3.0f, thick);
        const ImVec2 a(o0.x + thick, o0.y + thick);  // inner edge of the frame
        if (!m_settings.showValues || !m.priced) continue;
        FormatValue(val, sizeof(val), m.totalExalt, m.totalDivine);
        const ImVec2 ts = ImGui::CalcTextSize(val);
        const float pad = Snap(fs * 0.2f);
        const ImVec2 p0(Snap(a.x), Snap(a.y));
        const ImVec2 p1(p0.x + Snap(ts.x) + pad * 2.f, p0.y + Snap(ts.y) + 2.f);
        dl->AddRectFilled(p0, p1, kPanelBg, fs * 0.2f);
        PluginUi::TextOutlined(dl, ImVec2(p0.x + pad, p0.y + 1.f), col | IM_COL32_A_MASK, val);
    }
}

void RitualHelperPlugin::DrawDeferButton(const ScanSnapshot& snap, int64_t nowMs) {
    const ImGuiIO& io = ImGui::GetIO();
    const float fs = ImGui::GetFontSize();

    // Where the button lives: left of the hourglass, else above the grid's right edge.
    char label[48];
    std::snprintf(label, sizeof(label), "%s %s (%zu)", ICON_FA_HOURGLASS, m_settings.dryRun ? "DRY RUN" : "DEFER",
                  m_matches.size());
    const ImVec2 ts = ImGui::CalcTextSize(label);
    const ImVec2 size(Snap(ts.x + fs * 1.4f), Snap(ts.y + fs * 0.7f));
    const float gap = Snap(fs * 0.6f);
    ImVec2 pos;
    if (snap.toggleFound) {
        pos = ImVec2(snap.toggleRect.x - gap - size.x, snap.toggleRect.CenterY() - size.y * 0.5f);
    } else {
        pos = ImVec2(snap.window.gridX + snap.window.GridW() - size.x, snap.window.gridY - size.y - gap);
    }
    pos.x = Snap(std::clamp(pos.x, 2.f, std::max(2.f, io.DisplaySize.x - size.x - 2.f)));
    pos.y = Snap(std::clamp(pos.y, 2.f, std::max(2.f, io.DisplaySize.y - size.y - 2.f)));
    const ImVec2 p1(pos.x + size.x, pos.y + size.y);
    const ImVec2 under(pos.x + size.x, p1.y + Snap(fs * 0.35f));

    // A running sequence: progress instead of the button.
    if (m_sequence.IsRunning()) {
        SetCapture(false);
        char buf[96];
        std::snprintf(buf, sizeof(buf), "Deferring %zu/%zu  -  right-click or Esc stops", m_sequence.Done(), m_sequence.Total());
        DrawPill(ImVec2(p1.x, pos.y), buf, kTextWarn, true);
        return;
    }

    const bool showNotice = !m_notice.empty() && nowMs < m_noticeUntil;
    if (showNotice) {
        const SequenceOutcome o = m_sequence.Outcome();
        ImU32 c = kTextMain;
        if (m_notice.rfind("Dry run", 0) == 0) c = kTextWarn;
        else if (o == SequenceOutcome::Done) c = kTextGood;
        else if (o == SequenceOutcome::Aborted || o == SequenceOutcome::Cancelled) c = kTextBad;
        DrawPill(m_matches.empty() ? ImVec2(p1.x, pos.y) : under, m_notice.c_str(), c, true);
    }

    if (m_matches.empty()) {
        SetCapture(false);
        return;
    }
    // Defer mode can only be driven when the game's buttons were recognised.
    if (!snap.bottomFound || (!snap.toggleFound && snap.bottom != BottomMode::DeferItem)) {
        SetCapture(false);
        if (!showNotice && !snap.note.empty()) DrawPill(ImVec2(p1.x, pos.y), snap.note.c_str(), kTextWarn, true);
        return;
    }
    // The overlay menu covers the game: the sequence would stop at once.
    if (ctx()->Game.IsMenuVisible()) {
        SetCapture(false);
        if (!showNotice) DrawPill(ImVec2(p1.x, pos.y), "Close the overlay menu to defer", kTextMain, true);
        return;
    }

    const bool hovered = io.MousePos.x >= pos.x && io.MousePos.x < p1.x && io.MousePos.y >= pos.y && io.MousePos.y < p1.y;
    SetCapture(hovered || m_pressActive);

    ImGui::SetNextWindowPos(pos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(size, ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowMinSize, ImVec2(1, 1));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, fs * 0.3f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.11f, 0.09f, 0.06f, 0.92f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.20f, 0.16f, 0.09f, 0.96f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.08f, 0.06f, 0.04f, 1.f));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.62f, 0.50f, 0.26f, 1.f));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.93f, 0.84f, 0.58f, 1.f));
    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground |
                                   ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
                                   ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove;
    bool clicked = false;
    if (ImGui::Begin("##ritualhelper_defer", nullptr, flags)) {
        char btn[64];
        std::snprintf(btn, sizeof(btn), "%s###defer", label);
        clicked = ImGui::Button(btn, size);
        m_pressActive = ImGui::IsItemActive();
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) {
            ImGui::BeginTooltip();
            ImGui::PushTextWrapPos(fs * 26.0f);
            if (m_settings.dryRun) {
                ImGui::TextUnformatted("Dry run is on (Diagnostics): pressing this only lists the highlighted rewards in "
                                       "the log. No clicks are sent.");
            } else {
                ImGui::Text("Defers the %zu highlighted reward%s: switches defer mode on, clicks each one and "
                            "confirms.", m_matches.size(), m_matches.size() == 1 ? "" : "s");
                ImGui::Spacing();
                ImGui::TextUnformatted("Right-click, Esc or moving the mouse stops it.");
            }
            ImGui::PopTextWrapPos();
            ImGui::EndTooltip();
        }
    }
    ImGui::End();
    ImGui::PopStyleColor(6);
    ImGui::PopStyleVar(5);

    const bool hw = HardwareClickOn(pos, p1);
    if (clicked || hw) {
        SetCapture(false);
        StartDefer(snap, nowMs);
    }
}

void RitualHelperPlugin::DrawScanStatusLine(const ScanSnapshot& snap) {
    char buf[256];
    const char* bottom = snap.bottom == BottomMode::DeferItem ? "Defer Item"
                       : snap.bottom == BottomMode::OfferTribute ? "Offer Tribute" : "not found";
    if (snap.windowOpen) {
        std::snprintf(buf, sizeof(buf), "Ritual Helper: %zu rewards, %zu highlighted | button: %s | hourglass: %s",
                      snap.window.rewards.size(), m_matches.size(), bottom, snap.toggleFound ? "found" : "not found");
    } else {
        std::snprintf(buf, sizeof(buf), "Ritual Helper: no Favours window open");
    }
    const float fs = ImGui::GetFontSize();
    DrawPill(ImVec2(fs, fs * 4.0f), buf, IM_COL32(150, 220, 255, 255), false);
}

}  // namespace RitualHelper
