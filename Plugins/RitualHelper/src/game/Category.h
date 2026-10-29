// Ritual Helper - item categories of the "Items to defer" picker.
// Copyright (C) Omer Faruk ARPA and POEFixer contributors. Licensed under GPL-3.0 (see LICENSE).
#pragma once

#include <cstring>

namespace RitualHelper {

// Picker sections, in display order. The generated catalog (data/ItemCatalog.h)
// uses these enumerators; scripts/update_item_catalog.py lists the same order.
enum class Category : int {
    Currency = 0,
    Essence,
    Rune,
    SoulCore,
    Catalyst,
    Delirium,
    Incursion,
    Idol,
    Ritual,
    VaultKey,
    Fragment,
    Abyss,
    UncutGem,
    LineageGem,
    Verisium,
    Vaal,
    Expedition,
    UniqueWeapon,
    UniqueArmour,
    UniqueAccessory,
    UniqueJewel,
    UniqueFlask,
    UniqueMap,
    UniqueSanctum,
    Count
};

inline constexpr int kCategoryCount = static_cast<int>(Category::Count);

// The sections' names are texts of the settings tab (ui/SettingsUi.cpp, CategoryText).

inline bool IsUniqueSection(Category c) {
    return static_cast<int>(c) >= static_cast<int>(Category::UniqueWeapon) &&
           static_cast<int>(c) <  static_cast<int>(Category::Count);
}

// True for the price service's unique-item category ids (the poe2scout vocabulary it
// uses for both of its sources). A price found under one of these belongs to a
// unique: a non-unique reward that only shares a word or a base type with it must
// not take that price (the service aliases each base type to its priciest unique).
inline bool IsUniquePriceCategory(const char* category) {
    if (!category || !*category) return false;
    static const char* const kIds[] = {"accessory", "armour", "flask", "jewel", "map", "weapon", "sanctum"};
    for (const char* id : kIds)
        if (std::strcmp(category, id) == 0) return true;
    return false;
}

}  // namespace RitualHelper
