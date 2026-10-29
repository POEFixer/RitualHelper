// Ritual Helper - small ASCII-only text helpers (names are UTF-8; only ASCII letters fold).
// Copyright (C) Omer Faruk ARPA and POEFixer contributors. Licensed under GPL-3.0 (see LICENSE).
#pragma once

#include <cstddef>
#include <string>
#include <string_view>

namespace RitualHelper {

inline char LowerAscii(char c) {
    return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
}

// Case-folded (ASCII), trimmed key used to compare item names.
inline std::string NameKey(std::string_view s) {
    size_t b = 0, e = s.size();
    while (b < e && (s[b] == ' ' || s[b] == '\t')) ++b;
    while (e > b && (s[e - 1] == ' ' || s[e - 1] == '\t')) --e;
    std::string out;
    out.reserve(e - b);
    for (size_t i = b; i < e; ++i) out.push_back(LowerAscii(s[i]));
    return out;
}

// A slider's printf format from a text with a {value} ("{value} ms"): a '%' of the text is
// doubled first, then the FIRST {value} becomes the conversion. The translation itself is never
// a format, and a later {value} stays literal text: ImGui passes one number, so a second
// conversion would read an argument that is not there.
inline std::string SliderFormat(std::string_view text, const char* conversion = "%d") {
    constexpr std::string_view kValue = "{value}";
    std::string out;
    bool placed = false;
    for (size_t i = 0; i < text.size();) {
        if (!placed && text.compare(i, kValue.size(), kValue) == 0) {
            out += conversion ? conversion : "";
            placed = true;
            i += kValue.size();
            continue;
        }
        if (text[i] == '%') out.push_back('%');
        out.push_back(text[i++]);
    }
    return out;
}

inline bool ContainsCI(std::string_view hay, std::string_view needle) {
    if (needle.empty() || needle.size() > hay.size()) return false;
    for (size_t i = 0; i + needle.size() <= hay.size(); ++i) {
        size_t j = 0;
        for (; j < needle.size(); ++j)
            if (LowerAscii(hay[i + j]) != LowerAscii(needle[j])) break;
        if (j == needle.size()) return true;
    }
    return false;
}

}  // namespace RitualHelper
