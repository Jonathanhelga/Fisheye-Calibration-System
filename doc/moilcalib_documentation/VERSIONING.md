# Documentation Versioning

This site documents multiple releases of [moil-fisheye-calisys](https://github.com/perseverance-tech-tw/moil-fisheye-calisys) side by side, using Docusaurus's built-in versioning. Readers switch between them with the version dropdown in the navbar.

## Folder map

| Folder | What it is | Tracks branch | Edit it when... |
|---|---|---|---|
| `docs/` | The **current** (in-progress, unreleased) version. Always mutable, always named `docs/` no matter which version is being drafted (right now: **v2.0, ROS 2** — the page tree mirrors v1.1 plus the v2.0 additions; most page bodies are skeletons). See `docs/_README.md` for what is written and what is not. | `calibration_inROS` | You're writing docs for the next release. |
| `sidebars.js` | Sidebar for `docs/` (current version). | — | You add/remove/reorder a page in `docs/`. |
| `versioned_docs/version-1.1/` | **Frozen** snapshot of v1.1 (HTTP C++) — the complete C++/Qt6 client documentation. | `main_development` | You're fixing or extending the v1.1 docs. |
| `versioned_sidebars/version-1.1-sidebars.json` | Sidebar for `version-1.1`, frozen alongside it. | — | Same as above. |
| `versioned_docs/version-1.0/` | **Frozen** snapshot of v1.0 (HTTP Python). | `Feature-AutoLoadCaliResult` | You're patching a mistake in an already-released version's docs. Rare. |
| `versioned_sidebars/version-1.0-sidebars.json` | Sidebar for `version-1.0`, frozen alongside it. | — | Same as above. |
| `versions.json` | List of released (frozen) version numbers, newest first. | — | Auto-updated by the CLI command below — don't hand-edit. |
| `docusaurus.config.js` (`docs.versions`) | Each version's dropdown label, URL `path`, and banner; plus which version counts as the latest release (`lastVersion`). | — | You add a new frozen version, or rename a label. |

`docs/` is never renamed. It always means "whatever's being drafted right now" — after a freeze, it keeps being used for the *next* draft.

## Current state

| Version | Label in the dropdown | Lives in | URL | Banner |
|---|---|---|---|---|
| **v2.0** | `v2.0 (ROS 2)` | `docs/` (current, in progress) | `/docs/v2.0/…` | "unreleased — see the latest version (v1.1)" |
| **v1.1** | `v1.1 (HTTP C++)` | `versioned_docs/version-1.1/` (frozen) | `/docs/v1.1/…` | none (`banner: 'none'`) |
| **v1.0** | `v1.0 (HTTP PYTHON)` | `versioned_docs/version-1.0/` (frozen) | `/docs/1.0/…` | "no longer actively maintained" |

`lastVersion` is **`'1.1'`** — that is what the v2.0 banner points readers to, and what makes v1.0 count as outdated.

<!-- markdownlint-disable-next-line MD028 -->
> **⚠️ No version is served at the bare `/docs/` root.**
> Normally `lastVersion` takes the root path, but v1.1 pins `path: 'v1.1'` so its
> absolute cross-links keep working. That leaves `/docs/` empty, so any link to
> `/docs/<page>` is a broken link. Two site links therefore point at
> `/docs/v1.1/intro` explicitly: the **footer** "Introduction" entry in
> `docusaurus.config.js`, and the **"Get Started"** button in `src/pages/index.js`.
> Update both if the latest released version changes.

## Adding a new version (e.g. cutting v2.0 once the ROS port is ready to publish)

1. Make sure everything in `docs/` reflects the version you want to freeze.
2. Run, from the repo root:
   ```bash
   npx docusaurus docs:version 2.0
   ```
   This automatically:
   - Copies `docs/` → `versioned_docs/version-2.0/`
   - Copies `sidebars.js` → `versioned_sidebars/version-2.0-sidebars.json`
   - Prepends `"2.0"` to `versions.json`
   - `docs/` and `sidebars.js` are left in place, now representing the *next* draft — no manual renaming needed.
3. In `docusaurus.config.js`, update the `docs.versions` block:
   - Add a `'2.0'` entry with a `label`, and a `path` (see the warning below).
   - If v2.0 is now the latest **released** version, update `lastVersion: '2.0'`, and add `banner: 'none'` to that entry so it isn't labelled unreleased.
   - Update the `current` entry's `label` and `path` to whatever the *next* in-progress docs will be.
4. Repoint the two hard-coded links at the new latest version: the footer "Introduction" entry in `docusaurus.config.js` and the "Get Started" button in `src/pages/index.js`.
5. Clear out / rewrite `docs/` for the next in-progress version — keep each page's frontmatter and replace the body with a placeholder, so the page tree survives. A one-liner that empties every page while keeping its frontmatter:
   ```bash
   cd docs && for f in $(find . -name "*.md" ! -name "_README.md"); do
     awk 'BEGIN{n=0} /^---$/{n++; print; if(n==2){print ""; print "_Content coming soon._"; exit} next} n==1{print}' "$f" > "$f.tmp" && mv "$f.tmp" "$f"
   done
   ```
   Also delete `docs/assets/` — the images now live in the frozen snapshot.
6. Run `npm run build` locally to confirm the site builds (it fails on broken links, which is how the two link fixes above get caught), then commit and push — the GitHub Action rebuilds and redeploys automatically.

<!-- markdownlint-disable-next-line MD028 -->
> **⚠️ Freezing changes the URL unless you pin `path`.**
> While a version is *current* it is served at the `path` you set on the `current`
> entry (for example `/docs/v1.1/…`). Once frozen, Docusaurus defaults it to the
> bare version number (`/docs/1.1/…`), which breaks every absolute
> `/docs/v1.1/…` cross-link inside those pages and any external bookmark.
> **Give the newly frozen entry the same `path` it had as current** — this is why
> the `'1.1'` entry carries `path: 'v1.1'`.

> **💡 Prefer relative links in new pages.** Cross-links written as absolute
> `/moilcalib_documentation/docs/<version>/…` paths pin a page to one version and
> have to be rewritten if the path changes. Relative markdown links
> (`./other-page.md`, `../section/page.md`) are resolved per version by
> Docusaurus and survive a freeze untouched.

**Naming convention:** version numbers are `major.minor` (`1.0`, `1.1`, `2.0`, …) matching a released branch cut. Keep the platform or branch in the dropdown label (e.g. `v2.0 (ROS C++)`) so it's traceable to the source — Docusaurus itself has no concept of git branches.
