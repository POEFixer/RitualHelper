// Ritual Helper - the non-blocking click sequence that defers the matched rewards.
// Ticked once per frame on the render thread; input goes through an InputPort so the
// state machine can be tested without sending real clicks.
// Copyright (C) Omer Faruk ARPA and POEFixer contributors. Licensed under GPL-3.0 (see LICENSE).
#pragma once

#include "RitualTypes.h"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

namespace RitualHelper {

// Everything the sequence does to the real mouse/keyboard.
class InputPort {
public:
    virtual ~InputPort() = default;
    // Game client point -> absolute screen point. False when the game window is gone.
    virtual bool ClientToScreen(float cx, float cy, int& sx, int& sy) = 0;
    virtual bool GetCursor(int& sx, int& sy) = 0;
    virtual void MoveTo(int sx, int sy) = 0;
    virtual void ClickAt(int sx, int sy) = 0;   // left button down + up at an absolute point
    virtual void RestoreCursor(int sx, int sy) = 0;
    virtual bool RightButtonDown() = 0;
    virtual bool EscapeDown() = 0;
};

// What the rest of the plugin knows this frame.
struct SequenceView {
    bool       foreground = false;
    bool       menuVisible = false;   // the overlay's own menu is open
    bool       windowOpen = false;    // the latest scan still sees the Favours window
    int64_t    scanAgeMs = 0;         // age of that scan
    uint64_t   buttonPollSeq = 0;     // increases with every fresh read of the bottom button
    BottomMode bottom = BottomMode::None;
    ScreenRect bottomRect;
};

enum class SequenceOutcome { None, Done, Cancelled, Aborted };

class DeferSequence {
public:
    static constexpr int64_t kVerifyTimeoutMs = 2500;
    static constexpr int64_t kMaxScanAgeMs = 700;
    static constexpr int     kCursorSlackPx = 4;
    static constexpr int64_t kButtonGraceMs = 600;

    bool IsRunning() const { return m_phase != Phase::Idle; }
    const std::string& Status() const { return m_status; }
    SequenceOutcome Outcome() const { return m_outcome; }
    int64_t FinishedAtMs() const { return m_finishedAt; }
    size_t Done() const { return m_index; }
    size_t Total() const { return m_items.size(); }

    // clickPauseMs: time per click (cursor settle + wait after the click).
    void Start(int64_t nowMs, bool alreadyDeferMode, const ScreenRect& toggle,
               std::vector<ScreenRect> items, int clickPauseMs, InputPort& in) {
        if (IsRunning() || items.empty()) return;
        m_toggle = toggle;
        m_items = std::move(items);
        m_index = 0;
        m_sub = Sub::Move;
        const int pause = std::clamp(clickPauseMs, 100, 1000);
        m_settleMs = std::max(40, pause * 35 / 100);
        m_postMs = std::max(60, pause - m_settleMs);
        m_haveCursor = in.GetCursor(m_savedX, m_savedY);
        const int64_t perClick = m_settleMs + m_postMs + 150;
        m_watchdogAt = nowMs + std::min<int64_t>(60000, 5000 + perClick * static_cast<int64_t>(m_items.size() + 2));
        m_deferSeenMs = nowMs;
        m_outcome = SequenceOutcome::None;
        m_status = "Deferring...";
        m_phase = alreadyDeferMode ? Phase::Items : Phase::Toggle;
    }

    // Stops a running sequence (plugin disabled, area change).
    void Cancel(int64_t nowMs, InputPort& in, const char* why) {
        if (IsRunning()) Finish(in, SequenceOutcome::Cancelled, why, nowMs);
    }

