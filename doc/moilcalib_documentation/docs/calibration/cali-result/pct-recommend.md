---
id: pct-recommend
slug: /calibration/cali-result/pct-recommend
title: PCT Recommend
---

# PCT Recommend

**New in version 2.0.**

Paste the (PCT, ICT) rows you measured, and this window works out a **fresh PCT
series whose ICT steps are as even as integer PCT values allow**.

Opened from **Calibration Result → Recommend PCT** on the main window.

<Shot
  id="fig-1"
  number="1"
  file="pct-recommend-overview.png"
  caption="The PCT Recommendation window with the built-in sample data."
  what="Open the window without changing anything — it ships with sample rows, so all three panels are populated on first open. Capture the whole window."
  height={600}
/>

---

## 1. Why this window exists

**One PCT unit does not move the intersection radius by a fixed number of
pixels.** How far it moves depends on where you already are on the image.

Call that local rate the **gain** — how many pixels of radius one PCT unit buys
you, at the radius you are currently at:

```
gain(r) = dICT / dPCT
```

Because gain varies across the field, a PCT series picked by eye — 40, 40, 35,
35, 35… — produces **uneven** ICT steps. Uneven steps waste rounds: some parts of
the field get sampled densely and others barely at all.

This window measures your gain curve from a handful of rows, then works
backwards: given a target step, what integer PCT gets you there from where you
currently are?

### How it computes the answer

1. **Fit the gain curve.** Each measured row gives one gain sample
   `Δ ICT / PCT`, taken at that row's midpoint radius. A cubic is fitted through
   those samples.
2. **Integrate.** The radius reached by spending `pct` units from radius `r` is
   the solution of `dr/dp = gain(r)`, integrated numerically with fixed-step RK4.
3. **Choose each row.** Bisect for the continuous PCT that would land exactly on
   the next evenly spaced target, then take whichever **neighbouring integer**
   ends closer.

The series always starts from **ICT = 0**, the optical centre. There is no field
for a different start — one has never been needed.

---

## 2. Panel 1 — Reference data

<Shot
  id="fig-2"
  number="2"
  file="pct-recommend-reference.png"
  caption="Reference data panel with the fitted-curve message below it."
  what="Crop to panel 1 — the two PCT and ICT text boxes, the Load round 1 button, and the green status line under them."
  height={420}
/>

Two text boxes, `PCT` and `ICT`, one value per row.

**The parser is forgiving.** Values may be separated by whitespace, newlines,
commas or semicolons, so a pasted column, a comma list or a spreadsheet copy all
work. A comma inside a number is read as a decimal point.

### Load round 1 from Calibration Result

Reads the PCT and ICT-average columns straight out of round 1 in the
[Calibration Result](./index.md) window, so you do not copy anything by hand.

Two things it will tell you instead of loading:

| Message | Meaning |
|---|---|
| *"This window is not attached to a Calibration Result window…"* | Opened standalone, with no Cali Result window behind it |
| *"Round 1 has no row with both PCT and ICT avg filled in."* | **ICT avg is computed, not typed.** Run **Update Table** or **Calculate Result** on round 1 first |

### What it checks before fitting

Every one of these stops the fit and shows a red message:

| Check | Message |
|---|---|
| Either column empty | *Both columns must be filled in.* |
| Different lengths | *The columns have different lengths: PCT has N rows, ICT has M.* |
| A PCT unreadable or ≤ 0 | *A PCT value is unreadable or not greater than zero.* |
| An ICT unreadable | *An ICT value is unreadable.* |
| Fewer than 4 rows | *At least 4 rows are needed to fit the curve.* |
| ICT not increasing | *An ICT value does not increase from the row before it. Check the order.* |

### Six rows is the threshold that matters

| Rows | What is fitted | Message |
|---|---|---|
| **6 or more** | A **cubic of its own**, shaped entirely by your data | *"Curve fitted from your N rows."* |
| **4 or 5** | The **built-in curve**, rescaled by a single multiplier — only the scale is trustworthy from that little data | *"N rows are not enough to shape a curve, so the built-in curve is used with a multiplier of k."* |

