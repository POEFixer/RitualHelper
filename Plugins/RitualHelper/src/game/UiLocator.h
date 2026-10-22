// Ritual Helper - finds the Favours window's buttons among visible UI elements.
// Pure logic (no host calls) so it can be tested; the scanner feeds it candidates.
// Copyright (C) Omer Faruk ARPA and POEFixer contributors. Licensed under GPL-3.0 (see LICENSE).
#pragma once

#include "RitualTypes.h"
#include "Text.h"

#include <cmath>
#include <optional>
#include <vector>

namespace RitualHelper {

// The button under the reward grid reads "Offer Tribute" normally and "Defer Item"
// while defer mode is on. The game shows these texts in the client's language, so
// only an English client is recognised; the click sequence never starts without it.
inline BottomMode ClassifyBottomText(const std::string& text) {
    if (ContainsCI(text, "defer item")) return BottomMode::DeferItem;
    if (ContainsCI(text, "offer tribute")) return BottomMode::OfferTribute;
    return BottomMode::None;
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
};

// The texted element under the grid closest to its bottom edge.
inline BottomPick PickBottomButton(const std::vector<UiCandidate>& all, const RitualWindow& w) {
    BottomPick best;
    float bestDist = 1e30f;
    const float gridBottom = w.gridY + w.GridH();
    for (const auto& e : all) {
        if (e.text.empty()) continue;
        const BottomMode mode = ClassifyBottomText(e.text);
        if (mode == BottomMode::None || !BottomButtonPlausible(e.rect, w)) continue;
        const float d = std::fabs(e.rect.y - gridBottom);
        if (d < bestDist) {
            bestDist = d;
            best.element = e;
            best.mode = mode;
        }
    }
    return best;
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
