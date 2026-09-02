# Website

This website is built using [Docusaurus](https://docusaurus.io/), a modern static website generator.

## Installation

```bash
npm install
```

## Local Development

```bash
npm start
```

Then open **http://localhost:3000/moilcalib_documentation/** — the `/moilcalib_documentation/` part comes from `baseUrl` and the site 404s without it.

Markdown edits reload live. **Changes to `docusaurus.config.js` or `sidebars.js` need a restart.**

| Command | What it does |
|---|---|
| `npm start` | Dev server on port 3000, hot reload — use this while writing |
| `npm run build` | Production build into `build/`; fails on broken links, so run it before pushing |
| `npm run serve` | Serves the built `build/` folder |
| `npm run clear` | Wipes the `.docusaurus` cache when a change won't show up |

> The CI builds with `npm ci` and Node 22, so `package-lock.json` is the lockfile that matters. A `yarn.lock` is also present, but yarn is not used by the pipeline.

### Sharing the dev server with someone else

To let a reviewer on the same Wi-Fi open the site, bind the server to every network interface instead of loopback only:

```bash
ipconfig getifaddr en0        # your LAN IP, e.g. 192.168.1.42 (en0 is Wi-Fi on macOS)
npm start -- --host 0.0.0.0
```

Then send them `http://<LAN-IP>:3000/moilcalib_documentation/`.

By default the server listens on `127.0.0.1`, which is reachable only from your own machine, so a plain `npm start` cannot be opened by anyone else no matter what address they try.
The `--` passes `--host` through npm to `docusaurus start` rather than npm swallowing it.

The `/moilcalib_documentation/` path is required here too, for the same `baseUrl` reason as above.
Read it off the `[SUCCESS] ... running at:` line the server prints on startup and replace only the `localhost` portion with your IP.

Check it yourself before handing over the URL, since a wrong path and a blocked port look identical to the person receiving it:

```bash
curl -s -o /dev/null -w "%{http_code}\n" http://<LAN-IP>:3000/moilcalib_documentation/
```

`200` means it works, `404` means the path is wrong, and `000` or a hang means the server is down, the `--host` flag is missing, or the macOS firewall is blocking Node (allow it when prompted).
Your `192.168.x.x` address is a DHCP lease and can change when you reconnect to Wi-Fi, so re-run `ipconfig getifaddr en0` each time rather than reusing an old IP.

For a reviewer who is **not** on the same network, `npx cloudflared tunnel --url http://localhost:3000` prints a public `trycloudflare.com` URL.
That publishes the docs to the open internet for as long as the command runs, so use it only when the content is fine to be publicly reachable.

## Documentation Structure & Versioning

This site documents multiple releases of [moil-fisheye-calisys](https://github.com/perseverance-tech-tw/moil-fisheye-calisys) side by side, using Docusaurus's built-in versioning. A version dropdown in the navbar lets readers switch between them.

| Folder | Version | URL | Tracks | Status |
|---|---|---|---|---|
| `docs/` | current — **v2.0 (ROS 2)** | `/docs/v2.0/…` | `calibration_inROS` | In progress — page tree and sidebar final, most page bodies still skeletons (see `docs/_README.md`) |
| `versioned_docs/version-1.1/` | **v1.1 (HTTP C++)** | `/docs/v1.1/…` | `main_development` | **Latest release** (`lastVersion`) — the complete C++/Qt6 client docs |
| `versioned_docs/version-1.0/` | **v1.0 (HTTP PYTHON)** | `/docs/1.0/…` | `Feature-AutoLoadCaliResult` | Frozen — only edit to fix mistakes |

Note that **no version is served at the bare `/docs/` root**, because v1.1 pins its own URL path. The footer link and the homepage "Get Started" button therefore point at `/docs/v1.1/intro` explicitly.

**To edit content:**
- Writing the **v2.0** docs → edit files under `docs/`, and update `sidebars.js` if you add or remove a page.
- Fixing or extending **v1.1** → edit files under `versioned_docs/version-1.1/` (and `versioned_sidebars/version-1.1-sidebars.json` if the sidebar changes). It is frozen, so changes there do **not** affect `docs/`.
- Fixing **v1.0** → same, under `versioned_docs/version-1.0/`.

**To add a new version** (e.g. once the ROS port is ready to become v2.0), see [VERSIONING.md](./VERSIONING.md) for the full step-by-step, including the `docusaurus docs:version` command, the `docusaurus.config.js` changes it requires, and the `path` pitfall that breaks cross-links when a version is frozen.

## Build

```bash
npm run build
```

Generates static content into the `build` directory. The build **fails on broken links** (`onBrokenLinks: 'throw'`), so run it before pushing — it is the main safety net against a bad cross-link or a version path change.

## Deployment

Deployment is automatic. Pushing to `main` triggers [`.github/workflows/deploy-docs.yml`](.github/workflows/deploy-docs.yml), which:

1. Installs with `npm ci` on Node 22
2. Runs `npm run build`
3. Uploads `build/` as a GitHub Pages artifact and deploys it

No manual deploy step is needed — don't run `yarn deploy`, and don't push to a `gh-pages` branch by hand. The workflow also runs on `master` and `Feature-AutoLoadCaliResult`, and can be started manually from the Actions tab (`workflow_dispatch`).

Live site: **https://perseverance-tech-tw.github.io/moilcalib_documentation/**
