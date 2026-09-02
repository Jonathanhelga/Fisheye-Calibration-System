---
id: client-windows
slug: /installation/client/windows
title: Client Installation on Windows (Installer)
---

# Client Installation on Windows — MoilCali v2.0.0

The Moil Fisheye Calibration client for Windows. One installer; double-click it
and you get a Start Menu entry. Nothing else has to be installed first — no ROS,
no Qt, no Docker, no Linux knowledge on the machine.

Roughly 15 minutes end to end, most of it the download.

---

## 1. Check the machine can run it

| Requirement | Why |
|---|---|
| **Windows 11 22H2 (build 22621) or newer**, x64 | the window is drawn by WSLg, which needs Windows 11; mirrored networking needs 22H2 |
| Hardware virtualisation enabled in BIOS/UEFI | the Linux environment is a virtual machine |
| ~4 GB free disk | the installer is 293 MB and expands to ~1.5 GB |
| Administrator rights, **once** | only for the firewall rule, and for installing WSL if it is missing |
| The camera rig on the same subnet, its ROS nodes running on domain 42 | the app is only the client |

Check your build number:

```powershell
winver
```

> **Windows 10 is not supported.** Setup warns and lets you continue, but the
> application window may never appear.

---

## 2. Download and verify

Get **`MoilCali-2.0.0-setup.exe`** (293 MB). It lives in the application
repository at `Windows/v2.0.0/MoilCali-2.0.0-setup.exe`, and is also attached to
the GitHub release.

Then confirm it arrived intact:

```powershell
Get-FileHash .\MoilCali-2.0.0-setup.exe -Algorithm SHA256
```

It must print:

```
c1702af0727414bdff580bf48875f62e2313b9d97af0ff8f72c84b906c5086a0
```

The installer is not code-signed, so Windows SmartScreen will show
*"Windows protected your PC"*. Check the hash above, then choose **More info →
Run anyway**.

---

## 3. Have the rig's IP address ready