<div className="custom-note custom-warning">
  <div className="custom-note-title">⚠️ FOUR OR FIVE ROWS GIVES YOU A BORROWED CURVE SHAPE</div>
  <div>
    The recommendation is only as good as that assumption. If your lens does not behave like the built-in curve, the steps will not come out even no matter what the verdict says. Measure six rows or more.
  </div>
</div>

---

## 3. Steps in your reference data

The lower-right table, showing what your measured rows actually did.

| Column | Meaning |
|---|---|
| **no** | Row number |
| **PCT** | As you entered it |
| **ICT** | As you entered it |
| **step** | How far the radius advanced on this row |
| **gain** | `step / PCT` — the local rate this row implies |

**Rows whose step strays from the average are shown in red.** The threshold is
`max(2 px, 6 % of the mean)`.

Beneath it: *"Steps run L to H, average M, spread S px."* Green when the spread
is 3 px or less, amber above that, with *"Red rows stray far from the average."*
appended.

> This table is diagnosing your **input**, not the recommendation. A red-heavy
> table is exactly the situation this window exists to fix.

---

## 4. Gain model

<Shot
  id="fig-3"
  number="3"
  file="pct-recommend-gain-model.png"
  caption="Gain model panel: the fitted polynomial, its values across the field, and the validation line."
  what="Crop to the Gain model group box so the formula, the statistics line and the validation line are all legible."
  height={230}
/>

Three lines.

**The formula** — the fitted polynomial written out:

```
gain(r) = 0.02481 + 1.23456e-05*r - 4.56789e-09*r^2 + ...
```

**The statistics** — gain sampled at three points across the field
(`0`, half the measured tip, the tip), then how the fitted curve compares with
the built-in one as a single factor, plus its spread. If that spread is under
`0.01` you get *", constant across the range"* — meaning your lens has the same
curve *shape* as the built-in one and differs only in scale. Otherwise
*", the curve shape differs"*.

**The validation** — the honest check. Your own PCT column is replayed through
the fitted curve from 0. If the model is right, it should land back on your last
measured ICT:

> *"Validation: replaying your PCT column from 0.00 px ends at 596.21 px against
> a measured 596.55 px — off by 0.34 px after 16 steps, worst error along the way
> 0.62 px."*

**Green when the worst error along the way is under 1 px, amber otherwise.**

<div className="custom-note">
  <div className="custom-note-title">📌 READ THE VALIDATION LINE BEFORE THE RECOMMENDATION</div>
  <div>
    A model that cannot reproduce your own measurements will not predict new ones either. An amber validation line means the recommended series is a guess, whatever the verdict below it says.
  </div>
</div>

---

## 5. Panel 2 — Target

| Control | Object name | What it does |
|---|---|---|
| **Step to aim for (px)** | `spin_target` | The ICT advance you want per row |
| **Number of rows** | `spin_rows` | How many rows to generate |
| **Allowed spread (px)** | `spin_tol` | How uneven the result may be before the verdict turns amber |
| **Pick the best step** | `btn_auto` | Searches for the most even step — see below |
| **Copy table** | `btn_copy` | Copies the result to the clipboard |

**Rows and target are seeded from your data, once.** On the first successful fit,
*Number of rows* is set to your row count and *Step to aim for* to the rounded
average step. After that they are yours — re-fitting will not overwrite them
again.

Changing target, rows or tolerance rebuilds the series immediately. Editing the
reference data re-fits the curve first.

### Pick the best step

Scans candidate steps from **10 px to 80 px in 0.5 px increments**, builds the
series for each at your current row count, and keeps whichever gives the
**smallest spread** between its largest and smallest step. It then writes that
into *Step to aim for*, which rebuilds the series and reports the outcome.

If nothing in that range works: *"No usable step between 10 and 80 px."*

### Copy table

Puts the result on the clipboard as **tab-separated text with a header row** —
`no`, `PCT`, `ICT`, `step` — so it pastes straight into a spreadsheet. The button
reads **Copied** for 1.4 seconds.

