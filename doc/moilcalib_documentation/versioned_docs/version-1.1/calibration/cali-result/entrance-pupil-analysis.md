---
id: entrance-pupil-analysis
slug: /calibration/cali-result/entrance-pupil-analysis
title: Entrance-Pupil Shift — Graph Analysis Method
---

# Entrance-Pupil Shift — Graph Analysis Method

This page explains the three graphs on the **Graphs** tab of the Cali Result window.
It shows what they mean, and how to turn them into a single number that tells you whether your lens model is good enough.

The graphs are built from Figure 2 of D. B. Gennery, *[Generalized Camera Calibration Including Fish-Eye Lenses](https://link.springer.com/article/10.1007/s11263-006-5168-1)*, International Journal of Computer Vision 68(3), 239–266, 2006, reconstructed from your own measured calibration data.

---

## 1. Why the Entrance Pupil Moves

An ideal pinhole lens has one fixed point that it "looks from," called the entrance pupil.
Every ray of light, no matter its angle, passes through that single point.

A fish-eye lens does not work this way.
As a ray comes in at a steeper angle, the point the lens effectively looks from slides along the optical axis.
Instead of one fixed viewpoint, the lens has a whole line of possible viewpoints.

<Figure id="fig-1" number="1" caption={<>Gennery (2006), Figure 2. <strong>c</strong> is the entrance pupil for on-axis rays; the thick dashed line is the locus of the entrance pupil as the incident chief-ray angle changes. Each ray's intersection with the optical axis is the shifted point <strong>c′<sub>i</sub></strong>.</>}>

![Gennery Figure 2 — illustration of shift of entrance pupil](../../assets/images/img_103.png)

</Figure>

Gennery describes this as a shift along the optical axis:

```text
c'(θ) = c + s(θ) · o                      (Gennery Eq. 1)
```

Here `c` is the on-axis entrance pupil, `θ` is the off-axis angle of the ray, and `s(θ)` is how far the pupil has shifted.
Gennery's Eq. 3 gives that shift in terms of the lens's optical prescription, which we don't have access to.
What we do have is a set of measurements of where the camera looks from at different angles, and the shift can be recovered directly from those instead.
That estimator is defined in Section 3.

**Why this matters:** if the shift is bigger than the reprojection error you're willing to tolerate, a single-viewpoint camera model cannot fit your lens across the full field, no matter how many polynomial terms you add.
The shift is a physical property of the glass, not something a better fit can remove.
Measuring it tells you whether your remaining error is fixable or built into the lens.

---

## 2. What the System Measures

Each enabled range (up to 20) produces one measured pair of numbers:

1. **Capture** — the positive and negative pattern shots are taken, and the intersecting nodes are detected per PCT ring and direction.
2. **Compute** — these are turned into the off-axis angle (`alpha_*`) and the pattern-to-pupil distance (**PCT to Pupil** / `distance`) for that range.
3. **Aggregate** — each enabled range ends up as one Alpha Min/Max and Distance pair in the range panel.

For range `i`, the analysis uses:

| Symbol | From the UI | Meaning |
|---|---|---|
| `θ_i` | ½ (Alpha Min + Alpha Max) | Mid off-axis angle of the range, in degrees |
| `d_i` | Distance ("PCT to Pupil") | Pattern-to-pupil distance at that angle |
| `h_i` | ½ (Aggregation Min + Aggregation Max) | Mid image height of the range, in % |

The calibration pattern does not move between ranges; it sits at a fixed physical location.
So if the measured pattern-to-pupil distance changes from one range to the next, the pupil is what moved.
That distance is therefore a direct reading of the pupil's position along the optical axis, which is why the estimator below is so simple: the shift is already sitting in the data you already have.

---

## 3. The Numeric Estimator

### 3.1 Definition

The shift is defined relative to the on-axis pupil position:

```text
s(θ_i) = d_0 − d_i
```

Here `d_0` is the pattern-to-pupil distance extrapolated to `θ = 0`, so the shift at `θ = 0` is zero by definition.
A positive shift means the pupil moved toward the pattern, shortening the measured distance.
A negative shift means it moved away.

This is the same relationship as Gennery's Eq. 1, with `c` as the on-axis pupil and `s(θ)` as the shift, just recovered from measurement instead of a ray trace.

### 3.2 Estimating `d_0`

`θ = 0` is never measured directly, because a range at exactly zero off-axis angle covers no image area.
So `d_0` has to be extrapolated from the ranges you did measure.

A fish-eye lens is rotationally symmetric about its optical axis, so distance must be an even function of `θ`: it looks the same approaching zero from either side, and the curve is flat there.
Fit an even polynomial:

```text
d(θ) ≈ d_0 + a_1·θ² + a_2·θ⁴            (θ in radians)
```

Two terms are normally enough.
Use ordinary least squares over the enabled ranges; `d_0` is the intercept.
Then:

```text
s(θ) = −(a_1·θ² + a_2·θ⁴)
```

which is the measured counterpart of Gennery's Eq. 2, his model of shift versus off-axis angle.
Don't fit an odd-powered or unconstrained polynomial here: it would give a non-zero slope at the origin, which is impossible for a symmetric lens, and it would throw off the extrapolated `d_0`.

### 3.3 Algorithm

```text
Input : enabled ranges i = 1..N, each with (θ_i in degrees, d_i)
Output: d_0, s(θ), Δs, RMS residual

1. Require N ≥ 3.  (2 points fit a 2-term model exactly, leaving no residual
   and therefore no way to detect a bad measurement.)
2. Convert: θ_i <- θ_i · π / 180
3. Build the design matrix with columns [1, θ_i², θ_i⁴]
4. Solve least squares for [d_0, a_1, a_2] against the vector of d_i
5. s_i  <- d_0 − d_i                   (per-range measured shift)
6. ŝ_i  <- −(a_1·θ_i² + a_2·θ_i⁴)      (fitted shift)
7. Δs   <- max(s_i) − min(s_i)         (total pupil excursion)
8. RMS  <- sqrt( Σ(s_i − ŝ_i)² / N )   (fit quality)
```

The two headline numbers are:

| Number | Meaning |
|---|---|
| **Δs — total entrance-pupil excursion** | How far the viewpoint travels across the measured field. This is the physical quantity of interest. |
| **RMS — residual** | How well a smooth symmetric model describes that travel. |

### 3.4 Interpreting the Two Numbers

Read Δs and RMS together; the mixed cases are the interesting ones.

| Δs | RMS | Reading |
|---|---|---|
| small | small | The lens is near-single-viewpoint over this field. A pinhole-style model will fit well. |
| large | small | The pupil genuinely moves, but smoothly and predictably. Expected for a fish-eye — model it, don't fight it. |
| small | large | Suspicious. The pupil is not really moving, so the scatter is measurement noise. Check detection quality, pattern flatness, and range setup before trusting any of it. |
| large | large | Both real shift and bad data. Fix the data first; Δs is not trustworthy until RMS comes down. |

Judge Δs against your working distance: an excursion of a few millimetres is irrelevant when the pattern sits metres away, and serious in close-range work.
Judge RMS against Δs: a residual that is a large fraction of the excursion means the fit is not really describing the movement.

---

## 4. Reading the Three Graphs

<Figure id="fig-2" number="2" caption={<>The <strong>Ray Curve from Distance &amp; Alpha</strong> graph, drawn with <strong>Update Shift of Entrance Pupil</strong> on the <strong>Graphs</strong> tab — the same ray fan as Figure 1, built from measured calibration data. Each ray is labelled with its range's mid-angle and distance.</>}>

![Ray Curve from Distance and Alpha graph in the Cali Result window](../../assets/images/img_102.png)

</Figure>

### 4.1 Shift of Entrance Pupil

Axes: **Lateral displacement** (x) by **Optical Axis (distance)** (y).

For each enabled range, this draws one ray, starting on the optical axis at that range's measured distance and leaving at its measured mid-angle:

```text
p0 = (0, d_i)
p1 = (L·sin θ_i,  d_i + L·cos θ_i)          with ray length L = 300
```

This reproduces the ray fan from Gennery's Figure 2, but every ray's starting point and angle come from your own measured data, so the fan traces the real pupil movement of your lens.

The white dots are the part to actually read.
Each one sits on the optical axis at that range's distance, the point that range looks from.
A single tight cluster of dots means a stable viewpoint; a spread-out column means the pupil is travelling.
**The vertical spread of the white dots is Δs**, readable straight off the plot.
The rays themselves are just context, showing which angle produced which dot.

### 4.2 Distance vs Alpha

Axes: **Alpha Mean (degree)** by **Distance**. Plots each range's angle against its distance, sorted by angle.

This is the raw data the estimator is built from, and it's the most useful of the three graphs for judging data quality, because both failure modes are visible by eye:

- A smooth, monotone or gently curved trend means real pupil shift. Good.
- Scatter with no trend, or points jumping around, means measurement noise (the "small Δs, large RMS" row above).

Flip the axis mentally and this graph is the shift function, up to the offset `d_0` and a sign.
If it looks like noise here, no amount of fitting will fix it.

### 4.3 Distance vs IH Range

Axes: **IH Range Mean (%)** by **Distance**.

The same distances, now plotted against image height instead of angle, showing where the effect lands on the sensor rather than in object space.
Use this to see which part of the frame the pupil movement affects, and to spot ranges that cover too little image area to be reliable.

---

## 5. Practical Procedure

1. **Enable at least three ranges**, spread across the field. Clustering every range at similar angles leaves the fit unconstrained near `θ = 0` and makes `d_0` unreliable.
2. **Fill in each range's Distance and Alpha Min/Max**, via **Update** or **History Distance**. A blank or unparseable field is silently skipped by all three graphs, so an unexpectedly sparse plot usually means a missing value.
3. **Open the Graphs tab and press "Update Shift of Entrance Pupil"** to draw the ray fan from the current data.
4. **Check Distance vs Alpha first.** If it looks like noise, stop and fix the capture; the other two graphs will only launder the same bad numbers.
5. **Read Δs off the vertical spread of the white dots** in the shift plot.
6. **Compute `d_0`, the shift, Δs and RMS** per Section 3.3 for the numeric result.

---

## 6. Implementation Status

The graphs in Section 4 are implemented.
The numeric estimator in Section 3 is specified here but not yet coded.

| Piece | Status |
|---|---|
| Shift-of-entrance-pupil ray fan | ✅ Implemented |
| Distance vs Alpha | ✅ Implemented |
| Distance vs IH Range | ✅ Implemented |
| Theory dialog (Fig. 2, Eq. 1 / 3), via **Information — Entrance Pupil** | ✅ Implemented |
| Inputs — alpha and PCT-to-Pupil distance | ✅ Implemented |
| `d_0`, `s(θ)`, Δs, RMS | ❌ Not implemented |

Until the estimator is added, Δs has to be read by eye from the spread of the white dots, and the calculation in Section 3.3 has to be done outside the application.
Adding it later needs no new capture: it would reuse the same angle/distance pairs the Distance vs Alpha graph already collects, with the remaining work being the least-squares fit and somewhere to display Δs and RMS.

---

## Summary

A fish-eye lens has no single viewpoint: its entrance pupil slides along the optical axis as the off-axis angle grows.
Because the calibration pattern is fixed in space, any change in the measured PCT-to-Pupil distance between ranges is that movement.
The Shift of Entrance Pupil graph draws it as a ray fan whose axis crossings mark the viewpoint of each range, and the vertical spread of those points is the total excursion, Δs.
A large but smooth excursion is normal for a fish-eye and should be modelled; scatter without a trend means the data, not the lens, is the problem.
