---
id: overlap-and-aggregation-view
slug: /calibration/cali-result/overlap-and-aggregation-view
title: Overlap and Aggregation View
---

# Overlap and Aggregation View

Two tabs, documented together because they answer one question:

> **At what distance is this camera best characterised?**

| Tab | Shows |
|---|---|
| `Overlap` | How well the rounds agree with each other |
| `Aggr by Distance and Range` | The distance search, per round and per IH range |

---

## 1. The Overlap tab

**X axis:** `ICT (pixel)`, −2500 … 2500 · **Y axis:** `ZFL (pixel)`, −600 … 3000

<Shot
  id="fig-1"
  number="1"
  file="cali-result-overlap-tab.png"
  caption="Overlap: every enabled round's scatter plus the white snake line."
  what="Load several rounds, run Update All Cali Result, then capture the Overlap tab so the per-round colours and the white connecting line are both clear."
  height={430}
/>

The same ICT/ZFL points as the ZFL-IH graph, drawn one colour per round, plus a
**white "snake" line** connecting every point from every enabled round, sorted by
absolute ICT.

**Update Overlap** (`btn_update_overlap`) redraws it.

### How to read it

That snake line is the whole point:

| What it looks like | What it means |
|---|---|
| A **smooth curve** | The rounds agree. Their ZFL values line up into one continuous relationship — the distances are right |
| A **zig-zag** | The rounds disagree. Consecutive points from different rounds sit at different heights, so the line jumps back and forth |
| One round sitting clearly above or below | That round's distance is wrong, or its capture is bad |

> Because points are sorted by `|ICT|` and not by round, neighbouring points in
> the line normally come from *different* rounds. A smooth line is therefore
> direct evidence that the rounds overlap correctly — which is what the tab is
> named after.

