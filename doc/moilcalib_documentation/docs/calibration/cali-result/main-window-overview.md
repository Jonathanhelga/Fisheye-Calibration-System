---
id: main-window-overview
slug: /calibration/cali-result/main-window-overview
title: Main Window Overview
---

# Main Window Overview

The frame of the Calibration Result window: the header, the folder browser, the
tab strip, and the `current` tab that sits at index 0.

Everything here is visible whichever tab you are on, so it is worth knowing
before the per-tab pages.

<Shot
  id="fig-1"
  number="1"
  file="cali-result-frame.png"
  caption="The window frame: folder tree and header controls above the tab strip."
  what="Load a calibration folder, then capture the top third of the window — the folder path field, the tree, the header buttons and the tab strip. The per-tab content below can be cut off."
  height={420}
/>

---

## 1. The three areas that never change

| Area | What it is |
|---|---|
| **Cali Folder & file tree** | Pick a folder and browse what is in it |
| **Data management buttons** | Load, clear, update, stop, save |
| **Tab strip** | 16 tabs — see [the overview](./index.md#the-tab-strip) |

---

## 2. Cali Folder and the file tree

| Control | Object name | What it does |
|---|---|---|
| Folder path | `folder_path` | Type or paste a path and press Enter to load it |
| Tree | `tree_view_folder_path` | A file browser rooted at the working directory |

Only the name column is shown; size, type and date are hidden.

### Double-clicking in the tree

| What you double-click | What happens |
|---|---|
| An `.xlsx` file | Loaded into the round **guessed from its path** |
| A folder | Loaded as a complete set — every round |

<div className="custom-note custom-warning">
  <div className="custom-note-title">⚠️ THE ROUND IS GUESSED, AND DEFAULTS TO 1</div>
  <div>
    It looks for a standalone <code>1</code>–<code>10</code> in the filename, then for a path component that is a bare number in that range. <strong>Finding neither, it uses round 1.</strong> So <code>results_final.xlsx</code> silently overwrites round 1, and <code>scan_2024.xlsx</code> lands in round 2 because of the year.
  </div>
</div>

Typing a folder into the path field and pressing Enter does the same as
double-clicking it.

---

## 3. Header and data management

| Button | Object name | Scope |
|---|---|---|
| **Update Table** | `btn_update_table` | Current round, from fresh captures |
| **Clear Table** | `btn_clear_table` | Current table |
| **Clear All Table** | `btn_clear_all_table` | Every table. Asks first |
| **Load Excel** | `btn_load_excel` | One file into the current round |
| **Load All Excel** | `btn_load_all_excle` | A folder, rounds 1–10 |
| **Save to Excel** | `btn_save_to_excel` | Current round |
| **Load Database** | `btn_load_database` | Opens the Calibration Data window |
| **Update All Cali Result** | `btn_update_all_cali_result` | Rounds 1–10, plus every plot |
| **Keep Round Data** | `btn_keep_round_data` | `current` → a numbered round |
| **Stop** | `btn_stop` | Cancels a running search |

> **Several of these act on "the current tab".** Update Table, Clear Table, Load
> Excel and Save to Excel all read the tab index and clamp it to 0–10. On the
> `parameter`, `Overlap` or `Graphs` tabs that clamp lands on round 10 — so check
> which tab is selected before pressing them.

### Loaded-data indicators

| Marker | Set by | Cleared by |
|---|---|---|
| **` *`** on a tab | Update Table, Load Excel, Load All Excel — when data actually arrived | Clear Table, Clear All Table |
| **` [OFF]`** on a tab | Right-click → Turn Off Round N | Turn On Round N |

**Load All Excel resets every star first**, then sets them per round that
loaded — so stale stars cannot survive a reload.

---

## 4. Load Database

Opens the [Calibration Data window](../../database/database-overview.md).

<div className="custom-note custom-warning">
  <div className="custom-note-title">⚠️ IT ASKS YOU TO SIGN IN BEFORE THE WINDOW OPENS</div>
  <div>
    Every row in the database points at a shared folder, so the Microsoft Graph credentials are requested first and <strong>Cancel means no window</strong>:
    <br /><br />
    <em>"The calibration database needs the Microsoft Graph sign-in, because every row in it points at a shared folder. Click Load Database again when you have the tenant ID, client ID and client secret to hand."</em>
  </div>
</div>

### The database file resolves itself

`cali_system_v2.db` now ships with the application, so no file dialog appears. It
is searched for in order:

```
database/cali_system_v2.db
../database/cali_system_v2.db
../../database/cali_system_v2.db
mvc_controller/database/cali_system_v2.db
cali_system_v2.db
```

— under both the working directory and the application directory. **Only if none
of those exist** are you asked to locate it, so the button is never dead.

This is a v2.0 change: v1.1 always opened a file dialog.

---

## 5. Select Cali System

`combox_type_of_system` — for example `Yuanman - SIDE (EV2785)`.

Choosing a system applies that rig's **pixel sizes** and **horizontal and
vertical gaps** into the compute fields. Selecting the one already active does
nothing; the change is de-duplicated.

**Save Configuration** (`btn_save_configuration_system`) writes `main.json`:

```json
{
  "systemType": "Yuanman - SIDE (EV2785)",
  "distance_per_round": 20
}
```

That file is read back by **Load All Excel** if it sits in the folder, which is
what makes a stored calibration reload with the right system and spacing already
chosen.

---

## 6. Single distance

`cb_distance` switches how the distance per round is derived:

| State | Behaviour |
|---|---|
| **Off** | Base distance plus `distance_per_round` × round number |
| **On** | Each round uses its own single distance |

Toggling it triggers a full recompute.

### Clean bezel noise

A second checkbox is **added next to it at runtime**, so you will not find it in
the `.ui` file:

> **Clean bezel noise** — *"Update Table drops nodes inside the monitor's dark
> bezel band, detected from image brightness. Keeps real edge rings."*

It shares one global flag with **Clean Noise** in the main window's header. Turn
it on in either place and both show it on.

---

## 7. Keep Round Data

Copies the `current` tab into the round number in `lineedit_round`.

| Detail | Value |
|---|---|
| Rows copied | up to 60 |
| Columns copied | up to 23 |
| Empty source cells | **skipped** |

Because empties are skipped it **merges into** the destination rather than
replacing it. Then it recomputes everything.

> Use it to promote a scratch measurement into a real round without retyping. Be
> aware that the destination's existing values survive wherever the source is
> blank — clear the destination first if you want a clean copy.

---

## 8. Editing the tables

Every round table is a spreadsheet with full keyboard support:

| Key | Action |
|---|---|
| `Ctrl+C` / `Ctrl+X` / `Ctrl+V` | Copy / cut / paste as TSV |
| `Delete` | Blank the selection |
| `Ctrl+Z` | Undo — up to 200 groups |
| `Ctrl+Shift+Z` / `Ctrl+Y` | Redo |

Details in [Result Table View](./result-table-view.md#6-editing-by-hand).

---

## Troubleshooting

| Symptom | Cause |
|---|---|
| A file loaded into the wrong round | The round guess defaulted to 1, or matched a number in the filename |
| Update Table hit round 10 unexpectedly | You were on a non-round tab; the index clamps to 0–10 |
| A tab keeps its star after clearing | Only Clear Table and Clear All Table drop stars |
| Load Database asks to sign in | Expected — it gates on Microsoft Graph credentials |
| Asked to locate `cali_system_v2.db` | None of the five search paths held it |
| Clean Noise changed on its own | It shares one global flag with the main window's header toggle |
| Keep Round Data left old values behind | Empty source cells are skipped, so it merges |

---

## Screenshots this page needs

| Figure | File | What to capture |
|---|---|---|
| 1 | `cali-result-frame.png` | Folder tree, header buttons, tab strip |

Three more worth having:

| File | What to capture |
|---|---|
| `cali-result-folder-tree.png` | The tree with a loaded calibration folder expanded, subfolders `1`–`10` visible |
| `cali-result-header-buttons.png` | The header button row, cropped tight |
| `cali-result-signin-prompt.png` | The *"The calibration database needs the Microsoft Graph sign-in…"* message |

---

## Source material

| What | Where |
|---|---|
| Layout | `cpp/ui/cali_result.ui` |
| Behaviour | `cpp/src/controllers/controller_cali_result.cpp` |
| Database resolution | `ControllerCaliResult::openDatabase` |
| Rig profiles | `cpp/config/cali_system/*.json` |
