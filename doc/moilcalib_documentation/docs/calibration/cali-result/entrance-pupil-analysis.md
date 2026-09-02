---
id: entrance-pupil-analysis
slug: /calibration/cali-result/entrance-pupil-analysis
title: Entrance-Pupil Shift — Graph Analysis Method
---

# Entrance-Pupil Shift — Graph Analysis Method

The `Graphs` tab. Three plots that show whether — and by how much — the lens's
entrance pupil moves as the field angle grows.

<Shot
  id="fig-1"
  number="1"
  file="cali-result-graphs-tab.png"
  caption="The Graphs tab with all three plots drawn."
  what="Complete a distance search with several ranges enabled, then click all three Update buttons and capture the whole tab so the ray fan, the Dist-vs-IH-Range curve and the Dist-vs-Alpha curve are all filled."
  height={540}
/>

---

## 1. Why the entrance pupil moves

An ideal camera has a single point through which every ray passes. A real
wide-angle fisheye does not: **the effective viewpoint slides along the optical
axis as the field angle increases.**

That is why this window searches for a *distance* per round and per range at all.
If the entrance pupil were fixed, one distance would fit the whole field. It is
not, so different parts of the field are best described by different distances —
and the pattern of how that distance varies with angle **is** the entrance-pupil
shift.

**Information — Entrance Pupil** (`btn_graphs_info`) opens the theory in the
application: a rendering of Fig. 2 from Gennery (2006), and how these three plots
relate to it.

<div className="custom-note">
  <div className="custom-note-title">📌 THIS TAB READS THE RANGE TABLE, NOT THE ROUNDS</div>
  <div>
    All three plots are built from the <strong>enabled ranges</strong> on the <a href="./overlap-and-aggregation-view.md">Aggr by Distance and Range</a> tab — their distances, IH bounds and alpha bounds. They show nothing until that search has been run and ranges are ticked.
  </div>
</div>

---

## 2. Shift of Entrance Pupil

**Update Shift of Entrance Pupil** (`btn_show_curve_2`)

**X:** lateral displacement, −300 … 300 · **Y:** optical axis (distance), 0 … 600

This is the diagram, not a data plot. For each enabled range it draws **one ray**:

- starting at `(0, distance)` — the range's distance on the optical axis,
- at an angle of `(alpha_min + alpha_max) / 2` from that axis,
- **300 units long**,
- coloured by the range's index, cycling through the ten round colours,
- with a **white dot marking its origin**.

### How to read it

**The white dots are the answer.** Each one is where that band of the field
"sees from".

| What the dots do | What it means |
|---|---|
| Cluster at one height | The entrance pupil barely moves. The lens behaves close to a single-viewpoint camera |
| **Spread along the Y axis** | The entrance pupil shifts. The spread *is* the magnitude of the shift |
| Rise or fall monotonically with the ray angle | The normal, well-behaved case — the shift is a smooth function of field angle |
| Scatter with no order | Suspect the data, not the lens. Ranges whose distance search did not converge land anywhere |

*"No valid enabled range data."* means no range is both ticked and holding
parseable distance and alpha bounds.

---

## 3. Distance vs IH Range

**Update Dist vs IH Range** (`btn_show_graph_dist_ih_range`)

**X:** IH range mean (%), 0 … 100 · **Y:** distance, 0 … 400

One point per enabled range, at `x = (ih_min + ih_max) / 2` and
`y = that range's distance`. Points are **sorted by X** and joined into a curve,
drawn in round-1 red with white dots.

### How to read it

The X axis is a percentage of the largest ICT, so this is *distance against
position across the image*, from centre to edge.

| Shape | Meaning |
|---|---|
| **Flat** | One distance fits everywhere. Nothing to correct |
| **Monotonic rise or fall** | The expected entrance-pupil behaviour — the viewpoint slides steadily outward or inward |
| **Kinked or non-monotonic** | A range's search did not converge, or its band is too narrow to constrain a distance |

---

## 4. Distance vs Alpha

