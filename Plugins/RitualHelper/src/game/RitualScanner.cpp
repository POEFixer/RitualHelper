// Ritual Helper - background scanner.
// Copyright (C) Omer Faruk ARPA and POEFixer contributors. Licensed under GPL-3.0 (see LICENSE).
#include "RitualScanner.h"

#include "Text.h"
#include "UiLocator.h"

#include "../../../../POEFixer/core/FileIO.h"

#include <Windows.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <exception>
#include <system_error>
#include <utility>

namespace RitualHelper {

namespace {

int64_t NowMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now().time_since_epoch()).count();
}

constexpr int kInventoryReadDelayMs = 60;   // one host worker cycle after the scan request
constexpr int kIdleInventoryMs = 1000;      // while no Favours window is open
constexpr int kFastInventoryMs = 250;       // while the click sequence runs
constexpr int kFastLoopMs = 70;
constexpr int kMaxRewards = 240;
constexpr int kFullWalkBudget = 15000;
constexpr int kPanelWalkBudget = 4000;
constexpr int kMaxDepth = 40;

std::string PathToUtf8(const std::filesystem::path& p) {
    const std::wstring& w = p.native();
    if (w.empty()) return {};
    const int n = ::WideCharToMultiByte(CP_UTF8, 0, w.c_str(), static_cast<int>(w.size()), nullptr, 0, nullptr, nullptr);
    if (n <= 0) return {};
    std::string out(static_cast<size_t>(n), '\0');
    ::WideCharToMultiByte(CP_UTF8, 0, w.c_str(), static_cast<int>(w.size()), out.data(), n, nullptr, nullptr);
    return out;
}

}  // namespace

// SEH guard around one iteration: a bad game read must never take the overlay down.
// No C++ objects with destructors live in this frame (MSVC rule for __try).
int RitualScannerGuardedIteration(RitualScanner* self) noexcept {
    __try {
        self->Iteration();
        return 1;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return 0;
    }
}

void RitualScanner::Start(const PluginSDK::Context* ctx) {
    Stop();
    m_ctx = ctx;
    m_stop = false;
    m_cached = {};
    m_work = {};
    m_wasOpen = false;
    m_lastInventoryAt = 0;
    m_lastWalkAt = 0;
    {
        std::lock_guard<std::mutex> lk(m_pubMutex);
        m_latest = std::make_shared<const ScanSnapshot>();
        m_dumpRequest.clear();
        m_dumpResult.clear();
    }
    m_thread = std::thread([this] { Loop(); });
}

void RitualScanner::Stop() {
    {
        std::lock_guard<std::mutex> lk(m_cvMutex);
        m_stop = true;
    }
    m_cv.notify_all();
    if (m_thread.joinable()) m_thread.join();
}

void RitualScanner::SetFastPoll(bool fast) {
    const bool was = m_fast.exchange(fast, std::memory_order_relaxed);
    if (fast && !was) RequestScanNow();
}

void RitualScanner::RequestScanNow() {
    {
        std::lock_guard<std::mutex> lk(m_cvMutex);
        m_wake = true;
    }
    m_cv.notify_all();
}

void RitualScanner::RequestDump(const std::filesystem::path& file) {
    {
        std::lock_guard<std::mutex> lk(m_pubMutex);
        m_dumpRequest = file;
        m_dumpResult = "Writing...";
    }
    RequestScanNow();
}

std::shared_ptr<const ScanSnapshot> RitualScanner::Latest() const {
    std::lock_guard<std::mutex> lk(m_pubMutex);
    return m_latest;
}

std::shared_ptr<const CatalogPrices> RitualScanner::Catalog() const {
    std::lock_guard<std::mutex> lk(m_pubMutex);
    return m_catalog;
}

std::string RitualScanner::DumpResult() const {
    std::lock_guard<std::mutex> lk(m_pubMutex);
    return m_dumpResult;
}

