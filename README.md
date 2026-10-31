# Ritual Helper

Current version: 1.6.0

Requires POE2Fixer 364 or newer.

Ritual Helper highlights the rewards you want in the Ritual **Favours** window and
defers them with one click, so valuable items wait for a later ritual instead of
slipping by.

## What you get

- **Highlights** - every reward that will be deferred gets a coloured frame, with its
  price (the whole stack) in the corner.
- **One-click defer** - a **DEFER (N)** button appears next to the hourglass. It
  switches defer mode on, clicks each highlighted reward and confirms. Right-click,
  Esc or moving the mouse stops it at any time; afterwards the cursor returns where
  it was.
- **Pick by name** - tick items in a searchable list of currency, omens, essences,
  runes, fragments, gems and all uniques, sorted by price within each category.
- **Pick by value** - optionally defer any revealed reward worth at least a set
  number of Exalted or Divine Orbs.

## How to use it

1. Install **Ritual Helper** from the **Plugin Manager** (the first page of the
   Plugins tab), which also keeps it up to date; it is on right after installing.
2. Under **Items to defer**, tick what you want (search finds any item; a name the
   list does not know, such as an event reward, can be ticked as typed). Or set a
   **Minimum value**.
3. Open a ritual's Favours window. The rewards to defer are framed; press **DEFER**.

## Settings

**Overlay**
- *Show the overlay* - draws the highlights, values and the DEFER button. Turn it off
  to pause the plugin without disabling it.
- *Show values on highlighted rewards* - the price label in each frame.
- *Highlight colour* - colour of the frames.
- *Re-read the window every* - how often the open Favours window is read again
  (150-2000 ms). With no window open the plugin looks about once a second.

**Defer by value**
- *Minimum value* and its unit - rewards worth at least this much are deferred too;
  0 turns the rule off. Values count the whole stack.
- *Show prices in* - Exalted or Divine for labels and lists.
- Prices come from the overlay's price service: **Settings > Prices** chooses the
  league and the source.

**Clicking**
- *Time per click* - how long the cursor rests on each reward around its click
  (120-800 ms). Raise it if rewards are skipped on a slow connection.

**Items to defer**
- *Search* - filters every category; *Tick "..."* adds a typed name.
- *Ticked* - everything you ticked, with a remove button and *Untick all*.
- *In the open Favours window* - the window's current rewards, to tick directly.
- Categories - each with *Tick all* / *Untick all*.

**Diagnostics** (collapsed)
- *Dry run* - DEFER only lists what it would defer in the log; no clicks are sent.
- *Show scan status on screen* - one status line in the top-left corner.
- Live status of the window and its buttons, a defer preview, and *Write discovery
  dump* (saves `debug/ritual-dump.txt` in the plugin folder for bug reports).

## Safety

- Rewards are clicked only after the game shows defer mode is on, so a click can
  never pick a reward for purchase instead.
- The run stops when the game loses focus, the overlay menu opens, the Favours window
  closes, the mouse moves, or you right-click or press Esc.
- Unrevealed ("Hidden Item") rewards are never deferred automatically.

## Known limits

- The game's buttons are recognised by their text: with POEFixer 371 or later in
  every game language (the game's own "Defer Item" text in the game language, and
  the other button under the reward grid as "Offer Tribute"); an older POEFixer
  needs an English game client for the DEFER button. Highlights work in every
  language.
- The item list is in English. With POEFixer 371 or later rewards are matched by
  their English name in every game language (a reward ticked earlier under the game's
  own name still counts); an older POEFixer matches them as the game writes them, so
  tick rewards under *In the open Favours window* there.
- Unrevealed rewards are recognised by their item path in every game language.
- If the Favours window or its buttons stop being recognised after a game patch, use
  *Write discovery dump* under Diagnostics and attach the file to your report.

## Install

Install it from POE2Fixer → Plugins → Plugin Manager (one click, updates included), or download `RitualHelper.zip` from [Releases](https://github.com/POEFixer/RitualHelper/releases/latest) and extract it into the POE2Fixer folder (it creates `Plugins\RitualHelper\`). Then enable the plugin in the Plugins tab.

## Build from source

This repository is the plugin folder from the POE2Fixer source tree (`Plugins/RitualHelper/`) together with the host files it compiles against, kept at the same relative paths:

- `POEFixer/plugin_sdk/` - the POE2Fixer Plugin SDK v6 headers (`PluginAbi.h`, `PluginSDK.h`)
- `POEFixer/imgui/` - the Dear ImGui sources the DLL compiles against
- `POEFixer/core/` - small self-contained helper headers used by the plugin
- `tools/plugin_i18n/gen.py` - the translation tool: after editing `Plugins/RitualHelper/lang/en.json` run `python tools/plugin_i18n/gen.py Plugins/RitualHelper` (it rewrites the embedded English header); `--gate` checks every language file and the keys the sources use

1. Install Visual Studio 2022 with the "Desktop development with C++" workload (MSVC v143, Windows 10/11 SDK).
2. Open `RitualHelper.sln`, select **Release | x64** and build the solution.
3. The plugin is written to `x64\Release\Plugins\RitualHelper\RitualHelper.dll` (Release builds carry no debug information, see `Directory.Build.targets`).
4. Copy `RitualHelper.dll` into `Plugins\RitualHelper\` next to the POE2Fixer executable, together with `Plugins\RitualHelper\lang` from this repository (same folder names).

## Credits

Originally created by Omer Faruk ARPA
(https://github.com/omrfarukarpa/poefixer-ritual-helper). Maintained by POEFixer since
October 2026.

Licensed under GPL-3.0, see LICENSE.
