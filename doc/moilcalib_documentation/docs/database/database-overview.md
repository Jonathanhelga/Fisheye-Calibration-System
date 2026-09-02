---
id: database-overview
slug: /database/database-overview
title: Database Overview
---

# Database Overview

The **Calibration Data** window: browse and search every calibration record the
system knows about, drill into a camera's rounds and files, and pull a stored
calibration back into the application.

Opened from the Calibration Result window, or by the `database` HELP key.

<Shot
  id="fig-1"
  number="1"
  file="database-camera-list.png"
  caption="Calibration Data window, camera list view."
  what="Open the window against a populated database. Capture the whole window — Search Information panel on the left, camera list on the right with several rows visible."
  height={520}
/>

The window opens at **1450 × 820**.

---

## What the database is, and what it is not

**Think of it as a library catalogue, not the library.**

A record tells you that a calibration exists, when it was made, which camera it
belongs to, and where its files live. **The measured values themselves are not in
the database** — they live in Excel files in cloud storage.

That single fact explains the whole window: the table is a finding aid, and
[Load to System](./load-to-system.md) is the step that actually fetches the data.

<div className="custom-note">
  <div className="custom-note-title">📌 THIS WORKS DIFFERENTLY FROM VERSION 1.1</div>
  <div>
    In v1.1 this was a dead end — you could browse records but never load one, and the documented workaround was to download the folder by hand. Version 2.0 implements the download over the Microsoft Graph API, so <strong>Load to System</strong> now fills the round tables for you.
  </div>
</div>

---

## 1. Opening a database

The application ships with `cali_system_v2.db`, so **Load Database** in the
Calibration Result window resolves without asking you for a file. That is a v2.0
change — v1.1 popped a file dialog because nothing was bundled.

The file is plain SQLite, and everything this window reads is local. The only
genuinely remote step is Load to System.

---

## 2. Search Information

Ten fields down the left, one per column. **Filtering is live** — the list
narrows as you type, with no button to press.

| Field | Object name | Placeholder | Matching |
|---|---|---|---|
| Brand | `lineedit_brand` | Search brand | Text, contains |
| Name | `lineedit_name` | Search name | Text, contains |
| Number | `lineedit_number` | Search number | Text, contains |
| FoV | `lineedit_fov` | **FoV or use `>`, `<`, `>=`, `<=`** | Numeric |
| Resolution | `lineedit_resolution` | Search resolution | Text, contains |
| Calibration System | `lineedit_calisys` | Search calibration system | Text, contains |
| Calibrator | `lineedit_calibrator` | Search calibrator | Text, contains |
| Calibration Date | `lineedit_calidate` | Search date | Text, contains |
| ICX | `lineedit_icx` | **ICX or use `>`, `<`, `>=`, `<=`** | Numeric |
| ICY | `lineedit_icy` | **ICY or use `>`, `<`, `>=`, `<=`** | Numeric |

**Clear** (`btn_clear_search`) empties all ten at once.

### Numeric filters

Three fields — **FoV**, **ICX**, **ICY** — accept a comparison operator as well
as a plain value:

```
220        exactly 220
>200       greater than 200
<=1000     1000 or less
```

<div className="custom-note custom-warning">
  <div className="custom-note-title">⚠️ A NULL COLUMN FAILS EVERY NUMERIC FILTER</div>
  <div>
    A record with no FoV recorded shows a <strong>blank cell</strong>, and typing anything at all into the FoV box makes it disappear from the list — including <code>&gt;0</code>. If a camera you know exists vanishes when you filter, that column is empty for it, not wrong.
  </div>
</div>

### Why typing is instant

Every `camera_main` row is read into memory once when the database opens.
Filtering then runs locally, so no keystroke re-queries the file.

Results are sorted by **brand, then name, then number**. When nothing matches,
the table shows a single **`(no result)`** row spanning its full width.

---

## 3. The camera list

Eleven columns:

| No | Brand | Name | Number | FoV | Resolution | Calibration System | Calibrator | Calibration Date | ICX | ICY |
|---|---|---|---|---|---|---|---|---|---|---|

Rows select whole, one at a time, and are **not editable**. Sorting by clicking a
header is disabled — use the search fields instead.

**Double-click a camera** to open its rounds.

---

## 4. The rounds view

<Shot
  id="fig-2"
  number="2"
  file="database-rounds-view.png"
  caption="Rounds view for one camera, with the Back button enabled."
  what="Double-click a camera that has rounds recorded. Capture the window showing the four-column rounds table and the now-enabled Back button."
  height={470}
/>

The same table switches to four columns:

| No | Type | Name / Round | Info |
|---|---|---|---|

