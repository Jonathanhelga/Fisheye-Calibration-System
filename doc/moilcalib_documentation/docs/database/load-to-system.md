---
id: load-to-system
slug: /database/load-to-system
title: Load to System (SharePoint)
---

# Load to System (SharePoint)

**New in version 2.0.**

Downloads a shared calibration folder from SharePoint / OneDrive through the
**Microsoft Graph API**, straight into the Calibration Result window.

Reached from the right-click menu in the
[Calibration Data window](./database-overview.md#5-the-context-menu).

---

## What this replaces

Version 1.1 listed this as its headline limitation:

> You cannot open a calibration directly from the Database window. […] the record
> has nothing to load, because it only points at files stored elsewhere.

The v1.1 answer was to find the record, note where its folder lived, download it
from cloud storage by hand, and then use **Load All Excel**.

**That workaround is no longer the only route.** It still works — and it is still
the right answer when you are offline or the download fails — but Load to System
now does it for you. The manual procedure is documented in
[Reload Calibration Data](../calibration/reload-calibration-data.md).

---

## 1. Signing in

<div className="custom-note custom-danger">
  <div className="custom-note-title">🔐 CREDENTIALS ARE NOT COMPILED INTO THE APPLICATION</div>
  <div>
    They used to be. That handed the tenant's application secret to everyone holding a build and — once committed — to everyone who could read git history, where a leaked secret survives even after it is rotated away.
    <br /><br />
    In v2.0 <strong>nothing is stored by default</strong>. What you type lives in memory for the session, and reaches the disk only if you tick <strong>Remember on this computer</strong>.
  </div>
</div>

<Shot
  id="fig-1"
  number="1"
  file="sharepoint-credentials-dialog.png"
  caption="The Microsoft Graph sign-in dialog."
  what="Open it from the Calibration Data right-click menu → SharePoint Credentials… Capture the whole dialog with the Advanced group expanded, and the fields empty or filled with placeholder values — never a real client secret."
  height={520}
/>

<div className="custom-note custom-warning">
  <div className="custom-note-title">⚠️ NEVER SCREENSHOT A REAL CLIENT SECRET</div>
  <div>
    Leave the secret field empty, or type obvious placeholder text, before capturing this dialog. A published screenshot of a working secret is exactly the leak the redesign was meant to prevent — and "Show secret" makes it readable.
  </div>
</div>

### The fields

| Field | Object name | Notes |
|---|---|---|
| **Directory (tenant) ID** | `editTenant` | Required |
| **Application (client) ID** | `editClient` | Required |
| **Client secret** | `editSecret` | Required. Masked by default |
| **Show secret** | `chkShowSecret` | Unmasks it |
| **Scope** (under *Advanced*) | `editScope` | Defaults to `https://graph.microsoft.com/.default`. Rarely changed |
| **Remember on this computer** | `chkRemember` | Off by default |
| **Test Connection** | `btnTest` | Enabled only once all three required fields are filled |
| **Forget Saved** | `btnForget` | Enabled only when a secret is actually stored on disk |

The dialog opens with an explanation that these come from your IT
administrator — *"they are not part of this program"* — and, beside the Remember
checkbox:

> *Leave this off on a shared machine: the secret is stored unencrypted in your
> user profile.*

### OK verifies before it closes

<div className="custom-note">
  <div className="custom-note-title">📌 A TYPO IS CAUGHT HERE, NOT HALF A MINUTE LATER</div>
  <div>
    <strong>OK asks Entra ID for a token first and refuses to close if it is refused.</strong> One token request, nothing downloaded. The dialog stays open with the reason on the status line — green when they work, red when they do not.
  </div>
</div>

The status line reports:

| Message | Meaning |
|---|---|
| *"Tenant ID, client ID and client secret are all required."* | A required field is empty |
| *"Contacting Microsoft..."* | The request is in flight — the window blocks while it waits |
| *"These credentials work."* | Green. OK will now close |
| the error text from Entra ID | Red. Fix and retry |

A set that verified successfully is cached, so re-clicking OK without changing
anything does not make a second request.

**Cancel is always available.** Nothing here traps you in the dialog.

---

## 2. Where credentials come from

Three sources, tried in order. Whichever answers first wins:

| Order | Source | Persists? |
|---|---|---|
| 1 | **What you typed this session** — kept in RAM | No |
| 2 | **Environment variables** — `MOILCALI_SP_TENANT_ID`, `MOILCALI_SP_CLIENT_ID`, `MOILCALI_SP_CLIENT_SECRET`, `MOILCALI_SP_SCOPE` | Per shell |
| 3 | **`~/.config/moilcali/sharepoint.json`** | Yes |

```json
{
  "tenant_id": "…",
  "client_id": "…",
  "client_secret": "…",
  "scope": "https://graph.microsoft.com/.default"
}
```

When all three come up empty, the application asks.

> **For a shared lab machine, use the environment variables.** They give every
> user the same working setup without a secret sitting in anyone's profile.

---

## 3. What is stored, and where

Only when **Remember on this computer** is ticked.

| Property | Detail |
|---|---|
| Location | `~/.config/moilcali/sharepoint.json` |
| Permissions | Created `0600` — owner-only |
| Encryption | **None.** Plain text |
| Write mode | The file is rewritten **whole** |

<div className="custom-note custom-danger">
  <div className="custom-note-title">⚠️ SAVING WITHOUT THE SECRET DELETES A SECRET SAVED EARLIER</div>
  <div>
    Because the file is rewritten in full, saving with the secret omitted <strong>drops</strong> one stored before. Downloads that worked yesterday then start asking you to sign in again, with nothing obviously changed.
  </div>
</div>

If saving fails, the download still runs — the credentials just stay
session-only:

> *"[error] — They will be used for this session only."*

### Forget Saved

Asks for confirmation, naming the exact file, then deletes it and clears the
session copy. The status line reads *"Stored credentials deleted."* and the
Remember checkbox clears itself.

<div className="custom-note">
  <div className="custom-note-title">📌 FORGETTING FROM THE DATABASE WINDOW CLOSES IT</div>
  <div>
    Reached through <strong>SharePoint Credentials…</strong> in the Calibration Data window's context menu, forgetting the credentials also <strong>closes that window</strong> — the sign-in gate would no longer let anyone back in, so leaving it open would be misleading.
  </div>
</div>

---

## 4. Running the download

1. Open the Calibration Data window from Calibration Result — **not
   standalone**, or there is nowhere to load into.
2. Find the camera, round or file you want.
3. **Right-click → Load to System.**
4. Sign in, if you are asked.
5. The Calibration Data window **closes**, and the Calibration Result window
   downloads the folder and runs Load All Excel on it.

Nothing to load into produces the honest fallback instead:

> *"No Calibration Result window is attached to load into. Resolved URL: …
> Open it in the browser instead?"*

### Which link a row resolves to

| Row | Resolves to |
|---|---|
| A camera, in the camera list | That camera's share URL |
| A round, in the rounds view | You pick from that round's files |
| A file, in the rounds view | That file |

Failures are specific and mean the **record**, not the download:
*"This camera_main does not have a usable URL."*, *"Selected file has no
web_url."*, *"This file does not have web_url."*

---

## 5. Which links work

Not every SharePoint URL can be resolved through Graph. The application checks
the link before trying, and a link it cannot handle is reported rather than
attempted.

If a link is rejected, use **Open URL** from the same context menu, download the
folder in your browser, and load it with **Load All Excel**.

---

## Troubleshooting

| Symptom | Cause |
|---|---|
| Asked to sign in every time | **Remember on this computer** is off, or a later save without the secret dropped it |
| OK will not close the dialog | The credentials were refused. The reason is on the status line in red |
| **Test Connection** greyed out | One of the three required fields is empty |
| **Forget Saved** greyed out | Nothing is stored on disk — you may be running on env vars or a session copy |
| The window froze on OK | The token request blocks while it waits. Give it a few seconds |
| Offered the browser instead of loading | The Calibration Data window was opened standalone |
| *"…has no web_url"* | The catalogue record is incomplete. Nothing to fix here |
| Downloaded, but the tables stay empty | Not a credentials problem — check `unzip` is installed, see [Reload Calibration Data](../calibration/reload-calibration-data.md) |

---

## Screenshots this page needs

| Figure | File | What to capture |
|---|---|---|
| 1 | `sharepoint-credentials-dialog.png` | The sign-in dialog, **placeholder values only** |

Two more worth having, both safe to publish:

| File | What to capture |
|---|---|
| `sharepoint-status-ok.png` | Just the status line reading *"These credentials work."* in green |
| `sharepoint-status-error.png` | Just the status line with a refusal in red |

Cropping to the status line alone keeps the secret field out of frame entirely.

---

## Source material

| What | Where |
|---|---|
| Graph download, credential resolution, storage | `cpp/src/io/SharePointSync.{h,cpp}` |
| Sign-in dialog | `cpp/ui/sharepoint_credentials.ui`, `cpp/src/controllers/controller_sharepoint_credentials.{h,cpp}` |
| The Load to System action | `cpp/src/controllers/controller_db_cali_result.{h,cpp}` |
| Python original this ports | `mvc_controller/controller_cali_result.py` |
