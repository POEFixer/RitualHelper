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

inline const char* CategoryName(Category c) {
    switch (c) {
        case Category::Currency:        return "Currency";
        case Category::Essence:         return "Essences";
        case Category::Rune:            return "Runes";
        case Category::SoulCore:        return "Soul Cores";
        case Category::Catalyst:        return "Catalysts";
        case Category::Delirium:        return "Delirium";
        case Category::Incursion:       return "Incursion";
        case Category::Idol:            return "Idols";
        case Category::Ritual:          return "Omens";
        case Category::VaultKey:        return "Vault Keys";
        case Category::Fragment:        return "Fragments";
        case Category::Abyss:           return "Abyss";
        case Category::UncutGem:        return "Uncut Gems";
        case Category::LineageGem:      return "Lineage Gems";
        case Category::Verisium:        return "Verisium and Alloys";
        case Category::Vaal:            return "Vaal";
        case Category::Expedition:      return "Expedition";
        case Category::UniqueWeapon:    return "Unique Weapons";
        case Category::UniqueArmour:    return "Unique Armour";
        case Category::UniqueAccessory: return "Unique Accessories";
        case Category::UniqueJewel:     return "Unique Jewels";
        case Category::UniqueFlask:     return "Unique Flasks and Charms";
        case Category::UniqueMap:       return "Unique Tablets and Maps";
        case Category::UniqueSanctum:   return "Unique Relics";
        default:                        return "Other";
    }
}

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