bool RitualScanner::SleepFor(int ms) {
    std::unique_lock<std::mutex> lk(m_cvMutex);
    m_cv.wait_for(lk, std::chrono::milliseconds(ms), [this] { return m_stop.load() || m_wake.load(); });
    m_wake = false;
    return !m_stop.load();
}

void RitualScanner::Loop() {
    int failures = 0;
    while (!m_stop.load()) {
        if (RitualScannerGuardedIteration(this) == 0) {
            // Repeated faults: back off instead of spinning on a broken read.
            failures = std::min(failures + 1, 20);
            m_cached = {};
        } else {
            failures = 0;
        }
        int wait = 250;
        if (m_fast.load()) wait = kFastLoopMs;
        else if (m_active.load()) wait = std::clamp(m_intervalMs.load(), 100, 250);
        if (failures > 0) wait = std::max(wait, failures * 250);
        if (!SleepFor(wait)) break;
    }
}

void RitualScanner::Iteration() {
    try {
        if (!m_ctx) return;
        std::filesystem::path dump;
        {
            std::lock_guard<std::mutex> lk(m_pubMutex);
            dump.swap(m_dumpRequest);
        }
        if (!dump.empty()) WriteDumpNow(dump);

        const bool fast = m_fast.load();
        if (!fast) MaybeRebuildCatalog();

        const uint64_t area = m_ctx->Game.GetAreaChangeCounter();
        if (area != m_areaCounter) {
            // UI elements of the previous area are gone: never reuse their addresses.
            m_areaCounter = area;
            m_cached = {};
        }

        const int64_t now = NowMs();
        if (!m_active.load() && !fast) {
            if (m_wasOpen || m_work.windowOpen) {
                m_wasOpen = false;
                ScanSnapshot cleared;
                cleared.walks = m_walkCount;
                m_work = std::move(cleared);
                Publish(m_work);
            }
            return;
        }

        const int interval = std::clamp(m_intervalMs.load(), 100, 5000);
        const int64_t every = fast ? kFastInventoryMs : (m_wasOpen ? interval : std::max(interval, kIdleInventoryMs));
        if (now - m_lastInventoryAt >= every) {
            m_lastInventoryAt = now;
            const int64_t t0 = NowMs();
            m_ctx->Inventory.Scan(kRitualShopInventoryId);
            if (!SleepFor(kInventoryReadDelayMs)) return;
            RitualWindow w;
            const bool open = ReadWindow(w);
            m_work.lastInventoryMs = static_cast<float>(NowMs() - t0 - kInventoryReadDelayMs);
            if (!open) {
                const bool changed = m_wasOpen || m_work.windowOpen;
                m_wasOpen = false;
                m_work.windowOpen = false;
                m_work.window = {};
                m_work.bottomFound = false;
                m_work.bottom = BottomMode::None;
                m_work.toggleFound = false;
                m_work.note = "No Favours window open.";
                if (changed || m_work.seq == 0) Publish(m_work);
                return;
            }
            if (!m_wasOpen) {
                m_walkBackoffMs = 1500;
                m_lastWalkAt = 0;
            }
            m_wasOpen = true;
            m_work.windowOpen = true;
            m_work.window = std::move(w);
        }
        if (!m_wasOpen) return;

        const bool allFound = PollCached(m_work.window, m_work);
        const bool walkDue = NowMs() - m_lastWalkAt >= m_walkBackoffMs;
        // A lost bottom button inside a known panel is looked up again at once (a small
        // walk): the game may swap the button when defer mode toggles. Whole-tree
        // searches stay rate-limited.
        const bool quickPanel = !m_work.bottomFound && m_cached.panel != 0;
        if (!allFound && (walkDue || quickPanel)) {
            Locate(m_work.window, m_work, walkDue);
            if (walkDue) {
                m_lastWalkAt = NowMs();
                // Nothing recognisable (for example a non-English client): look less often.
                m_walkBackoffMs = m_work.bottomFound ? 1500 : std::min(m_walkBackoffMs * 2, 12000);
            }
        }
        if (m_work.bottomFound && m_work.toggleFound) m_work.note.clear();
        Publish(m_work);
    } catch (const std::exception&) {
        m_cached = {};
    } catch (...) {
        m_cached = {};
    }
}