    void Tick(int64_t nowMs, const SequenceView& v, InputPort& in) {
        if (m_phase == Phase::Idle) return;

        if (in.RightButtonDown() || in.EscapeDown()) { Finish(in, SequenceOutcome::Cancelled, "Cancelled", nowMs); return; }
        if (!v.foreground) { Finish(in, SequenceOutcome::Aborted, "Stopped: the game lost focus", nowMs); return; }
        if (v.menuVisible) { Finish(in, SequenceOutcome::Aborted, "Stopped: the overlay menu opened", nowMs); return; }
        if (!v.windowOpen || v.scanAgeMs > kMaxScanAgeMs) {
            Finish(in, SequenceOutcome::Aborted, "Stopped: the Favours window closed", nowMs);
            return;
        }
        if (nowMs > m_watchdogAt) { Finish(in, SequenceOutcome::Aborted, "Stopped: took too long", nowMs); return; }

        switch (m_phase) {
            case Phase::Toggle:
                // The toggle is only clicked while the game's buttons are recognised.
                if (!ButtonKnown(nowMs, v, false, in)) return;
                if (StepClick(nowMs, m_toggle, in)) {
                    m_phase = Phase::Verify;
                    m_verifyAfterSeq = v.buttonPollSeq;
                    m_verifyUntil = nowMs + kVerifyTimeoutMs;
                }
                break;
            case Phase::Verify:
                // Only a button read taken after the toggle click counts.
                if (v.buttonPollSeq > m_verifyAfterSeq && v.bottom == BottomMode::DeferItem) {
                    m_phase = Phase::Items;
                    m_sub = Sub::Move;
                    m_deferSeenMs = nowMs;
                } else if (nowMs > m_verifyUntil) {
                    Finish(in, SequenceOutcome::Aborted, "Stopped: defer mode did not turn on", nowMs);
                }
                break;
            case Phase::Items:
                // Never touch a reward unless the game is in defer mode: a click outside
                // it would pick the reward for purchase instead.
                if (!ButtonKnown(nowMs, v, true, in)) return;
                if (m_index >= m_items.size()) {
                    m_phase = Phase::Apply;
                    m_sub = Sub::Move;
                    break;
                }
                if (StepClick(nowMs, m_items[m_index], in)) ++m_index;
                break;
            case Phase::Apply: {
                if (!ButtonKnown(nowMs, v, true, in)) return;
                if (StepClick(nowMs, v.bottomRect, in)) {
                    char buf[64];
                    std::snprintf(buf, sizeof(buf), "Deferred %zu item%s", m_items.size(), m_items.size() == 1 ? "" : "s");
                    Finish(in, SequenceOutcome::Done, buf, nowMs);
                }
                break;
            }
            default:
                break;
        }
    }

private:
    enum class Phase { Idle, Toggle, Verify, Items, Apply };

    // True when the step may proceed. A button read that briefly finds nothing (the
    // scanner is re-locating it) only pauses the step; a definite wrong state, or no
    // read for kButtonGraceMs, stops the sequence.
    bool ButtonKnown(int64_t nowMs, const SequenceView& v, bool needDeferMode, InputPort& in) {
        const bool known = needDeferMode ? v.bottom == BottomMode::DeferItem : v.bottom != BottomMode::None;
        if (known) {
            m_deferSeenMs = nowMs;
            return true;
        }
        if (needDeferMode && v.bottom == BottomMode::OfferTribute) {
            Finish(in, SequenceOutcome::Aborted, "Stopped: defer mode turned off", nowMs);
            return false;
        }
        if (nowMs - m_deferSeenMs > kButtonGraceMs) {
            Finish(in, SequenceOutcome::Aborted, "Stopped: the ritual buttons are not recognised", nowMs);
        }
        return false;
    }
    enum class Sub { Move, Settle, Post };

    // Move, let the cursor settle, click, wait. True once the wait after the click is over.
    bool StepClick(int64_t nowMs, const ScreenRect& target, InputPort& in) {
        switch (m_sub) {
            case Sub::Move: {
                if (!in.ClientToScreen(target.CenterX(), target.CenterY(), m_tx, m_ty)) {
                    Finish(in, SequenceOutcome::Aborted, "Stopped: the game window is gone", nowMs);
                    return false;
                }
                in.MoveTo(m_tx, m_ty);
                m_deadline = nowMs + m_settleMs;
                m_sub = Sub::Settle;
                return false;
            }
            case Sub::Settle: {
                if (nowMs < m_deadline) return false;
                int cx = 0, cy = 0;
                if (!in.GetCursor(cx, cy) || cx < m_tx - kCursorSlackPx || cx > m_tx + kCursorSlackPx ||
                    cy < m_ty - kCursorSlackPx || cy > m_ty + kCursorSlackPx) {
                    // The player moved the mouse: never click somewhere unplanned.
                    Finish(in, SequenceOutcome::Cancelled, "Cancelled: the mouse moved", nowMs);
                    return false;
                }
                in.ClickAt(m_tx, m_ty);
                m_deadline = nowMs + m_postMs;
                m_sub = Sub::Post;
                return false;
            }
            case Sub::Post:
                if (nowMs < m_deadline) return false;
                m_sub = Sub::Move;
                return true;
        }
        return false;
    }

    void Finish(InputPort& in, SequenceOutcome outcome, const char* status, int64_t nowMs) {
        m_phase = Phase::Idle;
        m_sub = Sub::Move;
        m_outcome = outcome;
        m_status = status;
        m_finishedAt = nowMs;
        if (m_haveCursor) in.RestoreCursor(m_savedX, m_savedY);
        m_haveCursor = false;
    }

    Phase m_phase = Phase::Idle;
    Sub m_sub = Sub::Move;
    ScreenRect m_toggle;
    std::vector<ScreenRect> m_items;
    size_t m_index = 0;
    int m_settleMs = 80, m_postMs = 150;
    int m_tx = 0, m_ty = 0;
    int64_t m_deadline = 0, m_verifyUntil = 0, m_watchdogAt = 0, m_finishedAt = 0;
    uint64_t m_verifyAfterSeq = 0;
    int64_t m_deferSeenMs = 0;
    int m_savedX = 0, m_savedY = 0;
    bool m_haveCursor = false;
    SequenceOutcome m_outcome = SequenceOutcome::None;
    std::string m_status;
};

}  // namespace RitualHelper