Fix a zig-zag either by searching that round's distance again
([§2](#2-aggr-by-distance-and-range)), or by turning the round off.

---

## 2. Aggr by Distance and Range

The distance search. **Aggregation** measures how tightly the rounds agree — so
the best distance is the one that minimises it.

<Shot
  id="fig-2"
  number="2"
  file="cali-result-aggr-tab.png"
  caption="The Aggr by Distance and Range tab with a search completed."
  what="Run Min Aggregation by interval on a loaded calibration, then capture the whole tab — the per-round distance/aggregation fields, the 20-range table, and the Aggregation vs. Distance plot."
  height={540}
/>

### 2.1 Per-round search

| Control | Object name | What it does |
|---|---|---|
| **Aggr Round** (per tab) | `btn_aggr_round_<n>` | Searches **1–500** for the distance minimising that round's aggregation |
| **Min Aggregation by interval** | `btn_min_aggregation_by_interval` | Runs that search for **every round that has data** |
| **Stop** | `btn_stop` | Cancels the running search |

Each search writes `lineedit_distance_round_<n>` and
`lineedit_aggregation_round_<n>`, recomputes the round at the winning distance,
and adds a point to the Aggregation-vs-Distance plot.

<div className="custom-note">
  <div className="custom-note-title">📌 MIN AGGREGATION BY INTERVAL SKIPS TWO KINDS OF ROUND</div>
  <div>
    Rounds that are <strong>turned off</strong>, and rounds with <strong>no ICT data at all</strong> in columns 3–10. Both are skipped without comment, so a round whose distance field stays empty afterwards is one of those two — not a failure.
  </div>
</div>

A search cancelled with **Stop** leaves the fields as they were.

### 2.2 The 20 IH ranges

Twenty rows, each with a min %, a max %, a distance and an enable checkbox:

| Widget | Object name |
|---|---|
| Min % | `lineedit_aggregation_min_range_<i>` |
| Max % | `lineedit_aggregation_max_range_<i>` |
| Distance | `lineedit_distance_range_<i>` |
| Enable | `checkbox_enable_range_<i>` |

**The percentages are of the largest ICT across all rounds**, not of the sensor.
Range 0 is the global one, filled automatically by Update All Cali Result as
`0` to the maximum ICT.

Each range also gets a shaded band on the
[ZFL-IH graph](./parameter-view.md#the-coloured-range-bands) — and, importantly,
**every range with valid numbers is shaded, whether or not it is ticked.**

Right-clicking a range label offers **Show ZFL-IH for this range** as a pop-up.

### 2.3 Range Window — fill all twenty at once

**Range Window** (`btn_range_window`) loads a JSON and fills all 20 min/max
pairs from four numbers:

```json
{
  "range_min": 0,
  "range_max": 100,
  "step": 5,
  "window": 20
}
```

Range `i` gets `min = range_min + (i-1) × step` and
`max = min(min + window, range_max)`. So the example gives 0–20, 5–25, 10–30, …
— **overlapping** windows sliding across the field.

All four keys are required and finite, with `step > 0`, `window > 0` and
`range_max >= range_min`. Anything else is refused:

> *"Required finite keys: range_min, range_max, step>0, window>0,
> range_max>=range_min."*

<div className="custom-note custom-warning">
  <div className="custom-note-title">⚠️ RANGE WINDOW DOES NOT TOUCH THE CHECKBOXES</div>
  <div>
    It fills the min/max fields only. Which ranges are <em>enabled</em>, and the distances beside them, are left exactly as they were — so loading a new window over an old setup leaves stale enables and stale distances in place.
  </div>
</div>

### 2.4 Aggr by Range and Distance

`btn_aggr_by_range_and_distance` works in **two directions**, decided by which
field you filled:

| You filled | It computes |
|---|---|
| **Distance** (`lineedit_distance_aggr`) | The aggregation at that distance |
| **Aggregation** (`lineedit_aggr`) and left distance empty | Searches **1–500** for the distance that gets closest to your target |

Both need a valid IH window in `lineedit_min_aggr` and `lineedit_max_aggr` —
**0 to 100, min less than max**:

> *"Fill valid Min% and Max% (0..100, min&lt;max)."*

With neither field filled:

> *"Provide a Distance (to get Aggregation) or an Aggregation (to search
> Distance)."*

**The search is coarse-then-fine:** 201 samples across 1–500, then 81 more in a
narrow band around the best hit. Every sample is plotted, so the
Aggregation-vs-Distance curve after a target search shows the whole sweep, not
just the answer.

### 2.5 Aggregation vs. Distance plot

**X:** `Distance (pixel)`, 0–400 · **Y:** `Aggregation (pixel)`, 1300–1800

**Update Dist vs. Aggr** (`btn_update_dist_vs_aggr`) redraws it from whatever
samples the last search collected.

| Where the points came from | What you see |
|---|---|
| **Aggr Round**, run repeatedly | One point per round — the per-round optima |
| **Min Aggregation by interval** | One point per round, all at once |
| **Aggr by Range and Distance** target search | The full 282-sample sweep, showing the shape of the minimum |
| Nothing yet | Title reads `Aggregation vs. Distance (No Data)` |

> The sweep is the more informative of the two. A sharp minimum means the
> distance is well determined; a flat basin means the data cannot distinguish
> distances in that region, and the exact number matters less than it appears to.

### 2.6 History Distance

`btn_history_distance` is a **radio button used as a mode switch**, and it can be
clicked off again.

| State | Enabling a range does |
|---|---|
| **Off** (default) | **Searches** for that range's best distance |
| **On** | **Uses** the distance already in that range's field |

Switching it **on** asks for a folder of `history_best_distance_range_*.json`
files and fills the distance fields from them, then reports:

> *"History mode ON. Filled the distance for N range(s). Enable a range
> (checkbox) to calculate using its distance."*

Cancelling the folder dialog leaves the mode **on** with no new distances — which
means enabling a range then uses whatever was already in the field.

**Save Distance History** (`btn_save_history_distance`) writes the other
direction: one file per **enabled** range, into a folder you choose.

```json
{
  "range_index": 3,
  "ih_min": 10,
  "ih_max": 30,
  "distance": 187.5
}
```

Files are named `history_best_distance_range_03.json` — zero-padded to two
digits. Ranges that are unticked, or whose fields do not parse, are skipped
silently, and the count you get back is of files actually written.

> This is how you reuse a slow search. Run it once, save the history, and every
> later session on the same camera loads the distances instead of searching.

---

## Recommended workflow

1. Load every round and run **Update All Cali Result**.
2. Open **Overlap**. If the snake line zig-zags, fix or disable the offending
   round before going further.
3. Go to **Aggr by Distance and Range** and run **Min Aggregation by interval**.
4. Check **Aggregation vs. Distance** — the per-round optima should cluster.
5. Load a **Range Window** JSON, or fill the ranges by hand, and tick the ones
   you care about.
6. Use **Aggr by Range and Distance** with a target to see the shape of the
   minimum in a specific IH window.
7. **Save Distance History** so the next session skips the search.
8. Return to **Overlap** and confirm the snake line is now smooth.

---

## Troubleshooting

| Symptom | Cause |
|---|---|
| Overlap snake line zig-zags | Rounds disagree. Re-search distances, or turn a round off |
| A round has no distance after **Min Aggregation by interval** | It is turned off, or it has no ICT data |
| Plot title `(No Data)` | No search has run this session — the samples are not persisted |
| *"Fill valid Min% and Max% (0..100, min&lt;max)."* | The IH window fields are empty or out of order |
| *"No IH data in rounds."* | Nothing computed yet. Run **Update All Cali Result** |
| Range Window refused the file | One of the four keys is missing, non-finite, or violates the constraints |
| Range Window left stale enables and distances | By design — it fills min/max only |
| History mode on but nothing changed | The folder dialog was cancelled, or the files did not parse |
| **Save Distance History** saved fewer files than expected | Unticked ranges and unparseable fields are skipped without comment |
| Search takes a long time | It is sampling up to 282 distances and recomputing each. **Stop** cancels |

---

## Screenshots this page needs

| Figure | File | What to capture |
|---|---|---|
| 1 | `cali-result-overlap-tab.png` | Overlap tab with the snake line |
| 2 | `cali-result-aggr-tab.png` | Aggr tab after a completed search |

Three more that would carry this page:

| File | What to capture |
|---|---|
| `cali-result-overlap-good-vs-bad.png` | A smooth snake line beside a zig-zag one — the single most useful capture here |
| `cali-result-dist-vs-aggr-sweep.png` | The 282-sample sweep from a target search, showing the minimum |
| `cali-result-range-table.png` | The 20-range table with several ranges filled and ticked |

---

## Source material

| What | Where |
|---|---|
| Layout | `cpp/ui/cali_result.ui`, `Overlap` and `Aggr by Distance and Range` tabs |
| Behaviour | `cpp/src/controllers/controller_cali_result.cpp` |
| Aggregation and search | `cpp/src/core/cali/CaliCompute.cpp` |
