// Ritual Helper - real mouse/keyboard input for the click sequence (SendInput only:
// the game ignores posted window messages).
// Copyright (C) Omer Faruk ARPA and POEFixer contributors. Licensed under GPL-3.0 (see LICENSE).
#pragma once

#include "../game/DeferSequence.h"

#include <Windows.h>
#include <cmath>

namespace RitualHelper {

class Win32Input final : public InputPort {
public:
    // The game window: UI rectangles are relative to its client area, the mouse
    // works in screen coordinates. Null = a borderless window at the desktop origin.
    void SetGameWindow(HWND hwnd) { m_hwnd = hwnd; }

    bool ClientToScreen(float cx, float cy, int& sx, int& sy) override {
        POINT pt{static_cast<LONG>(std::lround(cx)), static_cast<LONG>(std::lround(cy))};
        if (m_hwnd) {
            if (!::IsWindow(m_hwnd) || !::ClientToScreen(m_hwnd, &pt)) return false;
        }
        sx = pt.x;
        sy = pt.y;
        return true;
    }

    bool GetCursor(int& sx, int& sy) override {
        POINT pt{};
        if (!::GetCursorPos(&pt)) return false;
        sx = pt.x;
        sy = pt.y;
        return true;
    }

    void MoveTo(int sx, int sy) override {
        INPUT in{};
        in.type = INPUT_MOUSE;
        Normalize(sx, sy, in.mi.dx, in.mi.dy);
        in.mi.dwFlags = MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_VIRTUALDESK;
        ::SendInput(1, &in, sizeof(INPUT));
    }

    // Both button events carry the absolute point, so the click lands on the target
    // even if something nudged the cursor between the move and the click.
    void ClickAt(int sx, int sy) override {
        INPUT in[2]{};
        LONG nx = 0, ny = 0;
        Normalize(sx, sy, nx, ny);
        const DWORD move = MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_VIRTUALDESK;
        in[0].type = INPUT_MOUSE;
        in[0].mi.dx = nx;
        in[0].mi.dy = ny;
        in[0].mi.dwFlags = move | MOUSEEVENTF_LEFTDOWN;
        in[1] = in[0];
        in[1].mi.dwFlags = move | MOUSEEVENTF_LEFTUP;
        ::SendInput(2, in, sizeof(INPUT));
    }

    void RestoreCursor(int sx, int sy) override { ::SetCursorPos(sx, sy); }

    bool RightButtonDown() override { return (::GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0; }
    bool EscapeDown() override { return (::GetAsyncKeyState(VK_ESCAPE) & 0x8000) != 0; }

private:
    static void Normalize(int sx, int sy, LONG& nx, LONG& ny) {
        const int vsX = ::GetSystemMetrics(SM_XVIRTUALSCREEN);
        const int vsY = ::GetSystemMetrics(SM_YVIRTUALSCREEN);
        int vsW = ::GetSystemMetrics(SM_CXVIRTUALSCREEN);
        int vsH = ::GetSystemMetrics(SM_CYVIRTUALSCREEN);
        if (vsW <= 1) vsW = ::GetSystemMetrics(SM_CXSCREEN);
        if (vsH <= 1) vsH = ::GetSystemMetrics(SM_CYSCREEN);
        nx = static_cast<LONG>(std::lround(static_cast<double>(sx - vsX) * 65535.0 / (vsW > 1 ? vsW - 1 : 1)));
        ny = static_cast<LONG>(std::lround(static_cast<double>(sy - vsY) * 65535.0 / (vsH > 1 ? vsH - 1 : 1)));
    }

    HWND m_hwnd = nullptr;
};

}  // namespace RitualHelper
