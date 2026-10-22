// Ritual Helper - prices from the overlay's price service (ctx->Prices), memoised.
// A name the service does not know costs it a scan of its whole database, so every
// answer is kept until the service publishes new prices (its revision changes).
// Copyright (C) Omer Faruk ARPA and POEFixer contributors. Licensed under GPL-3.0 (see LICENSE).
#pragma once

#include "game/Category.h"
#include "game/DeferPlanner.h"

#include "../../../POEFixer/plugin_sdk/PluginSDK.h"

#include <array>
#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace RitualHelper {

// One host answer turned into the planner's shape.
PriceInfo QueryPrice(const PluginSDK::Context* ctx, const std::string& name);

// The price service's identity of its current data: the revision when the host has
// one, else a fingerprint of its status (older hosts).
uint64_t PricesFingerprint(const PluginSDK::Context* ctx);

// Prices of every catalog entry, with each category's entries sorted by value.
// Built on the scanner thread whenever the price data changes; immutable afterwards.
struct CatalogPrices {
    uint64_t fingerprint = 0;
    bool     loaded = false;
    std::vector<PriceInfo> perEntry;                                  // index = catalog index
    std::array<std::vector<uint16_t>, kCategoryCount> sorted;         // catalog indices, priciest first

    static std::shared_ptr<const CatalogPrices> Build(const PluginSDK::Context* ctx, uint64_t fingerprint);
};

// Render-thread memo for the names the Favours window shows.
class PriceBook {
public:
    // Call once per frame; cheap. Drops the memo when the price data changed.
    void Refresh(const PluginSDK::Context* ctx);

    PriceInfo Lookup(const PluginSDK::Context* ctx, const std::string& name);

    // Bumped on every flush, so derived results (matches) know to recompute.
    uint64_t Generation() const { return m_generation; }
    void Clear();

private:
    std::unordered_map<std::string, PriceInfo> m_memo;
    uint64_t m_fingerprint = 0;
    uint64_t m_generation = 1;
    std::chrono::steady_clock::time_point m_nextCheck{};
};

}  // namespace RitualHelper
