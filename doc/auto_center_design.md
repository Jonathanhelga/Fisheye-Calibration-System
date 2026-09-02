# Auto centre-finding and 5-axis positioning loop — design record

The camera rides on five axes (X / Y / Z / Pitch / Yaw). The rig moves the camera
until the pattern is centred in the capture; the pattern centre is the origin
every ICT node is measured from. This document specifies the perception cascade
and the positioning loop that consumes it.

Untracked until the feature commit. The normative sections describe what the code
does today (the cascade) and what the code is required to do (the loop) — where
the two differ, the Status table says which.

## Status

| Stage | State | Evidence |
| --- | --- | --- |
| `evaluate_ring_center` + `offset_px` (twinned) | done | 4.008 read at a true 4 px displacement; noise floor 0.21 px at a converged optimum |
| `auto_center` cascade (5 stages) | done | bezel basin ratio 0.527 → 0.048 after the 5x trim; Gate B reads 0.834 where a true 3.00 px error estimates 2.742 |
| `expected_rings` from the slot-matched PNG | done | negative shows one fewer visible boundary than its layer count (colour swap, measured) |
| client wrapper `autoCenter()` | done | step 4; no `threshold` knob (gates are derived), `ringCenter()` dropped as unreachable, degrade path pinned by `SessionClientTest` |
| per-ring harmonics (twinned) | done | step 4.5; `a2` +0.0143→+0.3246 over 0–5° tilt about x, sign flips about y, `|b1|` 1.46→4.39 inner→outer |
| harmonics surfaced on the client | done | step 4.6; `AutoCenterResult` carries `ringA1/B1/A2/B2` + `ringKept`, index-aligned, fixture-tested |
| software closed-loop test | pending | fake rig, homography-warped captures |
| loop UI | pending | |
| axis orchestration | pending decision | see [Open questions](#open-questions) |

> **The client half of this work is not in this branch.** The Widgets client
> (`cpp/`) was removed, and with it `session_ros_client`'s `autoCenter()` wrapper,
> `core/centering/center_loop`, and every test target — `AutoCenterTest.cpp`,
> `RingCenterDump.cpp`, `CenterLoopTest.cpp`, `SessionClientTest.cpp`. The rows
> below marked *done* on the client side describe code that now lives only on
> `v2.0_2026_main-cpp-ros`. What remains here is the **server** half: the
> `auto_center` cascade in `ComputeDetectOps.cpp` and the engine it calls, both
> building clean.

Evidence lives in `cpp/tests/AutoCenterTest.cpp` and `cpp/tests/RingCenterDump.cpp`
on `v2.0_2026_main-cpp-ros` — no rig, no ROS, no fixtures; both generate their
patterns through `PatternGen::renderConcentric`. `ring_center_client` /
`ring_center_server` were the third dump pair: build both, run both, diff. That
pair existed because the engine was duplicated between client and server; with the
client gone there is exactly **one** copy of `moilcali_algorithm`, at
[Server/v2.1.0/common/engine/algorithm/](../Server/v2.1.0/common/engine/algorithm/),
so the `cmp` ritual no longer applies — and nothing may reintroduce a second copy.

## The cascade (normative)

One op, `detect::kAutoCenter`, in
[ComputeDetectOps.cpp](../Server/v2.1.0/common/engine/compute/ComputeDetectOps.cpp).
It is one op rather than a client chaining four because the validation needs the
ring cost at five points and there is no op for that; because `computeMutex` is
taken *per op*, so a client-driven chain would release it between stages and let
another client flip the process-wide noise and raw-node flags halfway through;
and because the ring count comes from the prepared pattern, which only the server
has.

```
auto_center(images[0], params)
  params: slot / slots[0]   which capture, and which prepared pattern to count
          escalate          false = the automatic post-shot path, true = all of it
          expected_rings    optional override
          max_seconds       default 30, checked between stages only
  |
  +-- RawNodesPin: the process-wide raw-nodes flag is forced OFF for the whole
  |   cascade and restored on the way out. Centre finding is ALWAYS done on the
  |   blurred image, whatever the node-extraction toggle says -- unpinned, the
  |   same capture would centre differently depending on a switch that has
  |   nothing to do with centring, and nothing downstream would look wrong.
  |
  +-- gray = blur_gray(capture)
  |
 [stage 0]  how many boundaries should a ray cross?
  |   params.expected_rings > 0 ..................... source = "params"
  |   else the slot's prepared PNG, if it gives >= 3 . source = "png"
  |   else leave 0, the engine takes the modal count . source = "modal"
  |
  |   The PNG is read even when params supplied the count, and reported as
  |   expected_rings_png. A prepared pattern the operator forgot to update looks
  |   identical to a correct one from in here; putting both numbers in the result
  |   makes a stale pattern visible instead of mysterious. "positive" and
  |   "negative" are the same geometry with the colour pair swapped, and a swap
  |   can still move the OUTERMOST boundary: renderConcentric draws onto a white
  |   canvas, so black-on-white is an edge and white-on-white is not. A count one
  |   too high makes every ray fall short, every ray gets dropped, and no centre
  |   is found at all -- which is why the slot picks the file.
  |
 [stage 1]  patternCenterFit(gray)
  |   Least-squares intersection of the ring edges' gradient lines: whole image,
  |   then two passes restricted to 0.22*min(W,H) around the first estimate.
  |   Milliseconds, no seed, no iteration, every strong edge pixel used.
  |
  |   cx < 0 ------------------> v1 = "gradient fit found no centre"
  |   else v1 = judge(gray, K, c1);  stages[] += pattern_center;  best = c1
  |
  +--< v1.good() OR escalate == false >------------------------> [stage 4]
  |    The automatic post-shot path stops here whatever the verdict. Escalating
  |    on every shot would put a multi-second search at the shutter while holding
  |    computeMutex -- and that lock is process-wide, i.e. every other client's
  |    ops as well.
  |
 [stage 2]  seeded ring search, radius 30 px (a 7x7 coarse grid, not 31x31)
  |   seed = c1 when stage 1 produced anything at all. Even a REJECTED gradient
  |          fit is usually within a few px, and that is what shrinks the grid.
  |        = detect_fisheye_edge() lens centre when stage 1 gave nothing. The
  |          LENS circle is not the pattern centre, so this is a seed of last
  |          resort and never a stage of its own.
  |        = the image centre when that fails too.
  |   find_center_from_rings -> judge -> stages[] += ring_center
  |
  +--< v2.usable() >-----------------------------------------> [stage 4]
  |
 [stage 3]  full search from the image centre, radius 150 px
  |   find_center_from_rings -> judge -> stages[] += ring_center_full
  |
 [stage 4]  the answer
      escalate == true  : a point only if some stage came back usable(). Else
                          cx = -1, method = "none", confidence = "failed", and
                          the metrics are CLEARED -- left in, a refusal read
                          "no centre" and "offset 0.306 px, basin ratio 0.074"
                          at the same time, the numbers of the candidate the
                          cascade had just rejected. The client falls through to
                          its roi_exact fallback.
      escalate == false : stage 1's point regardless, labelled with its verdict.
                          That is what this call answered before auto_center
                          existed, and losing it would be a regression dressed
                          up as caution.
```

`stages[]` holds at most three records — the cascade's five stages are the ring
count, the three search methods and the verdict, and only the middle three are
things that were *judged*.

### judge(): the three gates at one point

```
judge(gray, K, at)                     five ring-cost evaluations, nRays = 120
  fit = evaluate_ring_center(gray, K, at, 120)
  not ok -> evaluated = false, "no usable rings at this point"
            (an answer, not an error, and not a verdict from the gates)

  offset_px   = sqrt(2) * median(ringSpread)     UNTRIMMED: already a median, so
                                                 two bad rings in eight cannot
                                                 move it -- measured 0.218 px on
                                                 the bezel image against a true
                                                 error of 0.000 px
  keep[]      = ringSpread[k] <= 5 * median(ringSpread)
                computed ONCE, here, at the candidate
  cost        = mean over keep[] of ringSpread/ringRadius
  4 probes at (+-6, 0) and (0, +-6) px, each scored with the SAME keep[]
  basin_ratio = cost / min(probe cost)

  Gate C  rings_used >= max(3, 0.8 * expected)   rings_used counts rings DETECTED,
          rays_used  >= 0.5 * rays_total         before any trimming
             fail -> "coverage: N rings (want M), R/T rays"
  no probe produced a usable cost -> failed
  Gate A  offset_px   <= 2.0  -+
  Gate B  basin_ratio <= 0.5  -+--> "good"
  offset_px <= 6.0 AND basin_ratio <= 0.8 -> "marginal", reason carries both
  otherwise -> "failed", reason names each number and the bound it missed
```

The keep-set is frozen at the candidate and reused at every probe on purpose. Let
a probe trim a different ring and the numerator and denominator of `basin_ratio`
sum over different rings — two different quantities compared as if they were one.

### A wrong `expected_rings` is not symmetric

Measured (step 4, `[stale PNG]` in `autocenter_test`), against a capture with 8
rings:

| Prepared PNG says | What happens | Result |
| --- | --- | --- |
| **9 (over-count)** | every ray finds 8 crossings, needs 9, so every ray is dropped | all three stages fail, `ok=false`, `cx=-1` — **refuses** |
| **7 (under-count)** | rays have 8 ≥ 7, the first 7 radii are the right ones | `ok=true`, `good`, err **0.000 px** — succeeds on fewer rings, **silently** |

So the invariant is *not* "a wrong count is rejected". It is:

> **`!ok || err <= 2.0`** — the cascade either refuses or is right. It never
> returns a mis-centred point.

Over-counting is loud and fail-safe. Under-counting is silent and *correct for
this capture*, which is exactly why it is worth writing down: nothing downstream
can tell that the answer came from 7 rings instead of 8. `expected_rings_source`
and `expected_rings_png` are reported so a caller can see the count that was used
and the one the prepared pattern implies, and notice they disagree.

**This bites the loop.** The observable vector is
`[cx, cy, mean(ring_radius), mean(ring_a2/R), mean(ring_b2/R)]`, and the
per-ring arrays are indexed by ring. Change `expected_rings` mid-run — the
operator presses Update to Monitor between iterations, or a slot resolves to a
different prepared file — and ring *k* stops meaning the same physical ring. The
Broyden Jacobian was identified against the old indexing and is now being fed a
differently-shaped measurement with no signal that anything moved.

The frozen-weighting rule above only protects *within* one `judge()`; there is no
equivalent across iterations. Which way it fails follows the table:

- count goes **up** → coverage collapses → Gate C fails →
  `"coverage: N rings (want M), R/T rays"` → `confidence != good` → safety
  invariant 2 stops the move. A **visible abort**, which is the good case.
- count goes **down** → the fit still succeeds and still reports `good`, on a
  shorter observable vector. Nothing aborts.

So the loop must **latch `expected_rings` at teach time and pass it explicitly**
in `params` on every iteration, rather than letting each capture re-derive it.
That pins `ringsSource` to `"params"` for the whole run and turns any drift in the
prepared pattern into a visible `expected_rings_png` mismatch instead of a
silently reshaped measurement. Not yet implemented — see the ledger.

### Thresholds, with provenance

Every number's origin is recorded, because a threshold whose reasoning is lost
gets "tuned" by the next person.

| Constant | Value | Role |
| --- | --- | --- |
| `kGateOffsetPx` | 2.0 px | Gate A |
| `kGateBasinRatio` | 0.5 | Gate B |
| `kProbePx` | 6.0 px | probe displacement |
| `kMarginalOffsetPx` / `kMarginalBasinRatio` | 6.0 px / 0.8 | the marginal band |
| `kGateRingFraction` / `kGateRayFraction` / `kMinRings` | 0.8 / 0.5 / 3 | Gate C |
| `kSpreadOutlierFactor` | 5.0 | trim, cost only |
| `kValidateRays` | 120 | rays per judge evaluation (the search itself reports 720) |
| `kSeededSearchRadius` / `kFullSearchRadius` | 30 / 150 px | stages 2 and 3 |
| `kDefaultMaxSeconds` | 30 | checked between stages only |

**Gate A, 2.0 px.** Measured, not guessed: on a clean synthetic pattern an exact
centre reports 0.21 px, so the gate sits at about ten times the noise floor. That
headroom is why no noise-floor subtraction is done. The estimator reads ~9% LOW
near the gate — spread carries detection jitter as well as displacement and the
two add in quadrature, so a true 3.00 px error came back as 2.742 — which makes
2.0 a threshold on the **estimate**, not on the truth. Gate B is the redundancy
that covers the gap.

**Gate B, 0.5.** By construction, not by tuning. With a true error `e` the nearest
probe sits at `kProbePx - e`, so the ratio is `e/(6 - e)`, and `<= 0.5` is exactly
`e <= 2.0 px` — Gate A again, read off the cost **surface** instead of off the
spread. On the 3.00 px case that Gate A let through at 2.742, Gate B read 0.834
and rejected it. Neither gate is trusted alone.

**Trim, 5x the median.** Not a refinement — without it Gate B does not work on a
contaminated image. A flat dark band across the pattern is counted as an extra
outermost ring on the rays that survive it, and that ring comes back with a 63 px
spread against 0.15 px for the inner ones. The engine's cost is a plain mean, so
one term is ~400x every other; and being common-mode it lands in the candidate
and all four probes alike and cancels toward 1:

```
at the TRUE centre, untrimmed:  cost 0.02595  minProbe 0.04922  ratio 0.527
at the TRUE centre, trimmed:    cost 0.00143  minProbe 0.02999  ratio 0.048
```

0.527 **fails** the 0.5 gate: untrimmed, a pixel-perfect centre would be rejected
and escalated for nothing, on exactly the kind of image the escalation cannot
improve. The factor itself cannot be data-discriminated — contamination there is
30x and 409x the median, so every factor from 3 to 10 drops the same two rings,
and on the clean control every factor keeps all eight and moves the ratio not at
all. It is reasoned from the gap instead: clean ring spreads vary about ±6% around
their median, and a real capture can legitimately have outer rings a few times
worse than inner ones through lens distortion or panel tilt. 5x sits above that
legitimate band and far below the 30x floor of actual contamination.

The trim applies to the **cost** only. `offset_px` stays an untrimmed median, and
`ring_radius` / `ring_spread` / the harmonics are reported raw for every detected
ring — `ring_kept` says which of them the cost averaged, so a caller sees the trim
without the trim removing data from it.

**Gate C, rays >= 0.5.** Anchored on the engine, not on the image that exposed the
problem. The fraction was 0.75 until the bezel case measured what an occluded
image actually costs: a dark band takes out an angular sector, which is 86/120
rays at the validation ray count and 521/720 at the search's own — and at that
very point the offset was 0.306 px and the basin ratio 0.074, both comfortably
inside their gates, with the centre exact. 0.75 therefore rejected a
pixel-perfect answer on precisely the kind of image the rig produces, since a
monitor bezel *is* this. `ringCost` gives up entirely below `nRays/3`, so a third
is the hard floor already enforced one level down; half leaves this gate doing
work the engine does not while tolerating an occlusion of up to ~180°.
Deliberately **not** set near the measured 0.72, which would be fitting the
threshold to the one image that exposed it.

**Gate C, rings >= 0.8 x expected, counted PRE-trim.** Detection and scoring are
different questions — "did the pattern show up?" and "which rings can be trusted
to score it?". On the bezel case all 8 rings are detected and 2 are trimmed from
the cost; counted post-trim this gate would see 6 against a want of 6.4 and reject
a centre exact to a third of a pixel, punishing the fit for the very contamination
the trimming had just handled.

**Vignetting is a non-issue** — the local mid-level absorbs it. It stays in the
test as a regression guard and is never cited as evidence of robustness.

## The loop (normative)

Not yet implemented. This section is the specification the implementation is
measured against.

```
[teach]  once per rig geometry and pattern; never automatically
   operator jogs X/Y/Z/Pitch/Yaw to a pose they are willing to call good
   capture with the axes standing still
   auto_center(slot, escalate = true)
     confidence != good -> REFUSE to teach, quoting the stage reason verbatim.
                           A setpoint taught from a marginal pose is an error the
                           loop would then drive TOWARDS, permanently.
     confidence == good -> store s* = (cx, cy, rbar, a2bar, b2bar) and the axis
                           coordinates that produced it
   |
   v
[loop]  iteration i = 1 .. 10
   |
   +-> axes idle?  is_sensor_*_move false on all five, then settle
   |     still moving -> wait;  wait exceeded -> ABORT
   |
   +-> capture -> auto_center(slot, escalate = true)
   |     cx < 0             -> NO MOVE. stages[] is the diagnosis. STOP.
   |     confidence != good -> NO MOVE. report only. STOP.
   |        marginal is not "nearly good enough to move on" -- it is precisely
   |        the state in which the error estimate is not trustworthy
   |
   +-> s = (cx, cy, rbar, a2bar, b2bar);   e = s - s*
   |
   +-> every component within tolerance?
   |     yes, and the previous capture was too -> CONVERGED, stop
   |     yes, first time                       -> capture again, do not move
   |
   +-> dq = -J^-1 e
   |     clip |dq| to the per-move limit
   |     cumulative excursion over its limit -> ABORT
   |     |e| grew against the previous iteration -> ABORT (diverging)
   |
   +-> issue the five moves, wait idle
   |
   +-> Broyden:  J += ((ds - J dq) dq^T) / (dq^T dq)
   |             from what was OBSERVED, not from what was commanded
   +--------------------------------------------------------------> next i
```

### Observables

Five observables for five axes, all of them fields the cascade already returns:

| Observable | From | Seeded to |
| --- | --- | --- |
| `cx - cx*` | `auto_center` centre | X |
| `cy - cy*` | `auto_center` centre | Y |
| `mean(ring_radius)` against its setpoint | `ring_radius[]` | Z |
| `mean(ring_a2 / ring_radius)` | `ring_a2[]` | Pitch |
| `mean(ring_b2 / ring_radius)` | `ring_b2[]` | Yaw |

From a single image the X/Y and Pitch/Yaw corrections are very nearly degenerate:
a small tilt and a small translation move the pattern almost identically near the
centre, and a centre-finder cannot tell them apart *at all* — it is built to be
insensitive to exactly that. **Ellipticity is the tie-breaker**, because
translation cannot produce it. A bodily shift is a 1-lobed variation of constant
amplitude across rings; a tilt is 2-lobed, plus a 1-lobed keystone part that grows
with radius.

Measured, on homography-warped renders:

- fronto-parallel: every harmonic amplitude below 0.15 px;
- tilt about x: `a2` mean > 0 with `|b2| < |a2|`; tilt about y: `a2` mean < 0 —
  the phase rotates with the axis;
- ellipticity rises monotonically over 0–5° of tilt;
- the 1-lobed keystone `|b1|` is larger on the outer rings than the inner ones,
  which is what separates it from a shift.

The 1st and 2nd harmonics are exposed **raw and per ring**, and no attempt is made
to separate the centre-offset part of `a1/b1` from the keystone part. That
decomposition depends on focal length, working distance and the fisheye model, and
getting it subtly wrong would bias every correction in the same direction for
ever. The Jacobian is seeded geometrically — signs and rough magnitudes from the
table above — and Broyden-updated from the loop's own moves, which absorbs the
coupling and the scale without anyone having to write them down.

### Stop conditions

- all observables within tolerance for **2 consecutive captures** (one capture can
  be lucky; the estimator has a noise floor);
- **max 10 iterations**;
- per-move limit and total-excursion limit;
- divergence: `|e|` larger than the previous iteration.

## Safety invariants (non-negotiable)

1. **Never capture while axes are in motion.** Motion blur corrupts the fit, and
   the fit cannot tell blur from a bad centre. Wait on the `is_sensor_*_move`
   sensors, then settle, so the rig is not still ringing when the shutter opens.
2. **Never move on `confidence != good`.** Marginal means no move, report only.
3. **`cx == -1` means no move.** `stages[]` is the diagnosis; a centre fit
   returning `(-1,-1)` is the correct answer on a badly aimed shot, not an error.
4. **Step and excursion limits, and abort on divergence.**
5. **Killing the GUI stops move issuance by construction** — the loop is
   client-side, so there is nothing left to issue moves. (This is the one
   invariant the orchestration decision below can change; see Open questions.)

## Open questions

**Orchestration location.** Client-side is the current lean, for invariant 5. The
alternative is the existing server-side job mechanism —
[jobs_node.h](../Server/v2.1.0/packages/moil_server/src/jobs_node.h), the
`AutoCalibrate` action behind `/moil_jobs`, and the client's `autoCalibBtn_` —
which was built precisely so that closing the client does not abandon a moving
axis, and which keeps the frames off the network. That is the opposite trade to
invariant 5 and the two need reconciling rather than picking one blind. Pending a
read of both.

**Bootstrap when the pattern is out of frame.** Iteration 1 refuses and asks the
operator for a coarse jog. No blind moves: with no pattern there is no observable,
and a move chosen without one is a guess made by a machine that can reach its own
limits.

## Ledger

- **Latch `expected_rings` at teach time** and pass it in `params` every
  iteration, so a mid-run change to the prepared pattern cannot silently reshape
  the observable vector. See [A wrong `expected_rings` is not
  symmetric](#a-wrong-expected_rings-is-not-symmetric). Needed before the loop
  runs, not before step 6.
- `usable()` is **not** the loop's gate. It is true for `marginal`, whose offset
  estimate is allowed up to 6 px; safety invariant 2 is `good()`. Recorded because
  the two read alike at a call site and only one of them is safe to move on.
- `modalRingCount` export (one line) → final cleanup. It is file-local at
  [moilcali_algorithm.cpp:535](../Server/v2.1.0/common/engine/algorithm/moilcali_algorithm.cpp#L535).
  This was a *twinned* change when the client carried its own copy; there is now
  one copy, so it is a one-file edit and there is no `cmp` to run.
- `cpx`/`cpy` fractional handling → step 6. The op answers sub-pixel and the
  wrapper parsed doubles (`toInt()` on a `QJsonValue` holding 549.328 yields 0,
  not 549), while the form fields were read back with `.toInt()`
  (`controller_main.cpp:321` on `v2.0_2026_main-cpp-ros`). Decide where the
  rounding happens, once — and note that whichever client picks this up next has
  to make that decision from scratch, because neither call site is in this branch.
- Failed-write semantics: the automatic path writes fresh plus a loud verdict; the
  button path never overwrites. Confirm at step 6.
