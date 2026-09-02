# This folder = the CURRENT (unreleased) version

Right now this holds **v2.0 (ROS 2)** content, tracking the
[`calibration_inROS`](https://github.com/perseverance-tech-tw/moil-fisheye-calisys/tree/calibration_inROS)
branch of `moil-fisheye-calisys`.

This folder is always "whatever's being drafted next" — when v2.0 is frozen,
`npx docusaurus docs:version 2.0` copies its contents into
`versioned_docs/version-2.0/` automatically, and you keep writing the next draft
right here in `docs/`. No renaming needed.

Files here whose name starts with `_` are excluded from the built site. This is a
note for anyone browsing the repo, not a doc page.

See `/VERSIONING.md` at the repo root for the full folder map and how to cut a
new version.

---

## State of this draft

The **page tree and sidebar are complete**; most page bodies are still
skeletons. Every skeleton page carries:

- a 🚧 draft banner,
- **What this page covers** — the scope, so two people don't write the same thing,
- **Outline** — the H2 sections to fill, carried over from the v1.1 page where one
  exists,
- **What changed from v1.1** — the v2.0-specific deltas that must not be copied
  over unchanged,
- **Source material** — the exact files in `moil-fisheye-calisys` the facts come
  from, so the page can be written without guessing.

Pages that are already written, not skeletons:

| Page | Why |
|---|---|
| `intro.md` | Written from the v2.0 `README.md`. |
| `installation/client-windows.md` | Promoted from `drafts/v2.0-windows-client/`, already complete. |

## The HELP button contract

`cpp/app/Help.cpp` in the application maps each window to a page on **this**
version of the site:

| Window key | Page slug |
|---|---|
| `main_window`, `curve_color` | `/system-overview/main-window` |
| `pattern_generator` | `/calibration/pct-pattern-generator` |
| `monitor_viewer` | `/calibration/monitor-viewer` |
| `captured_image` | `/calibration/camera-calibration` |
| `cali_result` | `/calibration/cali-result` |
| `center_setup` | `/verification/setup-center` |
| `measure3d` | `/verification/3d-verification` |
| `database` | `/database/database-overview` |
| *(anything unmapped)* | `/intro` |

**Do not change those nine slugs** without changing `Help.cpp` in the same
release — a shipped binary hard-codes them, so an old build keeps requesting the
old URL and lands on a 404.