bool RitualScanner::ReadWindow(RitualWindow& out) {
    const PluginSDK::Inventory inv = m_ctx->Inventory.Get(kRitualShopInventoryId);
    if (inv.InventoryId != kRitualShopInventoryId || !inv.Grid.Valid) return false;
    const float cell = inv.Grid.CellSize;
    if (!(cell > 1.0f) || cell > 1000.0f) return false;
    if (inv.TotalBoxesX < 1 || inv.TotalBoxesX > 64 || inv.TotalBoxesY < 1 || inv.TotalBoxesY > 64) return false;

    const float dw = m_displayW.load(), dh = m_displayH.load();
    const float gx = inv.Grid.GridScreenX, gy = inv.Grid.GridScreenY;
    if (dw > 0.f && dh > 0.f && (gx < -4.f || gy < -4.f || gx >= dw || gy >= dh)) return false;

    out.boxesX = inv.TotalBoxesX;
    out.boxesY = inv.TotalBoxesY;
    out.gridX = gx;
    out.gridY = gy;
    out.cellSize = cell;
    out.rewards.clear();
    out.rewards.reserve(std::min<size_t>(inv.Items.size(), kMaxRewards));
    for (const auto& it : inv.Items) {
        if (out.rewards.size() >= static_cast<size_t>(kMaxRewards)) break;
        if (it.SlotX < 0 || it.SlotY < 0 || it.SlotX >= out.boxesX || it.SlotY >= out.boxesY) continue;
        const int iw = std::clamp(it.Width, 1, out.boxesX - it.SlotX);
        const int ih = std::clamp(it.Height, 1, out.boxesY - it.SlotY);
        RitualReward r;
        r.name = !it.UniqueName.empty() ? it.UniqueName : it.BaseTypeName;
        r.unique = it.Rarity == 3;
        r.stack = std::max(1, it.StackCount);
        r.slotX = it.SlotX;
        r.slotY = it.SlotY;
        const bool screenOk = it.ScreenValid && it.ScreenW > 0.f && it.ScreenH > 0.f &&
                              (dw <= 0.f || (it.ScreenX + it.ScreenW * 0.5f >= 0.f && it.ScreenX + it.ScreenW * 0.5f < dw &&
                                             it.ScreenY + it.ScreenH * 0.5f >= 0.f && it.ScreenY + it.ScreenH * 0.5f < dh));
        if (screenOk) r.rect = {it.ScreenX, it.ScreenY, it.ScreenW, it.ScreenH};
        else r.rect = {gx + it.SlotX * cell, gy + it.SlotY * cell, iw * cell, ih * cell};
        out.rewards.push_back(std::move(r));
    }
    return true;
}

namespace {

// Visible only when the element and every ancestor are visible: a closed panel keeps
// its children's own flags set.
bool VisibleChain(const PluginSDK::Context* ctx, uintptr_t addr) {
    for (int i = 0; i < 48 && addr; ++i) {
        const PluginSDK::UiElement e = ctx->Ui.Read(addr);
        if (!e.Valid || !e.IsVisible) return false;
        addr = e.ParentAddress;
    }
    return true;
}

bool RectOf(const PluginSDK::Context* ctx, uintptr_t addr, ScreenRect& r) {
    float x = 0, y = 0, w = 0, h = 0;
    if (!ctx->Ui.ComputeScreenRect(addr, x, y, w, h)) return false;
    r = {x, y, w, h};
    return w > 0.f && h > 0.f;
}

}  // namespace