Setup asks for it, and on Windows it is **required, not optional** — see
[Why the rig's address is required](#why-the-rigs-address-is-required) below.

For our rig it is **`192.168.103.56`**, on **ROS domain ID `42`**.

If your rig sits elsewhere, read its address on the rig itself:

```bash
hostname -I
```

---

## 4. Run the installer

1. Double-click `MoilCali-2.0.0-setup.exe`.

   **No administrator prompt to install.** The Linux environment is registered
   to your own user account, and that account is the only one that can use it.
   Installing for another user means running setup as that user.

2. **If WSL2 is not present**, setup installs it — one UAC prompt — then asks
   you to **restart Windows and run the installer again**. This is normal; the
   second pass continues from where the first stopped.

3. On **"Connection to the rig"**, fill in:

   | Field | Enter |
   |---|---|
   | Rig IP address | `192.168.103.56` |
   | ROS domain ID | `42` |
   | Fast DDS discovery server | leave blank |

4. Leave the two **Networking** tasks ticked — *Configure WSL mirrored
   networking* and *Allow incoming ROS 2 DDS traffic through Windows Firewall*.
   Without them the application installs cleanly and then finds nothing.

5. Accept the firewall step. This raises one UAC prompt and is the only part of
   the install that needs administrator rights.

Setup installs to `%LOCALAPPDATA%\Programs\MoilFisheyeCalibration`.

---

## 5. First run

**Start Menu → Moil Fisheye Calibration.**

**The first window can take up to a minute to appear.** The application probes
the rig's ROS services — 5 seconds' timeout each — before it draws anything. On
a machine that cannot reach the rig, this looks like nothing is happening. Give
it the full minute before deciding something is wrong.

The Start Menu group also contains:

| Entry | What it does |
|---|---|
| **Check connection to rig** | run this first whenever the app cannot see the rig |
| **MoilCali data folder** | your captures, in Windows Explorer |
| **View log** | what the last session did |
| **Read me first** | the same guidance, offline |

### Confirm it works

- **Capture** shows a frame → the camera node is alive.
- Type a distance (e.g. `10`) into an axis box and click a move arrow → the axis
  moves. A distance of **0** is silently ignored; that is not a bug.

---

## 6. Where your data lives

Captures, patterns and settings are at:

```
\\wsl.localhost\MoilCali\root\MoilCali
```

Open it from Explorer, or from the **MoilCali data folder** Start Menu entry.

- **Upgrading** backs this folder up to `Documents\MoilCali-backup\` first, then
  restores it. If the backup fails, setup stops rather than replacing anything.
- **Uninstalling** offers to save it to `Documents\MoilCali-backup\` — keeping it
  is the default.

---

## If it cannot see the rig

Start with **Start Menu → Check connection to rig**. It reports, in order: the
settings in use, whether mirrored networking is active, whether the rig answers,
and what the last session's ROS service calls did. Work through anything marked
`[FAIL]` from the top.

The usual causes:

| Symptom | Fix |
|---|---|
| Rig address blank or wrong | Re-run the installer — it remembers your previous answers and keeps your data. Or edit `\\wsl.localhost\MoilCali\root\.config\moilcali\env` and restart the app |
| Domain ID does not match the rig | Same place; it must equal the rig's `ROS_DOMAIN_ID` |
| Mirrored networking off | `%USERPROFILE%\.wslconfig` needs `[wsl2]` / `networkingMode=mirrored`, then `wsl --shutdown` |
| Firewall rule not covering Wi-Fi | See the check below |
| Changed Wi-Fi network since the app last ran | `wsl --shutdown`, then start the app again — WSL keeps the old address otherwise |

Verify the firewall rule covers **Public** as well:

```powershell
Get-NetFirewallRule -DisplayName "MoilCali ROS 2 DDS" | Format-List DisplayName, Profile, Enabled
```

A laptop's Wi-Fi is normally classed Public, and a rule that omits that profile
lists as enabled while allowing nothing through.

Then read the log at `\\wsl.localhost\MoilCali\root\moilcali.log` — lines
starting `[monitor]` or `[axis]` name the exact ROS service that was called and
whether it answered.

### If the window never appears

The GUI comes through WSLg. Check WSL is current:

```powershell
wsl --version
wsl --update
wsl --shutdown
```

---

## Why the rig's address is required

WSL2 mirrored networking forwards multicast out and drops it coming back, so
ROS 2's normal discovery never completes — the rig hears the client, the client
never hears the rig. Naming the rig makes discovery unicast, which does get
through. This is why an application that connects to nothing can sit on a
network where `ping` works perfectly.

Setup also writes `%USERPROFILE%\.wslconfig` for mirrored networking, but **only
if you do not already have that file**. If you do, it tells you the two lines to
add rather than overwriting settings other distros may depend on.

---

## Uninstalling

Windows Settings → **Apps & features**, or the uninstaller in the Start Menu
group. You are asked first whether to save your calibration data (the default
saves it to `Documents\MoilCali-backup`). The Linux environment and the firewall
rule are removed; your `.wslconfig` is deliberately left alone, because other WSL
distros may now depend on it.

---

## Known limitations in v2.0.0

- **Windows 11 only** in practice, per the requirements above.
- **The rig's address must be entered.** Leaving it blank installs a working
  application that will not find the rig.
- **No single-instance guard.** Launching the shortcut twice runs two copies,
  both writing to the same capture folder.
- **Not yet exercised on a machine with no WSL at all.** The two-pass
  install-WSL-then-reboot flow is implemented but has not been run end to end.
- Force-terminating the Linux environment (`wsl --terminate`) while the app is
  running logs a crash. Closing the window is clean.

---

## What is actually installed

| | |
|---|---|
| Application | `moilcali`, C++/Qt6, from the `calibration_inROS` branch |
| Source commit | `b048daa8d65ac87a0241308647fc7f382c48c5c3` |
| ROS | ROS 2 Jazzy, Fast DDS (`rmw_fastrtps_cpp`), domain 42 by default |
| Environment | A private WSL2 distribution named `MoilCali` — Ubuntu 24.04 with GL/xcb/CJK fonts only; ROS, Qt and OpenCV all come from the bundled application |
| Installed to | `%LOCALAPPDATA%\Programs\MoilFisheyeCalibration` |

It does not touch any other WSL distribution on the machine, and it is removed
completely on uninstall.

Why WSL rather than a native `.exe`: ROS 2 Jazzy has no official Windows binary
release. The binary shipped here is the identical one the Linux/Docker
deployment runs.

**Installing on Linux instead?** See [Client Installation on Linux](./client-linux.md).

**Cannot see the rig after all of the above?** The full diagnosis path — what
`ros2 node list` should return, and every reason it comes back empty — is in
[Connect the App to the Rig](../ros/connect-to-rig.md).
