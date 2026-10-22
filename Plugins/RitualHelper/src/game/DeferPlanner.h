// Ritual Helper - decides which rewards of the open Favours window to defer.
// Pure logic: prices arrive through a callback, so the rules can be tested alone.
// Copyright (C) Omer Faruk ARPA and POEFixer contributors. Licensed under GPL-3.0 (see LICENSE).
#pragma once

#include "RitualTypes.h"
#include "Text.h"

#include <string>
#include <unordered_set>
#include <vector>

namespace RitualHelper {

enum class ValueUnit : int { Exalted = 0, Divine = 1 };

// One price answer for a name (per single item, not per stack).
struct PriceInfo {
    bool   found = false;
    bool   uniqueCategory = false;  // the price belongs to a unique item
    double exalt = 0.0;
    double divine = 0.0;
};

struct DeferRules {
    const std::unordered_set<std::string>* selectedKeys = nullptr;  // NameKey()s
    double    minValue = 0.0;  // 0 = value rule off
    ValueUnit unit = ValueUnit::Exalted;
};

struct DeferMatch {
    size_t reward = 0;          // index into RitualWindow::rewards
    bool   selected = false;    // ticked in the item list
    bool   byValue = false;     // reached the minimum value
    bool   priced = false;
    double totalExalt = 0.0;    // the whole stack
    double totalDivine = 0.0;
};

// Unrevealed rewards cannot be identified; spending tribute on them blindly is the
// player's call, so they are never deferred automatically.
inline bool IsHiddenReward(const RitualReward& r) {
    return r.name.empty() || r.name == "Hidden Item";
}

// A price usable for this reward: a unique-item price only for a unique reward (the
// price service also files base types under their priciest unique).
inline PriceInfo UsablePrice(const RitualReward& r, const PriceInfo& p) {
    if (!p.found) return {};
    if (p.uniqueCategory && !r.unique) return {};
    return p;
}

template <class PriceFn>
std::vector<DeferMatch> MatchDeferItems(const RitualWindow& win, const DeferRules& rules, PriceFn&& priceOf) {
    std::vector<DeferMatch> out;
    for (size_t i = 0; i < win.rewards.size(); ++i) {
        const RitualReward& r = win.rewards[i];
        if (IsHiddenReward(r)) continue;

        DeferMatch m;
        m.reward = i;
        m.selected = rules.selectedKeys && rules.selectedKeys->count(NameKey(r.name)) != 0;

        const PriceInfo p = UsablePrice(r, priceOf(r.name));
        const double stack = r.stack > 1 ? static_cast<double>(r.stack) : 1.0;
        if (p.found) {
            m.priced = true;
            m.totalExalt = p.exalt * stack;
            m.totalDivine = p.divine * stack;
        }
        if (rules.minValue > 0.0 && m.priced) {
            const double v = rules.unit == ValueUnit::Divine ? m.totalDivine : m.totalExalt;
            m.byValue = v > 0.0 && v >= rules.minValue;
        }
        if (m.selected || m.byValue) out.push_back(m);
    }
    return out;
}

}  // namespace RitualHelper
