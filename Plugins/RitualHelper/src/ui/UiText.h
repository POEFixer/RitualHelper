// Ritual Helper - text helpers of the drawing code. Every text a user reads is a key of
// lang/en.json, looked up in the program language (PluginI18n.h, included by PluginSDK.h);
// a LocText the scanner or the click sequence made is turned into words here, on the drawing
// thread. The game's own strings (reward names, its button texts) stay as the game writes them.
// Copyright (C) Omer Faruk ARPA and POEFixer contributors. Licensed under GPL-3.0 (see LICENSE).
#pragma once

#include "../game/LocText.h"
#include "../game/Text.h"

#include "../../../../POEFixer/plugin_sdk/PluginSDK.h"

#include <sal.h>

#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <string>

namespace RitualHelper {

namespace I18n = PluginSDK::I18n;

// A LocText in the program language (its key arguments too).
inline std::string ToText(const LocText& t) {
    return RenderLocText(t, [](const char* key) { return I18n::Tr(key); });
}

// printf for a TrF argument: Num("%.2f", value). The format is code, never a translation.
inline std::string Num(_In_z_ _Printf_format_string_ const char* format, ...) {
    char buf[64];
    va_list args;
    va_start(args, format);
    std::vsnprintf(buf, sizeof(buf), format, args);
    va_end(args);
    return buf;
}

// SliderFormat (a slider's printf format from a "{value} ms" text) lives in game/Text.h.

// At most `maxChars` characters of the UTF-8 `text`, cut by characters, never bytes (a typed
// name may be Russian or Thai).
inline std::string ShortenChars(const std::string& text, size_t maxChars) {
    size_t chars = 0;
    for (size_t i = 0; i < text.size(); ++i) {
        if ((static_cast<unsigned char>(text[i]) & 0xC0) == 0x80) continue;
        if (chars == maxChars) return text.substr(0, i);
        ++chars;
    }
    return text;
}

}  // namespace RitualHelper
