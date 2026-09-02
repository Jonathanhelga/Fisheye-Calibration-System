---
id: reload-calibration-data
slug: /calibration/reload-calibration-data
title: Reload Calibration Data
---

# Reload Calibration Data

Getting a previous calibration back into the Calibration Result window: from a
folder on disk, from a single file, from the tree, or straight out of cloud
storage.

<Shot
  id="fig-1"
  number="1"
  file="reload-loaded-folder.png"
  caption="A complete calibration reloaded — folder tree expanded, several rounds starred."
  what="Run Load All Excel on a folder with subfolders 1–10, then capture the top of the window so the folder path, the expanded tree and the starred tabs are all visible."
  height={430}
/>

---

## Four ways in

| Route | Loads | Use when |
|---|---|---|
| **Load All Excel** | Every round from a folder | The normal case — reopening a whole calibration |
| **Load Excel** | One file into the current round | Replacing or adding a single round |
| **The folder tree** | Either, by double-click | Browsing rather than dialog-hunting |
| **Load to System** | Downloads from SharePoint, then runs Load All Excel | The files are in cloud storage, not on your disk |

---

## 1. Expected folder layout

**Load All Excel** looks for numbered subfolders, one per round:

```
my_calibration/
├── main.json                      ← optional, restores system + spacing
├── 1/
│   ├── moil_cali_result.xlsx      ← the first .xlsx found is used
│   ├── capture_positive_shot.png
│   └── capture_negative_shot.png
├── 3/
├── 5/
├── 7/
└── 9/
```

Subfolders are looked for by the names `1` through `10`. Gaps are fine — an
Auto Calibrate run produces `1, 3, 5, 7, 9` and loads correctly.

### Where the folder is on each deployment

