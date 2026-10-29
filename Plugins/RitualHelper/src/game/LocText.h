// Ritual Helper - a text decided away from the drawing code (the scanner thread, the click
// sequence): a key of lang/en.json and the values of its {name} placeholders. The drawing code
// turns it into words in the program language (ui/UiText.h, I18n::Tr); the file log reads the
// English. STL only, so the pure logic and its tests stay free of PluginSDK.h and compare keys.
// Copyright (C) Omer Faruk ARPA and POEFixer contributors. Licensed under GPL-3.0 (see LICENSE).
#pragma once

#include <cstring>
#include <string>
#include <utility>
#include <vector>

namespace RitualHelper {

namespace Loc {
// A key written where it is not looked up: the key itself. It marks the literal for
// tools/plugin_i18n/gen.py --gate as PluginSDK::I18n::Key does in code that includes the SDK.
constexpr const char* Key(const char* key) { return key; }
}  // namespace Loc

struct LocText {
    std::string key;                                         // empty: nothing to show
    std::vector<std::pair<std::string, std::string>> args;   // {name} -> value
    // {name} -> another key of lang/en.json, worded when the text is drawn (a text that stays
    // on screen follows a language switch; the log gets its English).
    std::vector<std::pair<std::string, std::string>> keyArgs;

    LocText() = default;
    LocText(const char* k) : key(k ? k : "") {}  // implicit: Finish(..., Loc::Key("status_x"), ...)
    LocText(const char* k, std::vector<std::pair<std::string, std::string>> a) : key(k ? k : ""), args(std::move(a)) {}
    LocText(const char* k, std::vector<std::pair<std::string, std::string>> a,
            std::vector<std::pair<std::string, std::string>> keyA)
        : key(k ? k : ""), args(std::move(a)), keyArgs(std::move(keyA)) {}

    bool empty() const { return key.empty(); }
    bool operator==(const LocText&) const = default;
};

// `text` with every {name} of `args` replaced by its value (an unknown {name} stays; values are
// never scanned again).
inline std::string FillPlaceholders(const char* text, const std::vector<std::pair<std::string, std::string>>& args) {
    std::string out;
    if (!text) return out;
    for (const char* p = text; *p;) {
        if (*p == '{') {
            if (const char* close = std::strchr(p + 1, '}')) {
                const std::string name(p + 1, static_cast<size_t>(close - p - 1));
                const std::pair<std::string, std::string>* hit = nullptr;
                for (const auto& a : args)
                    if (a.first == name) {
                        hit = &a;
                        break;
                    }
                if (hit) {
                    out += hit->second;
                    p = close + 1;
                    continue;
                }
            }
        }
        out.push_back(*p++);
    }
    return out;
}

// The words of `t` through `lookup` (key -> text): the program language on screen (ui/UiText.h
// ToText), the embedded English for the file log. Key arguments are looked up the same way.
template <class Lookup>
std::string RenderLocText(const LocText& t, Lookup&& lookup) {
    if (t.empty()) return {};
    if (t.keyArgs.empty()) return FillPlaceholders(lookup(t.key.c_str()), t.args);
    std::vector<std::pair<std::string, std::string>> args = t.args;
    for (const auto& [name, key] : t.keyArgs) args.emplace_back(name, lookup(key.c_str()));
    return FillPlaceholders(lookup(t.key.c_str()), args);
}

}  // namespace RitualHelper