bool RitualScanner::PollCached(const RitualWindow& w, ScanSnapshot& s) {
    const int64_t now = NowMs();
    bool bottomOk = false;
    if (m_cached.bottom && VisibleChain(m_ctx, m_cached.bottom)) {
        const BottomMode mode = ClassifyBottomText(m_ctx->Ui.GetText(m_cached.bottom));
        ScreenRect r;
        if (mode != BottomMode::None && RectOf(m_ctx, m_cached.bottom, r) && BottomButtonPlausible(r, w)) {
            bottomOk = true;
            s.bottom = mode;
            s.bottomRect = r;
        }
    }
    if (!bottomOk) m_cached.bottom = 0;
    s.bottomFound = bottomOk;
    if (!bottomOk) s.bottom = BottomMode::None;
    s.buttonPollSeq = ++m_buttonSeq;
    s.buttonPolledMs = now;

    bool toggleOk = false;
    if (m_cached.toggle && VisibleChain(m_ctx, m_cached.toggle)) {
        UiCandidate c;
        c.addr = m_cached.toggle;
        c.childCount = m_ctx->Ui.Read(m_cached.toggle).ChildCount;
        if (RectOf(m_ctx, m_cached.toggle, c.rect) && DeferTogglePlausible(c, w)) {
            toggleOk = true;
            s.toggleRect = c.rect;
        }
    }
    if (!toggleOk) m_cached.toggle = 0;
    s.toggleFound = toggleOk;
    return bottomOk && toggleOk;
}

void RitualScanner::Walk(uintptr_t root, int budget, bool leaves, std::vector<UiCandidate>& out, int& visited) {
    if (!root) return;
    std::vector<std::pair<uintptr_t, int>> stack;
    stack.reserve(256);
    stack.push_back({root, 0});
    while (!stack.empty() && visited < budget && !m_stop.load()) {
        const auto [addr, depth] = stack.back();
        stack.pop_back();
        ++visited;
        if (!addr || depth > kMaxDepth || !m_ctx->Ui.IsVisible(addr)) continue;
        const std::vector<uintptr_t> children = m_ctx->Ui.GetChildren(addr);
        std::string text = m_ctx->Ui.GetText(addr);
        const bool texted = !text.empty() && text.size() < 160;
        if (texted || (leaves && children.empty())) {
            UiCandidate c;
            c.addr = addr;
            if (texted) c.text = std::move(text);
            c.childCount = static_cast<int>(children.size());
            out.push_back(std::move(c));
        }
        for (auto it = children.rbegin(); it != children.rend(); ++it)
            if (*it) stack.push_back({*it, depth + 1});
    }
}

void RitualScanner::FillRects(std::vector<UiCandidate>& items) {
    if (items.empty()) return;
    std::vector<uintptr_t> addrs;
    addrs.reserve(items.size());
    for (const auto& c : items) addrs.push_back(c.addr);
    const auto rects = m_ctx->Ui.ComputeScreenRects(addrs);
    for (size_t i = 0; i < items.size(); ++i) {
        if (i < rects.size() && rects[i].ok) items[i].rect = {rects[i].x, rects[i].y, rects[i].w, rects[i].h};
        else RectOf(m_ctx, items[i].addr, items[i].rect);
    }
}

uintptr_t RitualScanner::FindPanel(uintptr_t from, const RitualWindow& w) {
    const float c = w.cellSize;
    uintptr_t a = m_ctx->Ui.Read(from).ParentAddress;
    uintptr_t third = 0;  // fallback: the third ancestor of the button
    for (int i = 0; i < 14 && a; ++i) {
        if (i == 2) third = a;
        ScreenRect r;
        if (RectOf(m_ctx, a, r) && r.x <= w.gridX + 0.25f * c && r.Right() >= w.gridX + w.GridW() - 0.25f * c &&
            r.y <= w.gridY + 0.25f * c && r.Bottom() >= w.gridY + w.GridH() - 0.25f * c)
            return a;
        a = m_ctx->Ui.Read(a).ParentAddress;
    }
    return third;
}