---

## 6. Panel 3 — Recommended PCT series

<Shot
  id="fig-4"
  number="4"
  file="pct-recommend-series.png"
  caption="The recommended series with its verdict line."
  what="Crop to panel 3 — the verdict line, the result table with several rows visible, and the extrapolation warning underneath if one is showing."
  height={430}
/>

| Column | Meaning |
|---|---|
| **no** | Row number |
| **PCT** | **The integer PCT value to use for this row** |
| **ICT** | The radius this row is predicted to reach |
| **step** | How far it advanced |

### The verdict line

> *"1.84 px largest difference between steps, from 48.12 to 49.96. Within the
> 2 px limit. 16 rows, PCT 21 to 74."*

Green when the spread is within your tolerance, amber when it is over — with
*"Over the N px limit — try a larger target step."*

If no series can be built at all: *"No step fits. Reduce the target step."*

### The extrapolation warning

Amber, beneath the table, when the series reaches more than 2 % past your
measured data:

> *"20 rows reach 812.4 px, past your measured data which stops at 596.6 px.
> Rows beyond that are extrapolated."*

<div className="custom-note custom-danger">
  <div className="custom-note-title">⚠️ EXTRAPOLATED ROWS ARE NOT MEASUREMENTS</div>
  <div>
    Past the measured tip the cubic is guessing, and a cubic extrapolates badly. Those rows are shown to exactly the same precision as the reliable ones, so the table gives you no visual cue — <strong>this warning is the only signal you get</strong>. Either measure further out, or ask for fewer rows.
  </div>
</div>

---

## 7. Using the result

1. Get real ICT values — run at least one round through
   [Calibration Result](./index.md) so ICT avg is computed.
2. Open this window and click **Load round 1 from Calibration Result**.
3. Check the **validation line** is green.
4. Set the number of rows you want, then click **Pick the best step**.
5. Check the verdict is green and no extrapolation warning is showing.
6. **Copy table**.
7. Paste the PCT column into the
   [PCT Pattern Generator](../pct-pattern-generator.md) — the concentric
   **Radius** fields, or the stripline **interval** fields.
8. Regenerate the patterns, push them to the monitors, and re-run the
   calibration.

---

## Troubleshooting

| Symptom | Cause |
|---|---|
| Everything blank, red message | The reference data failed one of the [§2](#what-it-checks-before-fitting) checks |
| *"…is not attached to a Calibration Result window"* | Opened standalone. Open Cali Result first |
| *"Round 1 has no row with both PCT and ICT avg filled in"* | Run **Update Table** or **Calculate Result** on round 1 — ICT avg is computed |
| *"An ICT value does not increase from the row before it"* | The ICT column is cumulative radius, not per-row step, and must rise |
| Message says the built-in curve is being used | Fewer than 6 rows |
| Validation line is amber | The curve does not reproduce your own data. More or better rows |
| Verdict amber, spread too large | Click **Pick the best step**, or raise the tolerance if the spread is genuinely acceptable |
| *"No usable step between 10 and 80 px"* | Row count too high for what the curve can cover |
| Extrapolation warning | The series runs past your measured data. Fewer rows, or measure further out |

---

## Screenshots this page needs

| Figure | File | What to capture |
|---|---|---|
| 1 | `pct-recommend-overview.png` | Whole window with the built-in sample data |
| 2 | `pct-recommend-reference.png` | Reference data panel and its status line |
| 3 | `pct-recommend-gain-model.png` | Gain model: formula, statistics, validation |
| 4 | `pct-recommend-series.png` | Result table and verdict line |

Worth adding: a **before/after** pair — the reference table with red rows, and
the recommended series with an even spread. That single comparison explains the
window better than any of the four above.

---

## Source material

| What | Where |
|---|---|
| Layout | `cpp/ui/pct_recommend.ui` |
| Behaviour | `cpp/src/controllers/controller_pct_recommend.{h,cpp}` |
| The maths | `cpp/src/core/pattern/PctRecommend.{h,cpp}` |
| Tests | the `PatternGen` test binary |
