# Changelog

## 1.5.8

- Works with the separate menu window (POEFixer 366): the host hands the plugin the right drawing context for its settings tab and for its overlay.

## 1.5.7

- Rebuilt with the updated plugin SDK headers (POEFixer 365 adds optional calls for UI control names, stash objects and tabs, wider entity scans and the host automation flag). No change in what the plugin does.

## 1.5.6

- The "?" next to a section title now sits in its own gap instead of on the section's line.
- The bottom of the settings tab names the plugin's original author, as in the other plugins maintained by POEFixer.

## 1.5.5

- Now part of POEFixer: prices come from the overlay's price service (Settings > Prices
  picks the league and source), so the plugin no longer downloads prices itself.
- The Favours window is found on its own, even when no other plugin is scanning
  inventories, and is read in the background: no more stutter while it is open.
- DEFER clicks the right places in windowed mode, stops when the window closes, the
  overlay menu opens, the mouse moves or Esc is pressed, and waits for defer mode to
  really be on.
- Values count the whole stack, and a plain base item no longer takes the price of a
  unique on the same base.
- New settings page: cards, a search over every item, a list of what you ticked, and
  ticking straight from the open Favours window. New: highlight colour and time per
  click.
- Highlights and labels scale with the font and stay readable on bright scenes.
- Removed: the plugin's own enable switch, league selector and price refresh options
  (the Plugins tab and Settings > Prices cover them). Your ticked items and other
  settings are kept.
- A damaged or locked settings file is no longer replaced by the defaults: it is kept
  (copied to settings.json.corrupt) until you change a setting.

## 1.5.4

Last release by the original author (https://github.com/omrfarukarpa/poefixer-ritual-helper).
