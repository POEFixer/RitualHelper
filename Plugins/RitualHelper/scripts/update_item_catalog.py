#!/usr/bin/env python3
"""Regenerate src/data/ItemCatalog.h, the list of item names Ritual Helper offers in
its "Items to defer" picker.

Prices do NOT come from this file: the plugin asks the overlay's price service for
every name at run time. The catalog only provides the names and their category, so
the picker can show a browsable, searchable list (the price service cannot list its
items). Run it after a league or patch adds new currency or uniques:

    python scripts/update_item_catalog.py                  # every softcore league poe2scout lists
    python scripts/update_item_catalog.py --league "Standard" --league "Runes of Aldur"

The script uses only the Python standard library and the public poe2scout API. It
merges every league it reads (an event league often has no unique data of its own),
keeps each name once, and writes the header with non-ASCII bytes escaped so the file
compiles the same under any source code page.
"""

import argparse
import json
import pathlib
import sys
import time
import urllib.parse
import urllib.request

API = "https://api.poe2scout.com/poe2"
USER_AGENT = "RitualHelper-catalog-generator/1.0"

# (C++ enumerator, poe2scout category id, kind) in the order the picker shows them.
# The category ids are the vocabulary the overlay's price service uses for both of
# its sources (poe2scout and poe.ninja).
CATEGORIES = [
    ("Currency", "currency", "currency"),
    ("Essence", "essences", "currency"),
    ("Rune", "runes", "currency"),
    ("SoulCore", "ultimatum", "currency"),
    ("Catalyst", "breach", "currency"),
    ("Delirium", "delirium", "currency"),
    ("Incursion", "incursion", "currency"),
    ("Idol", "idol", "currency"),
    ("Ritual", "ritual", "currency"),
    ("VaultKey", "vaultkeys", "currency"),
    ("Fragment", "fragments", "currency"),
    ("Abyss", "abyss", "currency"),
    ("UncutGem", "uncutgems", "currency"),
    ("LineageGem", "lineagesupportgems", "currency"),
    ("Verisium", "verisium", "currency"),
    ("Vaal", "vaal", "currency"),
    ("Expedition", "expedition", "currency"),
    ("UniqueWeapon", "weapon", "unique"),
    ("UniqueArmour", "armour", "unique"),
    ("UniqueAccessory", "accessory", "unique"),
    ("UniqueJewel", "jewel", "unique"),
    ("UniqueFlask", "flask", "unique"),
    ("UniqueMap", "map", "unique"),
    ("UniqueSanctum", "sanctum", "unique"),
]

MAX_PAGES = 20
PER_PAGE = 250


def get_json(path):
    req = urllib.request.Request(API + path, headers={"User-Agent": USER_AGENT})
    for attempt in range(4):
        try:
            with urllib.request.urlopen(req, timeout=30) as resp:
                return json.load(resp)
        except Exception as exc:  # network hiccup or rate limit: back off and retry
            if attempt == 3:
                raise RuntimeError(f"GET {path} failed: {exc}") from exc
            time.sleep(2.0 * (attempt + 1))
    return None


def softcore_leagues():
    leagues = []
    for entry in get_json("/Leagues") or []:
        value = (entry or {}).get("Value") or ""
        short = (entry or {}).get("ShortName") or ""
        if not value or value.startswith("HC ") or value == "Hardcore" or short.endswith("hc"):
            continue
        leagues.append(value)
    return leagues


def collect(league, scout_cat, kind, sink):
    quoted = urllib.parse.quote(league)
    endpoint = "Currencies" if kind == "currency" else "Uniques"
    name_key = "Text" if kind == "currency" else "Name"
    page = 1
    while page <= MAX_PAGES:
        body = get_json(f"/Leagues/{quoted}/{endpoint}/ByCategory?Category={scout_cat}"
                        f"&PerPage={PER_PAGE}&Page={page}")
        items = (body or {}).get("Items") or []
        for item in items:
            name = (item or {}).get(name_key) or ""
            name = name.strip()
            if name and len(name.encode("utf-8")) <= 96:
                sink.add(name)
        pages = int((body or {}).get("Pages") or 0)
        if page >= pages:
            break
        page += 1
        time.sleep(0.15)


def cpp_literal(text):
    """A C++ string literal with every non-ASCII byte escaped as \\xHH. When a hex
    digit follows an escape, the literal is split so the escape cannot swallow it."""
    out = []
    after_escape = False
    for ch in text:
        if 32 <= ord(ch) < 127:
            if after_escape and ch in "0123456789abcdefABCDEF":
                out.append('" "')
            out.append("\\" + ch if ch in ('"', "\\") else ch)
            after_escape = False
        else:
            for b in ch.encode("utf-8"):
                out.append(f"\\x{b:02X}")
            after_escape = True
    return '"' + "".join(out) + '"'


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--league", action="append",
                        help="league to read (repeatable); default: every softcore league poe2scout lists")
    parser.add_argument("--out", default=str(pathlib.Path(__file__).resolve().parent.parent
                                             / "src" / "data" / "ItemCatalog.h"))
    args = parser.parse_args()

    leagues = args.league or softcore_leagues()
    if not leagues:
        print("no leagues to read", file=sys.stderr)
        return 1

    names = {enum: set() for enum, _, _ in CATEGORIES}
    for league in leagues:
        for enum, scout_cat, kind in CATEGORIES:
            collect(league, scout_cat, kind, names[enum])
            time.sleep(0.15)
        print(f"read {league}", file=sys.stderr)

    seen = set()
    rows = []
    for enum, _, _ in CATEGORIES:
        for name in sorted(names[enum], key=lambda n: n.casefold()):
            if name in seen:  # a name lives in its first category only
                continue
            seen.add(name)
            rows.append((enum, name))

    lines = [
        "// GENERATED by scripts/update_item_catalog.py from the public poe2scout API.",
        "// Do not edit by hand: rerun the script. Leagues read: " + ", ".join(leagues) + ".",
        "// Names only: prices come from the overlay's price service at run time.",
        "#pragma once",
        "",
        "#include \"../game/Category.h\"",
        "",
        "#include <cstddef>",
        "",
        "namespace RitualHelper {",
        "",
        "struct CatalogEntry {",
        "    Category category;",
        "    const char* name;  // UTF-8",
        "};",
        "",
        "inline constexpr CatalogEntry kItemCatalog[] = {",
    ]
    for enum, name in rows:
        lines.append(f"    {{Category::{enum}, {cpp_literal(name)}}},")
    lines += [
        "};",
        "",
        "inline constexpr size_t kItemCatalogCount = sizeof(kItemCatalog) / sizeof(kItemCatalog[0]);",
        "",
        "}  // namespace RitualHelper",
        "",
    ]
    out = pathlib.Path(args.out)
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text("\r\n".join(lines), encoding="ascii", newline="")
    print(f"wrote {len(rows)} names to {out}", file=sys.stderr)
    return 0


if __name__ == "__main__":
    sys.exit(main())
