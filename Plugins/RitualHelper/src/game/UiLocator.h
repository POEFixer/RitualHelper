// Ritual Helper - finds the Favours window's buttons among visible UI elements.
// Pure logic (no host calls) so it can be tested; the scanner feeds it candidates.
// Copyright (C) Omer Faruk ARPA and POEFixer contributors. Licensed under GPL-3.0 (see LICENSE).
#pragma once

#include "RitualTypes.h"
#include "Text.h"

#include <cmath>
#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

namespace RitualHelper {

// The button under the reward grid reads "Offer Tribute" normally and "Defer Item" while
// defer mode is on, in the client's language. "Defer Item" is recognised by its text: the
// game's own RitualRewardWindowPrepayButton text in the game language (`deferText`, from the
// host's GameString on desktop 371+) or the English words. "Offer Tribute" is in no balance
// table (F13): the English words name it here, and on a client whose defer text is known
// PickBottomButton / ClassifyPlacedBottomText take another text on the button's place for
// it. Such a pick by place is provisional (BottomPick::byPlace, PlanButtonSearch): the
// scanner keeps looking until a text it recognises names the button. Without either the
// click sequence never starts (rewards are only ever clicked while the button reads Defer
// Item).
inline BottomMode ClassifyBottomText(const std::string& text, const std::string& deferText = {}) {
    if (!deferText.empty() && ContainsCI(text, deferText)) return BottomMode::DeferItem;
    if (ContainsCI(text, "defer item")) return BottomMode::DeferItem;
    if (ContainsCI(text, "offer tribute")) return BottomMode::OfferTribute;
    return BottomMode::None;
}

// Whether a text can be a button's label: it holds a letter (any non-ASCII character counts
// as one), not only digits, spaces and punctuation (a count "12" or "3/10" never is).
inline bool HasLetter(std::string_view text) {
    for (const char ch : text) {
        const unsigned char c = static_cast<unsigned char>(ch);
        if (c >= 0x80 || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')) return true;
    }
    return false;
}

// A text already at the button's place (BottomButtonPlausible): recognised as above, else,
// when the defer text is known, Offer Tribute (another text there that can be a label).
inline BottomMode ClassifyPlacedBottomText(const std::string& text, const std::string& deferText) {
    const BottomMode mode = ClassifyBottomText(text, deferText);
    if (mode != BottomMode::None || deferText.empty()) return mode;
    return HasLetter(text) ? BottomMode::OfferTribute : BottomMode::None;
}

// Shaped like the button: at least one and a half cells wide and wider than tall. A text
// picked by place prefers this shape (a short label just under the grid is not the button);
// a short translated label on the button itself still counts when nothing else is there.
inline bool ButtonShaped(const ScreenRect& r, const RitualWindow& w) {
    return r.w >= 1.5f * w.cellSize && r.w >= r.h;
}

// Geometry is judged relative to the grid's cell size so it holds at every resolution
// and UI scale; the original fixed pixel windows (tuned at one resolution) are kept as
// an alternative so nothing that matched before stops matching.
inline bool BottomButtonPlausible(const ScreenRect& r, const RitualWindow& w) {
    const float c = w.cellSize;
    const float gridBottom = w.gridY + w.GridH();
    const float cx = r.CenterX();
    if (cx < w.gridX || cx > w.gridX + w.GridW()) return false;
    const bool relative = r.y >= gridBottom - 0.5f * c && r.y <= gridBottom + 6.0f * c;
    const bool legacy = r.y >= gridBottom - 20.f && r.y <= gridBottom + 220.f;
    return relative || legacy;
}

inline bool DeferTogglePlausible(const UiCandidate& e, const RitualWindow& w) {
    if (!e.text.empty() || e.childCount != 0) return false;
    const ScreenRect& r = e.rect;
    if (r.w <= 0.f || r.h <= 0.f) return false;
    const float c = w.cellSize;
    const float midX = w.gridX + w.GridW() * 0.5f;
    const float right = w.gridX + w.GridW();
    const float cx = r.CenterX(), cy = r.CenterY();
    const float squareTol = std::fmax(6.f, 0.12f * r.w);
    if (std::fabs(r.w - r.h) > squareTol) return false;

    const bool sizeRel = r.w >= 1.5f * c && r.w <= 4.0f * c;
    const bool sizeLegacy = r.w >= 80.f && r.w <= 130.f;
    if (!sizeRel && !sizeLegacy) return false;

    if (cx <= midX) return false;
    const bool xRel = cx <= right + 1.5f * c;
    const bool xLegacy = cx <= right + 60.f;
    if (!xRel && !xLegacy) return false;
    const bool yRel = cy >= w.gridY - 5.5f * c && cy <= w.gridY - 0.5f * c;
    const bool yLegacy = cy >= w.gridY - 220.f && cy <= w.gridY - 20.f;
    return yRel || yLegacy;
}

struct BottomPick {
    std::optional<UiCandidate> element;
    BottomMode mode = BottomMode::None;
    // Taken for the button by its place alone (no text recognised it): provisional.
    bool byPlace = false;
};

// The texted element under the grid closest to its bottom edge: a recognised text first;
// only when none is recognised and the defer text is known, another text there that can be
// a label (Offer Tribute in the client's language, F13), a button-shaped one before a
// closer one that is not. That last pick is by place only (byPlace).
inline BottomPick PickBottomButton(const std::vector<UiCandidate>& all, const RitualWindow& w,
                                   const std::string& deferText = {}) {
    const float gridBottom = w.gridY + w.GridH();
    for (int pass = 0; pass < 2; ++pass) {
        if (pass == 1 && deferText.empty()) break;
        BottomPick best;
        float bestDist = 1e30f;
        bool bestShaped = false;
        for (const auto& e : all) {
            if (e.text.empty()) continue;
            const BottomMode mode =
                pass == 0 ? ClassifyBottomText(e.text, deferText) : ClassifyPlacedBottomText(e.text, deferText);
            if (mode == BottomMode::None || !BottomButtonPlausible(e.rect, w)) continue;
            const float d = std::fabs(e.rect.y - gridBottom);
            const bool shaped = pass == 1 && ButtonShaped(e.rect, w);
            const bool better = !best.element || (shaped != bestShaped ? shaped : d < bestDist);
            if (better) {
                bestDist = d;
                bestShaped = shaped;
                best.element = e;
                best.mode = mode;
                best.byPlace = pass == 1;
            }
        }
        if (best.element) return best;
    }
    return {};
}

// A button taken by its place alone is checked again while the click sequence runs (fast
// polling): the remembered panel is searched every kPlacedRecheckMs, so the real button wins
// as soon as it shows the defer text (a wrong pick never reads it, and the sequence would
// wait for it in vain).
inline constexpr int64_t kPlacedRecheckMs = 250;

struct ButtonSearch {
    bool search = false;     // look for the buttons in this iteration
    bool allowFull = false;  // the whole UI tree may be walked (else the remembered panel only)
};

// Whether the scanner looks for the window's buttons in this iteration. Never while both are
// known for good; a button known only by its place does not count as found (byPlace). A
// search is made when the regular one is due (`walkDue`, whole tree allowed), and in the
// remembered panel at once after the button was lost, or every kPlacedRecheckMs during the
// click sequence while the button is known by its place only.
inline ButtonSearch PlanButtonSearch(bool bottomFound, bool bottomByPlace, bool toggleFound, bool panelKnown,
                                     bool walkDue, bool fastPoll, int64_t sinceLastSearchMs) {
    const bool allFound = bottomFound && !bottomByPlace && toggleFound;
    if (allFound) return {};
    const bool recheck = bottomFound && bottomByPlace && fastPoll && sinceLastSearchMs >= kPlacedRecheckMs;
    const bool quickPanel = panelKnown && (!bottomFound || recheck);
    ButtonSearch plan;
    plan.search = walkDue || quickPanel;
    plan.allowFull = walkDue;
    return plan;
}

// The hourglass (defer toggle): a square icon above the grid's right half; the one
// nearest the grid's top-right corner wins.
inline std::optional<UiCandidate> PickDeferToggle(const std::vector<UiCandidate>& all, const RitualWindow& w) {
    std::optional<UiCandidate> best;
    float bestDist = 1e30f;
    const float cornerX = w.gridX + w.GridW(), cornerY = w.gridY;
    for (const auto& e : all) {
        if (!DeferTogglePlausible(e, w)) continue;
        const float dx = e.rect.CenterX() - cornerX, dy = e.rect.CenterY() - cornerY;
        const float d = dx * dx + dy * dy;
        if (d < bestDist) {
            bestDist = d;
            best = e;
        }
    }
    return best;
}

}  // namespace RitualHelper