void RitualScanner::Locate(const RitualWindow& w, ScanSnapshot& s, bool allowFull) {
    const int64_t t0 = NowMs();
    int visited = 0;
    std::vector<UiCandidate> cands;

    // 1. The bottom button: inside the remembered panel when there is one, else the
    //    whole UI tree (once per opening of the window, then the panel is remembered).
    const bool panelOk = m_cached.panel && VisibleChain(m_ctx, m_cached.panel);
    if (!panelOk) m_cached.panel = 0;
    auto findBottom = [&](uintptr_t root, int budget) {
        cands.clear();
        Walk(root, budget, false, cands, visited);
        std::vector<UiCandidate> texted;
        for (const auto& c : cands)
            if (!c.text.empty() && ClassifyBottomText(c.text) != BottomMode::None) texted.push_back(c);
        FillRects(texted);
        return PickBottomButton(texted, w);
    };
    BottomPick bottom;
    if (m_cached.panel) bottom = findBottom(m_cached.panel, kPanelWalkBudget);
    if (!bottom.element) {
        // The remembered panel no longer holds the button: forget it, so the quick
        // panel lookup runs once per loss and the next search covers the whole tree.
        m_cached.panel = 0;
        if (allowFull) bottom = findBottom(m_ctx->Ui.GetGameUiRoot(), kFullWalkBudget);
    }

    if (bottom.element) {
        m_cached.bottom = bottom.element->addr;
        s.bottomFound = true;
        s.bottom = bottom.mode;
        s.bottomRect = bottom.element->rect;
        s.buttonPollSeq = ++m_buttonSeq;
        s.buttonPolledMs = NowMs();

        // 2. The hourglass: a textless icon around the panel. Search the panel that holds
        //    the grid, then up to two levels above it, and remember the level it was in.
        if (!s.toggleFound) {
            uintptr_t level = m_cached.panel ? m_cached.panel : FindPanel(bottom.element->addr, w);
            for (int up = 0; up < 3 && level && !s.toggleFound; ++up) {
                std::vector<UiCandidate> leaves;
                int v = 0;
                Walk(level, kPanelWalkBudget, true, leaves, v);
                visited += v;
                leaves.erase(std::remove_if(leaves.begin(), leaves.end(),
                                            [](const UiCandidate& c) { return !c.text.empty() || c.childCount != 0; }),
                             leaves.end());
                FillRects(leaves);
                if (auto t = PickDeferToggle(leaves, w)) {
                    m_cached.toggle = t->addr;
                    m_cached.panel = level;
                    s.toggleFound = true;
                    s.toggleRect = t->rect;
                    break;
                }
                level = m_ctx->Ui.Read(level).ParentAddress;
            }
            if (!m_cached.panel) m_cached.panel = FindPanel(bottom.element->addr, w);
        }
        s.note = s.toggleFound ? std::string() : "The defer hourglass was not found (defer mode must be switched on by hand).";
    } else {
        s.bottomFound = false;
        s.bottom = BottomMode::None;
        s.note = "Favours window found, but its buttons were not recognised (an English game client is needed).";
    }
    s.lastWalkNodes = visited;
    s.lastWalkMs = static_cast<float>(NowMs() - t0);
    s.walks = ++m_walkCount;
}

void RitualScanner::Publish(ScanSnapshot s) {
    s.seq = ++m_seq;
    s.takenMs = NowMs();
    auto p = std::make_shared<const ScanSnapshot>(std::move(s));
    std::lock_guard<std::mutex> lk(m_pubMutex);
    m_latest = std::move(p);
}

void RitualScanner::MaybeRebuildCatalog() {
    const uint64_t fp = PricesFingerprint(m_ctx);
    {
        std::lock_guard<std::mutex> lk(m_pubMutex);
        if (m_catalog && m_catalog->fingerprint == fp) return;
    }
    auto built = CatalogPrices::Build(m_ctx, fp);
    std::lock_guard<std::mutex> lk(m_pubMutex);
    m_catalog = std::move(built);
}