| Deployment | Path |
|---|---|
| Ubuntu build from source | Wherever you saved it |
| **Windows installer** | `\\wsl.localhost\MoilCali\root\MoilCali\` — from Explorer, or the Start Menu **MoilCali data folder** entry |
| Docker | Inside the container unless you mounted a volume |

---

## 2. Load All Excel

Pick the **parent** folder — the one containing `1`, `3`, `5` … — not a round
folder.

What it does, in order:

1. Writes the folder into the path field and roots the tree there.
2. **Clears every tab's star**, so nothing stale survives.
3. For each round 1–10, reads the **first `.xlsx`** in that subfolder, finds the
   header row by looking for `pct` or `(mm)` in it, and loads the rows after it
   into columns 0–10.
4. Stars each round that actually received data.
5. Applies `main.json` if present.
6. Recomputes everything and redraws every plot.

<div className="custom-note custom-danger">
  <div className="custom-note-title">⚠️ "No round subfolders (1..10) with .xlsx found" USUALLY MEANS `unzip` IS MISSING</div>
  <div>
    An <code>.xlsx</code> file <em>is</em> a zip archive, and the application shells out to <code>unzip</code> to read one. Without it the subfolders are found and every file reads back <strong>empty</strong> — which produces exactly this message, pointing at the wrong cause.
    <br /><br />
    <code>sudo apt install -y unzip zip</code> — <code>setup.sh</code> does not install them. The packaged Windows installer already contains both.
  </div>
</div>

Other reasons for the same message: you picked a round folder instead of its
parent, or the subfolders are named something other than `1`–`10`.

---

## 3. Load Excel — one round

Loads into **whichever tab is currently selected**, clamped to 0–10.

It reads sheet `Sheet`, falling back to the first sheet, and maps **Excel row 3
onward to table row 2 onward**, columns A–K.

Two behaviours worth knowing:

- The table is **cleared first**, then re-starred only if the file held data.
- An **empty Side cell is skipped**, so a sheet without a Side column cannot
  blank out the layer indices.

Failure: *"No data read (is this a Calibration Result file?)"* — wrong sheet
layout, or `unzip` missing again.

<div className="custom-note custom-warning">
  <div className="custom-note-title">⚠️ CHECK WHICH TAB IS SELECTED FIRST</div>
  <div>
    On the <code>parameter</code>, <code>Overlap</code> or <code>Graphs</code> tabs the index clamps to <strong>round 10</strong>, and the file is loaded there. There is no confirmation.
  </div>
</div>

---

## 4. Loading from the folder tree

| Double-click | Result |
|---|---|
| An `.xlsx` | Loaded into the round **guessed from its path** |
| A folder | Loaded as a complete set |

Typing a path into the field and pressing Enter does the same.

<div className="custom-note custom-danger">
  <div className="custom-note-title">⚠️ THE ROUND GUESS DEFAULTS TO 1</div>
  <div>
    It looks for a standalone <code>1</code>–<code>10</code> in the filename, then for a path component that is a bare number in that range. <strong>Finding neither, it uses round 1</strong> — so <code>results_final.xlsx</code> quietly overwrites round 1, and <code>scan_2024.xlsx</code> lands in round 2 because of the year. Prefer <strong>Load All Excel</strong> on the parent folder, where the round comes from the subfolder name.
  </div>
</div>

---

## 5. What main.json restores

If it sits in the folder, Load All Excel applies it after loading:

```json
{
  "systemType": "Yuanman - SIDE (EV2785)",
  "distance_per_round": 20
}
```

| Key | Restores |
|---|---|
| `systemType` | The **Select Cali System** combo box — and with it the rig's pixel sizes and h/v gaps. **Unrecognised values fall back to the first entry, silently** |
| `distance_per_round` | The distance-per-round field |

Write it with **Save Configuration** on the parameter tab, before you archive the
folder. Without it, a reloaded calibration comes back on whatever system happened
to be selected — and the numbers will be wrong in a way nothing flags.

`main.json` is **not** applied by Load Excel or by a single-file tree
double-click; only by a full folder load.

---

## 6. Load to System — straight from cloud storage

New in v2.0, and it closes what v1.1 documented as an unavoidable manual step.

1. **Load Database** in the header — you will be asked to sign in to Microsoft
   Graph first.
2. Find the camera or round.
3. **Right-click → Load to System.**
4. The database window closes and the folder downloads, then runs through Load
   All Excel automatically.

Full detail: [Load to System](../database/load-to-system.md).

### The manual route still works

It is still the right answer offline, or when a link cannot be resolved:

1. Find the record and note where its folder lives.
2. **Right-click → Open URL**, and download the folder in your browser.
3. **Load All Excel** on what you downloaded.

<div className="custom-note custom-warning">
  <div className="custom-note-title">⚠️ THE CALI FOLDER FIELD IS LOCAL PATHS ONLY</div>
  <div>
    Pasting an <code>http://</code> or <code>https://</code> address into the folder path field does not work — it reads folders on your own disk. Cloud links go through <strong>Load to System</strong>, not through this field.
  </div>
</div>

---

## 7. Verify what you loaded

After any reload, check in this order:

| Check | Where |
|---|---|
| The right rounds are starred | Tab strip |
| No round is unexpectedly ` [OFF]` | Tab strip |
| The system combo matches the rig used | Header |
| Distance per round is right | Parameter tab |
| ICT columns hold values | Any round tab |
| ICT avg is filled | Any round tab — if not, run **Update All Cali Result** |
| The plots have points | Parameter and Overlap tabs |
| The Overlap snake line is smooth | Overlap tab |

---

## 8. Saving so you can reload later

Before archiving a calibration:

1. **Save to Excel** on each round, into its numbered subfolder.
2. Copy `capture_positive_shot.png` and `capture_negative_shot.png` in beside it.
3. **Save Configuration** on the parameter tab → `main.json` in the parent folder.
4. **Save Parameters** for the camera JSON.
5. **Save Distance History** if you ran a distance search — that is the slowest
   step to repeat.

<div className="custom-note custom-warning">
  <div className="custom-note-title">⚠️ THE SAVED .xlsx HOLDS ONLY THE MEASURED COLUMNS</div>
  <div>
    Columns 0–10 — round, side, PCT and the eight ICT values. ICT avg, PCT_CAL, distance and the alpha/ZFL pairs are <strong>not written</strong>, because reloading re-derives them. It is an input file, not a result report.
  </div>
</div>

---

## Troubleshooting

| Symptom | Cause |
|---|---|
| *"No round subfolders (1..10) with .xlsx found"* | `unzip` missing; or you picked a round folder instead of its parent; or the subfolders are not named `1`–`10` |
| *"No data read (is this a Calibration Result file?)"* | Wrong sheet layout, or `unzip` missing |
| Loaded into the wrong round | The round guess defaulted to 1, or matched a number in the filename |
| Loaded into round 10 unexpectedly | A non-round tab was selected; the index clamps |
| ICT avg empty after loading | Run **Update All Cali Result** — it is computed, not stored |
| System combo came back wrong | `main.json` missing, or its `systemType` did not match an entry — it falls back to the first one silently |
| A pasted cloud link did nothing | The folder field is local paths only. Use **Load to System** |
| Layer indices blanked out | An older build. Reload the round; the Side column is skipped on load now |
| Save fails | `zip` is missing |

---

## Screenshots this page needs

| Figure | File | What to capture |
|---|---|---|
| 1 | `reload-loaded-folder.png` | Tree expanded, rounds starred after Load All Excel |

Three more worth having:

| File | What to capture |
|---|---|
| `reload-folder-layout.png` | An Explorer or file-manager view of a correct folder — parent with `1`, `3`, `5`, `7`, `9` and `main.json` |
| `reload-no-subfolders-error.png` | The *"No round subfolders (1..10) with .xlsx found"* dialog |
| `reload-tabs-starred.png` | The tab strip after a successful load, several stars visible |

---

## Source material

| What | Where |
|---|---|
| Loading behaviour | `cpp/src/controllers/controller_cali_result.cpp` — `loadExcel`, `loadAllExcelFromFolder`, `loadMainJson`, `guessRoundFromPath` |
| Excel I/O | `cpp/src/io/XlsxIO.cpp` |
| SharePoint download | `cpp/src/io/SharePointSync.cpp` |
| Bundled database | `cpp/database/cali_system_v2.db` |