**Update Dist vs Alpha** (`btn_show_graph_dist_alpha`)

**X:** alpha mean (degree), 0 … 90 · **Y:** distance, 0 … 400

The same construction, but against **field angle** instead of image position,
using `lineedit_alpha_min_<i>` and `lineedit_alpha_max_<i>`. Drawn in round-2
yellow with white dots.

### Why both this and §3

They are the same relationship in two coordinate systems, and the mapping between
them is exactly the IH-Alpha curve this calibration produces.

> **This is the physically meaningful one.** Entrance-pupil shift is a function of
> *ray angle*, not of where a pixel happens to land. §3 is easier to relate back
> to the image; §4 is the one to quote.

*"No valid enabled alpha data."* means the alpha bounds are missing or
unparseable, even where the IH bounds are fine — the two are separate fields, and
§3 can work while §4 does not.

---

## 5. Practical procedure

1. Load every round and run **Update All Cali Result**.
2. Load a **Range Window** JSON — overlapping windows across the field give a
   dense, evenly spaced sample.
3. Run the distance search so each range has a distance.
4. **Tick the ranges you trust.** Ranges whose search did not converge belong
   unticked; they will otherwise distort all three plots.
5. Click all three Update buttons.
6. Read **Distance vs Alpha** first. A smooth monotonic curve means a
   well-characterised lens.
7. Use **Shift of Entrance Pupil** to see the magnitude: how far apart the white
   dots sit along the axis.
8. **Save Distance History** so the search does not have to run again.

---

## 6. What this tab does and does not give you

| | |
|---|---|
| ✅ Three visual diagnostics from the distance search | |
| ✅ Ray geometry showing the shift's magnitude and direction | |
| ✅ The theory dialog, with the source it comes from | |
| ❌ **No numeric estimator** | There is no field reporting the shift as a number, no extrapolated `d₀`, no RMS |

<div className="custom-note custom-warning">
  <div className="custom-note-title">⚠️ THE OUTPUT OF THIS TAB IS A JUDGEMENT, NOT A NUMBER</div>
  <div>
    Everything here is read off the plots by eye. If you need the shift as a figure to quote, take the range distances out of the Aggr tab and compute it yourself — the application will not do it for you, and no field on this tab is holding it.
  </div>
</div>

---

## Troubleshooting

| Symptom | Cause |
|---|---|
| *"No valid enabled range data."* | No range is both ticked and holding a parseable distance + alpha bounds |
| *"No valid enabled alpha data."* | The alpha min/max fields are empty. They are separate from the IH min/max |
| All three plots empty | The distance search has not run, or no range is ticked |
| Ray dots scattered with no pattern | Ranges whose search did not converge. Untick them and redraw |
| A curve kinks sharply | One range's distance is an outlier — check that range's band width |
| Ranges shaded on ZFL-IH but absent here | Shading draws every valid range; these plots use only the **ticked** ones |

---

## Screenshots this page needs

| Figure | File | What to capture |
|---|---|---|
| 1 | `cali-result-graphs-tab.png` | The whole Graphs tab, all three plots drawn |

Four more that would carry this page:

| File | What to capture |
|---|---|
| `graphs-shift-pupil.png` | The ray fan cropped tight, white origin dots clearly spread |
| `graphs-dist-vs-alpha.png` | Distance vs Alpha showing a smooth monotonic curve |
| `graphs-dist-vs-ih-range.png` | Distance vs IH Range for comparison |
| `graphs-info-dialog.png` | The **Information — Entrance Pupil** dialog with the Gennery figure |

---

## Source material

| What | Where |
|---|---|
| Layout | `cpp/ui/cali_result.ui`, `Graphs` tab |
| Behaviour | `cpp/src/controllers/controller_cali_result.cpp` — `showCurve2`, `showGraphDistIhRange`, `showGraphDistAlpha`, `showGraphsInfo` |
| Round colours | `CaliCompute::roundColor` |
| Theory | Gennery (2006), Fig. 2 — quoted in the application's info dialog |