void RitualScanner::WriteDumpNow(const std::filesystem::path& file) {
    std::string s = "Ritual Helper discovery dump\r\n\r\n";
    char line[512];

    s += "=== Favours window (inventory 10001) ===\r\n";
    const PluginSDK::Inventory inv = m_ctx->Inventory.Get(kRitualShopInventoryId);
    std::snprintf(line, sizeof(line), "id=%d boxes=%dx%d gridValid=%d grid=%.0f,%.0f cell=%.1f items=%zu\r\n",
                  inv.InventoryId, inv.TotalBoxesX, inv.TotalBoxesY, inv.Grid.Valid ? 1 : 0, inv.Grid.GridScreenX,
                  inv.Grid.GridScreenY, inv.Grid.CellSize, inv.Items.size());
    s += line;
    int n = 0;
    for (const auto& it : inv.Items) {
        if (++n > 120) { s += "  ...\r\n"; break; }
        std::snprintf(line, sizeof(line), "  slot=%d,%d size=%dx%d stack=%d rarity=%d base='%s' unique='%s' screen=%s\r\n",
                      it.SlotX, it.SlotY, it.Width, it.Height, it.StackCount, it.Rarity, it.BaseTypeName.c_str(),
                      it.UniqueName.c_str(), it.ScreenValid ? "ok" : "none");
        s += line;
    }

    RitualWindow w;
    const bool open = ReadWindow(w);
    std::vector<UiCandidate> all;
    int visited = 0;
    Walk(m_ctx->Ui.GetGameUiRoot(), kFullWalkBudget, true, all, visited);
    std::snprintf(line, sizeof(line), "\r\n=== UI walk: %d nodes visited, %zu kept ===\r\n", visited, all.size());
    s += line;

    static const char* const kWords[] = {"ritual", "tribute", "defer", "reroll", "favour", "favor", "offer"};
    std::vector<UiCandidate> hits;
    for (const auto& c : all) {
        for (const char* k : kWords)
            if (ContainsCI(c.text, k)) { hits.push_back(c); break; }
    }
    FillRects(hits);
    s += "--- elements with ritual words ---\r\n";
    for (size_t i = 0; i < hits.size() && i < 120; ++i) {
        const auto& c = hits[i];
        std::snprintf(line, sizeof(line), "kids=%d rect=%.0f,%.0f %.0fx%.0f text='%s'\r\n", c.childCount, c.rect.x,
                      c.rect.y, c.rect.w, c.rect.h, c.text.c_str());
        s += line;
    }
    if (hits.empty()) s += "(none)\r\n";

    if (open) {
        const float c = w.cellSize;
        const ScreenRect region{w.gridX - 3.f * c, w.gridY - 7.f * c, w.GridW() + 6.f * c, w.GridH() + 14.f * c};
        std::vector<UiCandidate> around;
        for (const auto& e : all)
            if (e.text.empty() || e.childCount == 0) around.push_back(e);
        FillRects(around);
        s += "\r\n--- elements around the reward grid ---\r\n";
        int shown = 0;
        for (const auto& e : around) {
            if (e.rect.w <= 0.f || e.rect.h <= 0.f || !region.Contains(e.rect.CenterX(), e.rect.CenterY())) continue;
            if (++shown > 300) { s += "(more truncated)\r\n"; break; }
            const bool toggleLike = DeferTogglePlausible(e, w);
            std::snprintf(line, sizeof(line), "kids=%d rect=%.0f,%.0f %.0fx%.0f%s text='%s'\r\n", e.childCount, e.rect.x,
                          e.rect.y, e.rect.w, e.rect.h, toggleLike ? " [hourglass?]" : "", e.text.c_str());
            s += line;
        }
        if (shown == 0) s += "(none)\r\n";
    }

    std::string result;
    std::error_code ec;
    std::filesystem::create_directories(file.parent_path(), ec);
    if (FileIO::AtomicWrite(file, s)) result = "Saved to " + PathToUtf8(file);
    else result = "Could not write " + PathToUtf8(file);
    if (m_ctx) m_ctx->Log.Info(("[Ritual Helper] " + result).c_str());
    std::lock_guard<std::mutex> lk(m_pubMutex);
    m_dumpResult = std::move(result);
}

}  // namespace RitualHelper
