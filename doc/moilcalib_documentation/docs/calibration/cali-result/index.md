---
id: index
slug: /calibration/cali-result
title: Main Cali Result Overview
---

# Main Cali Result Overview

The main workspace for turning captured rounds into camera parameters. Load
Excel data, check each round, compute, inspect the graphs, search for the best
distance, and save the result.

Opened from **Calibration Result → Moil Cali Result** on the main window. It
opens at **1600 × 900** and is what the `cali_result` HELP key points at.

<Shot
  id="fig-1"
  number="1"
  file="cali-result-overview.png"
  caption="The Calibration Result window with data loaded."
  what="Load a complete calibration folder with Load All Excel, then capture the whole window maximised on the Aggr by Distance and Range tab, so the header, the tab strip and the panels all have content."
  height={580}
/>

---

## The tab strip

The window is one tab widget. **The tab index is the round number** — that
matters, because several actions use the current tab to decide what to act on.

| Tab | Index | Documented in |
|---|---:|---|
| `current` | 0 | [Main Window Overview](./main-window-overview.md) |
| `round_1` … `round_10` | 1–10 | [Result Table View](./result-table-view.md) |
| `parameter` | — | [Parameter View](./parameter-view.md) |
| `Overlap` | — | [Overlap & Aggregation View](./overlap-and-aggregation-view.md) |
| `Aggr by Distance and Range` | — | [Overlap & Aggregation View](./overlap-and-aggregation-view.md) |
| `Graphs` | — | [Entrance-Pupil Analysis](./entrance-pupil-analysis.md) |
| `test` | — | Internal. Not documented |

<Shot
  id="fig-2"
  number="2"
  file="cali-result-tab-strip.png"
  caption="The tab strip, with a star on a loaded round and one round turned off."
  what="Load data into a few rounds and turn one off by right-clicking its tab. Crop to the tab strip so both the ' *' marker and the ' [OFF]' marker are visible."
  height={90}
/>

### Two markers appear on tabs

| Marker | Meaning |
|---|---|
| **` *`** | That round holds data. Added by Update Table and by Excel loads; removed by Clear Table |
| **` [OFF]`** | That round is disabled and excluded from every computation |

### Right-click a round tab

<Shot
  id="fig-3"
  number="3"
  file="cali-result-tab-menu.png"
  caption="The round tab context menu."
  what="Right-click a round tab that has data and capture the open menu — three entries."
  height={200}
/>

| Entry | What it does |
|---|---|
| **Turn Off / Turn On Round N** | Excludes or includes that round. The table greys out and everything recomputes |
| **Show ZFL-IH Graph (Round N)** | That round alone, in a pop-up |
| **Show Overlap Graph (Round N)** | Same, overlap view |

> **Turning a round off is how you drop a bad capture** without deleting its
> data. It stays in the table, greyed out, and every later computation ignores
> it.

---

## Header and data management

The controls above the tab strip apply to whichever tab is current.

| Button | Object name | What it does |
|---|---|---|
| **Update Table** | `btn_update_table` | Fills the **current** round from the latest captures — see below |
| **Clear Table** | `btn_clear_table` | Blanks the current round's data cells and drops its star |
| **Clear All Table** | `btn_clear_all_table` | Same for every table. Asks first |
| **Load Excel** | `btn_load_excel` | One `.xlsx` into the current round |
| **Load All Excel** | `btn_load_all_excle` | A whole folder, rounds 1–10 |
| **Save to Excel** | `btn_save_to_excel` | The current round to `.xlsx` |
| **Load Database** | `btn_load_database` | Opens the [Calibration Data window](../../database/database-overview.md) |
| **Update All Cali Result** | `btn_update_all_cali_result` | Recomputes rounds 1–10 and redraws every plot |
| **Keep Round Data** | `btn_keep_round_data` | Copies the `current` tab into a numbered round |
| **Stop** | `btn_stop` | Cancels a running distance search |

<div className="custom-note custom-warning">
  <div className="custom-note-title">⚠️ CLEAR TABLE KEEPS THE HEADERS — BY DESIGN</div>
  <div>
    The round tables have <strong>no Qt header row</strong>. Their column titles live in table row 0, the units in row 1, and the layer index in the Side column. Clearing skips all three, because blanking them would destroy the table's headers permanently. This was a v2.0 fix — in earlier builds Clear Table wiped them.
  </div>