It lists every `camera_round` row for that camera, followed by the camera's own
global files from `camera_main`.

**Back** (`btn_back`) returns to the camera list. It is **disabled until you open
a camera**, which is the only clue the window gives that it has two views at all.

The search fields keep their contents while you are in here but do not filter
this view — the rounds table has its own contents.

### Twelve file kinds per round

Each round can reference up to twelve files:

| | | |
|---|---|---|
| `excel_file_id` | `capture_negative_file_id` | `capture_positive_file_id` |
| `main_image_file_id` | `ictpctcut_excel_file_id` | `ictpct_png_file_id` |
| `ictpct_cut_png_file_id` | `round_main_png_file_id` | `session_bmp_file_id` |
| `session_dat_file_id` | `session_i_bmp_file_id` | `session_profile_txt_file_id` |

Most rounds have only some of them. **Double-clicking a round** offers the ones
that exist.

---

## 5. The context menu

**Right-click any row** for three actions.

<Shot
  id="fig-3"
  number="3"
  file="database-context-menu.png"
  caption="The right-click menu on a camera row."
  what="Right-click a camera row and capture the open menu — Load to System, Open URL, separator, SharePoint Credentials…"
  height={230}
/>

| Action | What it does |
|---|---|
| **Load to System** | Resolves the row to a share URL and downloads it into the Calibration Result window — see [Load to System](./load-to-system.md) |
| **Open URL** | Opens the resolved link in your browser instead. Downloads nothing |
| **SharePoint Credentials…** | Opens the sign-in dialog |

<div className="custom-note">
  <div className="custom-note-title">📌 THE CREDENTIALS ENTRY IS THE ONLY WAY IN WITHOUT FAILING FIRST</div>
  <div>
    Every other route to the sign-in dialog requires a download to fail. This entry is how you change or delete stored credentials deliberately — which is what someone leaving a shared machine needs.
    <br /><br />
    If you use it to <strong>forget</strong> the credentials, <strong>this window closes itself</strong>. That is intentional: the sign-in gate would no longer let anyone back in, so leaving it open would be misleading.
  </div>
</div>

### What each row resolves to

| Where you are | Action | Resolves to |
|---|---|---|
| Camera list | Load to System | That camera's share URL |
| Camera list | Open URL | You pick one of the camera's global files |
| Rounds view, a round | either | You pick one of that round's files |
| Rounds view, a file | either | That file directly |

Failures are specific: *"This camera_main does not have a usable URL."*,
*"Selected file has no web_url."*, *"This file does not have web_url."*

---

## 6. Double-click, by view

| View | Double-click does |
|---|---|
| Camera list | Opens that camera's rounds |
| Rounds view, a round row | Opens one of that round's remote files |
| Rounds view, a file row | Opens that file |

---

## 7. Standalone vs attached

This window is normally opened from the Calibration Result window, and hands data
back to it. Opened on its own, **Load to System** has nowhere to load into and
says so:

> *"No Calibration Result window is attached to load into. Resolved URL: …
> Open it in the browser instead?"*

That is an honest fallback, not an error — the URL is still correct.

---

## Troubleshooting

| Symptom | Cause |
|---|---|
| `(no result)` although records exist | A search field still has text in it. Click **Clear** |
| A camera vanishes when you filter FoV/ICX/ICY | That column is `NULL` for it, and NULL fails every numeric comparison |
| **Back** is greyed out | You are in the camera list. It only enables once a camera is open |
| Clicking a column header does nothing | Sorting is disabled by design |
| Cells will not edit | Read-only by design |
| The window closed by itself | You chose **Forget Saved** in the credentials dialog |
| Load to System offers a browser instead of loading | The window was opened standalone |
| *"…does not have a usable URL"* | The record has no `web_url` — a catalogue gap, not a fault here |

---

## Screenshots this page needs

| Figure | File | What to capture |
|---|---|---|
| 1 | `database-camera-list.png` | Whole window, camera list, several rows |
| 2 | `database-rounds-view.png` | Rounds view with Back enabled |
| 3 | `database-context-menu.png` | Right-click menu open |

Worth adding: the Search Information panel with a numeric filter typed in
(`>200` in FoV) and the list visibly narrowed — it makes the operator syntax
obvious at a glance.

---

## Source material

| What | Where |
|---|---|
| Layout | `cpp/ui/db.ui` |
| Behaviour | `cpp/src/controllers/controller_db_cali_result.{h,cpp}` |
| Bundled database | `cpp/database/cali_system_v2.db` |
| Python original this matches | `mvc_controller/controller_db_cali_result.py`, `mvc_view/ui_db.ui` |
