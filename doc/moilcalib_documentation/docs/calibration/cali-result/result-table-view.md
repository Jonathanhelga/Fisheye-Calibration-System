---
id: result-table-view
slug: /calibration/cali-result/result-table-view
title: Result Table View
---

# Result Table View

The `round_1` … `round_10` tabs: one table per round, holding the measured node
radii and everything computed from them.

<Shot
  id="fig-1"
  number="1"
  file="cali-result-round-table.png"
  caption="A round table with measured and computed columns filled."
  what="Load a round with Load Excel, click Calculate Result, then capture the round tab. Scroll so both the ICT columns and the computed ICT avg / PCT_CAL / Distance columns are visible in one frame if you can."
  height={520}
/>

---

## 1. The table has no Qt header row

This is the single most important structural fact about these tables, and it
explains several behaviours that otherwise look like bugs.

| Table row | Holds |
|---|---|
| **Row 0** | The column titles |
| **Row 1** | The units |
| **Rows 2+** | The data — one row per pattern layer |

So **layer `n` is at table row `n + 2`**, and the layer index itself lives in the
**Side** column, written once when the window is built and never rewritten.

<div className="custom-note custom-warning">
  <div className="custom-note-title">⚠️ THIS IS WHY CLEAR TABLE SKIPS THREE THINGS</div>
  <div>
    Clearing blanks the data cells only — <strong>rows 0 and 1, and the Side column, are left alone</strong>. Blanking them would delete the table's headers permanently, with nothing to restore them from. In v2.0 Clear Table was fixed to respect that; earlier builds wiped them.
  </div>
</div>

---

## 2. Column map

35 columns. The layout is fixed in code, and Excel import and export both depend
on it.

### Measured — filled by Update Table or an Excel load

| # | Column | Meaning |
|---:|---|---|
| 0 | `round` | Round number. A `*` in a layer row marks the last used layer |
| 1 | `side` | **Layer index.** A label, never data |
| 2 | `pct` | The PCT value for this layer, from the Pattern Generator |
| 3–10 | `ict_n`, `ict_s`, `ict_w`, `ict_e`, `ict_nw`, `ict_se`, `ict_sw`, `ict_ne` | **The measured intersection radius in each of the eight directions** |
| 11 | — | Spacer |

### Computed — filled by Calculate Result

| # | Column | Meaning |
|---:|---|---|
| 12 | `ict_avg` | Average of the eight ICT values, ignoring blanks |
| 13 | `pct_cal` | Cumulative PCT converted to physical units |
| 14 | `distance` | The distance used for this round |
| 15 | — | Spacer |
| 16–31 | `alpha_<dir>`, `zfl_<dir>` | Field angle and focal length per direction — eight pairs |
| 32 | — | Spacer |
| 33 | `alpha_avg` | Average field angle |
| 34 | `zfl_avg` | Average focal length |

<div className="custom-note">
  <div className="custom-note-title">📌 ICT AVG IS COMPUTED, NEVER TYPED</div>
  <div>
    This is the column <a href="./pct-recommend.md">PCT Recommend</a> reads. If it is empty, run <strong>Update Table</strong> or <strong>Calculate Result</strong> on that round first — that is exactly the situation behind its <em>"Round 1 has no row with both PCT and ICT avg filled in"</em> message.
  </div>
</div>

### The eight directions are in two different orders

| Where | Order |
|---|---|
| **Table columns** | `n, s, w, e, nw, se, sw, ne` |
| **Averaging** | `n, w, s, e, nw, se, sw, ne` |

Both are correct for what they do — averaging is order-independent — but it
means a column index cannot be derived from a direction list without checking
which one you are holding.

---

## 3. Filling the table

### Update Table — from a fresh capture

Runs 8-direction node detection on `image_cali/capture_positive_shot.png` and
`capture_negative_shot.png`, using the four centre values from the main window.
It writes the PCT column from the Pattern Generator's 75 values, fills the eight
ICT columns, recomputes, and stars the tab.

It also **mirrors the four centres into that round's own display fields**, so
each round records the centres it was measured with.

### Load Excel — one round

Reads sheet `Sheet` — falling back to the first sheet — and maps **Excel rows 3
onwards into table rows 2 onwards**, columns A to K (0–10). Up to 75 layers.

Two behaviours worth knowing:

- **The table is cleared first**, then re-starred only if the file actually
  contained data.
- **An empty Side cell is skipped**, so a sheet without a Side column cannot
  blank out the layer indices.

After loading it runs a full recompute and redraws every plot.

Failure: *"No data read (is this a Calibration Result file?)"*

### Load All Excel — every round

Point it at a folder holding numbered subfolders `1` … `10`. For each, it reads
the **first `.xlsx`** it finds, locates the header row by looking for `pct` or
`(mm)` in it, and loads the rows after it.

All stale stars are cleared first, then set per round that actually loaded.
`main.json` in the folder is applied afterwards — see
[Reload Calibration Data](../reload-calibration-data.md).

### The folder tree

Double-clicking in the tree on the left:

| What you double-click | What happens |
|---|---|
| An `.xlsx` file | Loaded into the round guessed from its path |
| A folder | Loaded as a complete set, all rounds |

**The round is guessed** from the first standalone `1`–`10` in the filename, then
from any path component that is a bare number in that range. **Failing both, it
defaults to round 1.** A file named `results_2024.xlsx` will not land where you
expect.

---

## 4. Calculate Result