</div>

### Update Table

Fills the current round from a fresh capture. It needs three things, from three
different places:

| Input | Comes from |
|---|---|
| Image centres | The main window's four CPX/CPY fields |
| PCT values | The [Pattern Generator](../pct-pattern-generator.md) — 25 radii + 50 intervals |
| The captures | `image_cali/capture_positive_shot.png` and `capture_negative_shot.png` |

It runs 8-direction node detection on the two images using those centres, writes
the results into the round, recomputes, and marks the tab with a star. The
centres are also mirrored into that round's own display fields.

Missing captures give: *"Capture images not found
(image_cali/capture_*_shot.png)."*

### Keep Round Data

Copies the `current` tab into the round number typed in `lineedit_round`, up to
60 rows and 23 columns, **skipping empty source cells** so it merges rather than
overwrites. Then it recomputes everything.

---

## Editing the tables directly

Every round table is a spreadsheet. Full keyboard editing is wired up:

| Key | Action |
|---|---|
| `Ctrl+C` | Copy the selection as tab-separated text |
| `Ctrl+X` | Cut |
| `Ctrl+V` | Paste TSV, starting at the current cell |
| `Delete` | Blank the selection |
| `Ctrl+Z` | Undo |
| `Ctrl+Shift+Z` / `Ctrl+Y` | Redo |

**Undo holds the last 200 edit groups.** A fresh edit clears the redo history.

Because paste is plain TSV, a block copied out of Excel or from
[PCT Recommend](./pct-recommend.md) drops straight in.

---

## Select Cali System

The `combox_type_of_system` combo box — for example `Yuanman - SIDE (EV2785)` —
picks the rig profile. Choosing one applies that system's pixel sizes and the
horizontal and vertical gaps into the compute fields.

Selecting the same system twice does nothing; the change is de-duplicated.

**Save Configuration** (`btn_save_configuration_system`) writes the current
settings back out.

---

## Clean bezel noise

A checkbox added next to `cb_distance` at runtime, so you will not find it in the
`.ui` file.

> When it is on, **Update Table drops nodes that fall inside the monitor's dark
> bezel band**, detected from image brightness. Real edge rings are kept.

It shares one global flag with **Clean Noise** in the main window's header — turn
it on in either place and both are on.

---

## What changed from version 1.1

| Change | Where it shows |
|---|---|
| **PCT Recommend** — new window | [PCT Recommend](./pct-recommend.md) |
| **Load to System** downloads from SharePoint | [Load to System](../../database/load-to-system.md) |
| `cali_system_v2.db` ships with the app | **Load Database** resolves without a file dialog |
| Clear Table no longer wipes headers | Above |
| A star marks a loaded round | The tab strip |
| Per-round **Clean Noise** dropped | Replaced by the single global toggle |

<div className="custom-note custom-warning">
  <div className="custom-note-title">⚠️ LOAD DATABASE NOW ASKS YOU TO SIGN IN FIRST</div>
  <div>
    Every row in the database points at a shared folder, so the Microsoft Graph sign-in is requested <strong>before</strong> the window opens, and Cancel means no window:
    <br /><br />
    <em>"The calibration database needs the Microsoft Graph sign-in, because every row in it points at a shared folder. Click Load Database again when you have the tenant ID, client ID and client secret to hand."</em>
  </div>
</div>

---

## Screenshots this page needs

| Figure | File | What to capture |
|---|---|---|
| 1 | `cali-result-overview.png` | Whole window with a full calibration loaded |
| 2 | `cali-result-tab-strip.png` | Tab strip showing both ` *` and ` [OFF]` |
| 3 | `cali-result-tab-menu.png` | The round tab right-click menu |

---

## Source material

| What | Where |
|---|---|
| Layout and tab strip | `cpp/ui/cali_result.ui` |
| Behaviour | `cpp/src/controllers/controller_cali_result.{h,cpp}` |
| Compute core | `cpp/src/core/cali/{CaliCompute,CaliMath,CaliRound,Regression}.cpp` |
| Excel import/export | `cpp/src/io/XlsxIO.cpp` |
| Rig profiles | `cpp/config/cali_system/*.json` |
