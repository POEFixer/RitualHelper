// Ritual Helper - prices from the overlay's price service.
// Copyright (C) Omer Faruk ARPA and POEFixer contributors. Licensed under GPL-3.0 (see LICENSE).
#include "PriceBook.h"

#include "data/ItemCatalog.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace RitualHelper {

static_assert(kItemCatalogCount < 65535, "catalog indices are stored as uint16_t");

PriceInfo QueryPrice(const PluginSDK::Context* ctx, const std::string& name) {
    PriceInfo p;
    if (!ctx || name.empty()) return p;
    const PluginSDK::PriceResult r = ctx->Prices.LookupPrice(name);
    if (!r.found) return p;
    const double ex = std::isfinite(r.exalt) ? r.exalt : 0.0;
    const double dv = std::isfinite(r.divine) ? r.divine : 0.0;
    if (ex <= 0.0 && dv <= 0.0) return p;
    p.found = true;
    p.exalt = ex;
    p.divine = dv;
    p.uniqueCategory = IsUniquePriceCategory(r.category.c_str());
    return p;
}

uint64_t PricesFingerprint(const PluginSDK::Context* ctx) {
    if (!ctx) return 0;
    if (const uint64_t rev = ctx->Prices.GetRevision()) return rev;
    const PluginSDK::PriceStatus s = ctx->Prices.GetStatus();
    if (!s.loaded) return 0;
    uint32_t d = 0, e = 0;
    std::memcpy(&d, &s.divineInChaos, sizeof(d));
    std::memcpy(&e, &s.exaltedInChaos, sizeof(e));
    return (static_cast<uint64_t>(static_cast<uint32_t>(s.totalItems)) << 32) ^ (static_cast<uint64_t>(d) << 7) ^ e ^ 1u;
}

std::shared_ptr<const CatalogPrices> CatalogPrices::Build(const PluginSDK::Context* ctx, uint64_t fingerprint) {
    auto out = std::make_shared<CatalogPrices>();
    out->fingerprint = fingerprint;
    out->loaded = fingerprint != 0;
    out->perEntry.resize(kItemCatalogCount);
    if (out->loaded) {
        for (size_t i = 0; i < kItemCatalogCount; ++i) {
            PriceInfo p = QueryPrice(ctx, kItemCatalog[i].name);
            // A currency-style entry never takes a unique's price through the service's
            // loose name matching.
            if (p.found && p.uniqueCategory && !IsUniqueSection(kItemCatalog[i].category)) p = {};
            out->perEntry[i] = p;
        }
    }
    for (size_t i = 0; i < kItemCatalogCount; ++i) {
        const int c = static_cast<int>(kItemCatalog[i].category);
        if (c >= 0 && c < kCategoryCount) out->sorted[static_cast<size_t>(c)].push_back(static_cast<uint16_t>(i));
    }
    for (auto& list : out->sorted) {
        std::stable_sort(list.begin(), list.end(), [&](uint16_t a, uint16_t b) {
            const double pa = out->perEntry[a].found ? out->perEntry[a].exalt : -1.0;
            const double pb = out->perEntry[b].found ? out->perEntry[b].exalt : -1.0;
            return pa > pb;  // the generated catalog is alphabetical: ties keep that order
        });
    }
    return out;
}

void PriceBook::Refresh(const PluginSDK::Context* ctx) {
    const auto now = std::chrono::steady_clock::now();
    if (now < m_nextCheck) return;
    m_nextCheck = now + std::chrono::milliseconds(500);
    const uint64_t fp = PricesFingerprint(ctx);
    if (fp != m_fingerprint) {
        m_fingerprint = fp;
        Clear();
    }
}

PriceInfo PriceBook::Lookup(const PluginSDK::Context* ctx, const std::string& name) {
    auto it = m_memo.find(name);
    if (it != m_memo.end()) return it->second;
    PriceInfo p = m_fingerprint != 0 ? QueryPrice(ctx, name) : PriceInfo{};
    if (m_memo.size() > 4096) m_memo.clear();
    m_memo.emplace(name, p);
    return p;
}

void PriceBook::Clear() {
    m_memo.clear();
    ++m_generation;
}

}  // namespace RitualHelper