Each round tab has its own **Calculate Result** button
(`btn_calculate_result_<n>`). It computes that round only, then redraws the
ZFL-IH, IH-Alpha and Overlap plots.

**Update All Cali Result** in the header does the same for rounds 1–10 at once,
and additionally sets the global IH range from the data — Min `0`, Max the
largest ICT across all rounds.

<div className="custom-note">
  <div className="custom-note-title">📌 UPDATE ALL RETURNS INSTANTLY WHEN NOTHING IS LOADED</div>
  <div>
    With no raw ICT data anywhere, the full recompute is skipped entirely. That is why changing the calibration system on an empty window is instant rather than a freeze — it is not ignoring you.
  </div>
</div>

---

## 5. Aggr Round — find this round's best distance

Each round tab also has **Aggr Round** (`btn_aggr_round_<n>`).

It searches distances from **1 to 500** for the one that minimises this round's
aggregation, then:

1. writes the winner into `lineedit_distance_round_<n>`,
2. writes the aggregation into `lineedit_aggregation_round_<n>`,
3. **recomputes the round at that distance**, leaving the table on the best
   result,
4. adds the point to the Aggregation-vs-Distance plot.

A progress dialog runs during the search, and **Stop** cancels it. A cancelled
search leaves the fields untouched.

---

## 6. Editing by hand

The tables are fully editable spreadsheets:

| Key | Action |
|---|---|
| `Ctrl+C` / `Ctrl+X` | Copy / cut the selection as TSV |
| `Ctrl+V` | Paste TSV from the current cell |
| `Delete` | Blank the selection |
| `Ctrl+Z` | Undo — up to **200** edit groups |
| `Ctrl+Shift+Z` / `Ctrl+Y` | Redo |

A new edit clears the redo history. Paste writes cell by cell and **silently
stops at the table edge** rather than growing it.

> This is how you drop a single bad node: select its cell, press `Delete`, and
> re-run Calculate Result. ICT avg ignores blanks, so the row still averages
> correctly on what is left.

---

## 7. Save to Excel

Writes the **current** round: 77 rows — 2 header plus 75 layers — and columns 0
to 10, `round` through `ict_ne`.

<div className="custom-note custom-warning">
  <div className="custom-note-title">⚠️ THE SAVED FILE HOLDS ONLY THE MEASURED COLUMNS</div>
  <div>
    Columns 12 onwards — ICT avg, PCT_CAL, distance, the alpha and ZFL pairs — are <strong>not written</strong>. That is deliberate: they are derived, and reloading re-derives them. But it means the <code>.xlsx</code> is an input file, not a result report. Read the computed values off the Parameter tab, or export them yourself.
  </div>
</div>

`.xlsx` is appended if you leave it off, and missing directories are created.

<div className="custom-note custom-danger">
  <div className="custom-note-title">⚠️ SAVING NEEDS `zip`, LOADING NEEDS `unzip`</div>
  <div>
    Both are shelled out to. Without <code>unzip</code>, <em>Load All Excel</em> reports <strong>"No round subfolders (1..10) with .xlsx found"</strong> even when the files are plainly there — the folders are found, every file just reads back empty. Install both with <code>sudo apt install -y unzip zip</code>.
  </div>
</div>

---

## 8. Turning a round off

Right-click its tab → **Turn Off Round N**. The table greys out, the tab gains
` [OFF]`, and every computation from then on skips it.

The data stays. Turn it back on and it rejoins.

> This is the right way to drop a bad capture. Deleting the data loses your
> record of what happened; turning the round off keeps it and excludes it.

---

## Troubleshooting

| Symptom | Cause |
|---|---|
| ICT avg empty | The round has not been computed. Run **Calculate Result** |
| Column titles vanished | An older build's Clear Table. Reload the round from Excel |
| Excel loaded into the wrong round | The round guess found no `1`–`10` in the path and defaulted to 1 |
| *"No data read (is this a Calibration Result file?)"* | Wrong sheet layout, or `unzip` is missing |
| *"Capture images not found"* | No positive/negative shots in `image_cali/` |
| **Aggr Round** did nothing | It was cancelled with **Stop**, or no distance in 1–500 produced a finite aggregation |
| Paste dropped some cells | It stops at the table edge; the table does not grow |
| A round is ignored by every plot | It is turned off — check its tab for ` [OFF]` |
| Saved `.xlsx` has no computed columns | Expected. Only columns 0–10 are written |

---

## Screenshots this page needs

| Figure | File | What to capture |
|---|---|---|
| 1 | `cali-result-round-table.png` | A computed round table |

Three more that would carry this page:

| File | What to capture |
|---|---|
| `cali-result-table-header-band.png` | Rows 0 and 1 plus the Side column, cropped tight — makes the "no Qt header" point visible |
| `cali-result-table-empty-vs-filled.png` | The same round before and after Calculate Result |
| `cali-result-aggr-round-progress.png` | The distance-search progress dialog with **Stop** |

---

## Source material

| What | Where |
|---|---|
| Layout | `cpp/ui/cali_result.ui`, `tablewidget_1` … `tablewidget_10` |
| Behaviour | `cpp/src/controllers/controller_cali_result.cpp` |
| Column map, per-round maths | `cpp/src/core/cali/CaliCompute.cpp`, `CaliRound.cpp`, `CaliMath.cpp` |
| Excel I/O | `cpp/src/io/XlsxIO.cpp` |
