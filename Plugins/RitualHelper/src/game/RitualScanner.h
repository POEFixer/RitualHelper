// Ritual Helper - background scanner: reads the open Favours window and finds its
// buttons off the render thread, then publishes an immutable snapshot.
// Copyright (C) Omer Faruk ARPA and POEFixer contributors. Licensed under GPL-3.0 (see LICENSE).
#pragma once

#include "LocText.h"
#include "RitualTypes.h"
#include "../PriceBook.h"

#include "../../../../POEFixer/plugin_sdk/PluginSDK.h"

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace RitualHelper {

// The host publishes the Ritual shop grid under this synthesized inventory id
// (POEFixer game_client/GameClientData.h kRitualShopInventoryId).
inline constexpr int kRitualShopInventoryId = 10001;

struct ScanSnapshot {
    uint64_t seq = 0;
    int64_t  takenMs = 0;            // steady clock, ms

    bool         windowOpen = false;
    RitualWindow window;

    // The button under the grid ("Offer Tribute" / "Defer Item").
    bool       bottomFound = false;
    BottomMode bottom = BottomMode::None;
    std::string bottomText;          // the button's text as the game shows it (diagnostics)
    ScreenRect bottomRect;
    uint64_t   buttonPollSeq = 0;    // increases with every read of the bottom button
    int64_t    buttonPolledMs = 0;

    // The hourglass that switches defer mode on.
    bool       toggleFound = false;
    ScreenRect toggleRect;

    // Diagnostics
    int         lastWalkNodes = 0;
    float       lastWalkMs = 0.f;
    int         walks = 0;
    float       lastInventoryMs = 0.f;
    LocText     note;                // why something is missing (a key: the drawing code translates it)
};

class RitualScanner {
public:
    RitualScanner() = default;
    ~RitualScanner() { Stop(); }
    RitualScanner(const RitualScanner&) = delete;
    RitualScanner& operator=(const RitualScanner&) = delete;

    void Start(const PluginSDK::Context* ctx);
    void Stop();

    // Render-thread inputs.
    void SetActive(bool active) { m_active.store(active, std::memory_order_relaxed); }
    void SetFastPoll(bool fast);
    void SetIntervalMs(int ms) { m_intervalMs.store(ms, std::memory_order_relaxed); }
    void SetDisplaySize(float w, float h) {
        m_displayW.store(w, std::memory_order_relaxed);
        m_displayH.store(h, std::memory_order_relaxed);
    }
    void RequestScanNow();
    // Writes a discovery dump on the scanner thread; the result appears in DumpResult().
    void RequestDump(const std::filesystem::path& file);

    std::shared_ptr<const ScanSnapshot> Latest() const;
    std::shared_ptr<const CatalogPrices> Catalog() const;
    // The last dump's outcome (a key and its {path}); empty before the first request.
    LocText DumpResult() const;

private:
    struct Cached {
        uintptr_t panel = 0, bottom = 0, toggle = 0;
        // The bottom button was taken by its place alone (BottomPick::byPlace): provisional
        // until a text the scanner recognises names it.
        bool bottomByPlace = false;
    };

    void Loop();
    void Iteration();
    friend int RitualScannerGuardedIteration(RitualScanner* self) noexcept;

    bool SleepFor(int ms);  // false when stopping
    bool ReadWindow(RitualWindow& out);
    void PollCached(const RitualWindow& w, ScanSnapshot& s);
    void Locate(const RitualWindow& w, ScanSnapshot& s, bool allowFull);
    void Walk(uintptr_t root, int budget, bool leaves, std::vector<UiCandidate>& out, int& visited);
    void FillRects(std::vector<UiCandidate>& items);
    uintptr_t FindPanel(uintptr_t from, const RitualWindow& w);
    void Publish(ScanSnapshot s);
    void MaybeRebuildCatalog();
    void RefreshDeferText();
    std::string EnglishNameOf(const PluginSDK::InventoryItem& item, const std::string& shown);
    void WriteDumpNow(const std::filesystem::path& file);

    const PluginSDK::Context* m_ctx = nullptr;
    std::thread m_thread;
    std::atomic<bool> m_stop{false};
    std::atomic<bool> m_active{false};
    std::atomic<bool> m_fast{false};
    std::atomic<bool> m_wake{false};
    std::atomic<int>  m_intervalMs{400};
    std::atomic<float> m_displayW{0.f}, m_displayH{0.f};
    std::mutex m_cvMutex;
    std::condition_variable m_cv;

    mutable std::mutex m_pubMutex;
    std::shared_ptr<const ScanSnapshot> m_latest;
    std::shared_ptr<const CatalogPrices> m_catalog;
    std::filesystem::path m_dumpRequest;
    LocText m_dumpResult;

    // Scanner-thread state.
    Cached   m_cached;
    uint64_t m_seq = 0;
    uint64_t m_buttonSeq = 0;
    uint64_t m_areaCounter = 0;
    int64_t  m_lastInventoryAt = 0;
    int64_t  m_lastWalkAt = 0;
    int64_t  m_lastSearchAt = 0;     // the last Locate, whole tree or panel only
    int      m_walkBackoffMs = 1500;
    bool     m_wasOpen = false;
    ScanSnapshot m_work;             // the snapshot being kept up to date
    // The game's "Defer Item" button text in the game language (GameString
    // RitualRewardWindowPrepayButton, desktop 371+; "" = unknown) and the game-language
    // revision it was read at.
    std::string m_deferText;
    uint64_t    m_deferTextRevision = ~0ull;
    // English identity names of the open window's rewards (LookupPriceByItem); forgotten
    // when the window closes. A reward the host could not name is asked again.
    EnglishNameCache m_englishNames;
    int      m_walkCount = 0;
};

}  // namespace RitualHelper
