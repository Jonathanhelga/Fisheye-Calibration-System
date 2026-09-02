---
id: client-linux
slug: /installation/client/linux
title: Client Installation on Linux (Ubuntu 24.04)
---

# Client Installation on Linux (Ubuntu 24.04)

Building and installing the client from source. This is also the route used from
**WSL2** on Windows 11, and the one the shipped installer is produced from.

Roughly 30–45 minutes the first time.

<div className="custom-note custom-danger">
  <div className="custom-note-title">⛔ UBUNTU 24.04 LTS (NOBLE) IS REQUIRED, NOT RECOMMENDED</div>
  <div>
    ROS 2 Jazzy is only packaged for Noble. Check with <code>lsb_release -a</code>. Version 1.1's build worked on any recent distro — this one does not.
  </div>
</div>

You must also be on the **same subnet as the rig**: ROS 2 discovery uses UDP
multicast, and multicast does not cross a router.

---

## 1. Install ROS 2 Jazzy (once)

```bash
# locale + tools
sudo apt update && sudo apt install -y locales curl gnupg software-properties-common
sudo locale-gen en_US en_US.UTF-8
sudo update-locale LC_ALL=en_US.UTF-8 LANG=en_US.UTF-8

# enable "universe" + add the ROS 2 apt repository
sudo add-apt-repository universe -y
sudo curl -sSL https://raw.githubusercontent.com/ros/rosdistro/master/ros.key \
     -o /usr/share/keyrings/ros-archive-keyring.gpg
echo "deb [arch=$(dpkg --print-architecture) signed-by=/usr/share/keyrings/ros-archive-keyring.gpg] http://packages.ros.org/ros2/ubuntu noble main" \
     | sudo tee /etc/apt/sources.list.d/ros2.list > /dev/null

sudo apt update
sudo apt install -y ros-jazzy-desktop python3-colcon-common-extensions git
```

<div className="custom-note custom-warning">
  <div className="custom-note-title">⚠️ python3-colcon-common-extensions IS NOT PULLED IN BY ros-jazzy-desktop</div>
  <div>
    Install it explicitly — it is in the command above — or the build fails later, at <code>moil_interfaces</code>, with an error that does not obviously name colcon.
  </div>
</div>

Verify:

```bash
source /opt/ros/jazzy/setup.bash
ros2 --help >/dev/null && colcon version-check >/dev/null 2>&1 && echo "ROS + colcon OK"
```

---

## 2. Build, in one shot

```bash
git clone -b calibration_inROS https://github.com/perseverance-tech-tw/moil-fisheye-calisys.git
cd moil-fisheye-calisys

./setup.sh
```

`setup.sh` does four things:

1. Installs the system dependencies — Qt6, OpenCV, Eigen, cmake, colcon.
2. Sources ROS.
3. Builds `moil_interfaces` into `~/moil_ros_ws`.
4. Builds and installs the app to `~/.local` — the binary, a desktop icon, and a
   `moilcali-run` wrapper that sources ROS for you.

> **The repository is private.** The clone needs GitHub access:
> `sudo apt install -y gh && gh auth login`, then re-run it. An SSH remote or a
> personal access token works too.

---

## 3. Install `zip` and `unzip` yourself

```bash
sudo apt install -y unzip zip
```

<div className="custom-note custom-danger">
  <div className="custom-note-title">⚠️ setup.sh DOES NOT INSTALL THESE, AND THE APP NEEDS BOTH AT RUNTIME</div>
  <div>
    Excel <code>.xlsx</code> import and export shell out to them.
    <br /><br />
    Without <code>unzip</code>, <em>Load All Excel</em> reports <strong>"No round subfolders (1..10) with .xlsx found"</strong> even when the files are plainly there — the folders are found, and every file reads back empty. The message points at the wrong cause entirely.
    <br /><br />
    Without <code>zip</code>, saving results fails.
  </div>
</div>

---

## 4. Run

```bash
~/.local/bin/moilcali-run
```

Or the **Moil Fisheye Calibration** desktop icon.

The application sets `ROS_DOMAIN_ID=42` itself — you never export it by hand for
the app, only for CLI checks.

> **The first window can take up to a minute.** It probes the rig's services with
> a 5-second timeout each before drawing anything. On a machine that cannot see
> the rig, this looks like nothing is happening.

---

## 5. Rebuilding while you work

`setup.sh` is for the first install only. Afterwards build directly — but **source
the interfaces workspace first**, or CMake will not find `moil_interfaces`:

```bash
source /opt/ros/jazzy/setup.bash
source ~/moil_ros_ws/install/setup.bash

cmake -S cpp -B cpp/build          # configure — re-run after adding new files
cmake --build cpp/build -j2        # compile -> cpp/build/moilcali
```

<div className="custom-note custom-danger">
  <div className="custom-note-title">⚠️ ALWAYS PASS `-j2`</div>
  <div>
    A bare <code>-j</code> or <code>-j$(nproc)</code> can exhaust memory and get the compiler killed — <strong>exit code 137</strong>, with no useful message.
  </div>
</div>

<div className="custom-note custom-warning">
  <div className="custom-note-title">⚠️ CHANGED ANYTHING UNDER ros/moil_interfaces/?</div>
  <div>
    Rebuild that package <strong>first</strong>: <code>cd ~/moil_ros_ws && colcon build --packages-select moil_interfaces</code>, or re-run <code>./setup.sh</code>. The app compiles against the <em>installed</em> interfaces, not the ones in this tree — so rebuilding only the app changes nothing.
  </div>
</div>

---

## 6. Windows 11 + WSL2 — three extra steps

Do these **once**, then follow §1 onward inside the Ubuntu-24.04 terminal.
Everything from there is identical.

### 6.1 Install the distro

```powershell
wsl --install -d Ubuntu-24.04
```

### 6.2 Give WSL your real LAN address

By default WSL sits behind a NAT at `172.x`, and ROS 2 cannot work from there.
Create or edit `%USERPROFILE%\.wslconfig`:

```ini
[wsl2]
networkingMode=mirrored
dnsTunneling=true
firewall=true
vmIdleTimeout=-1
```

Apply it, then confirm WSL holds a **LAN** address:

```powershell
wsl --shutdown
wsl -d Ubuntu-24.04 -- ip -4 -o addr show eth0
```

<div className="custom-note custom-danger">
  <div className="custom-note-title">⚠️ IF IT STILL READS 172.x, NOTHING LATER WILL WORK</div>
  <div>
    Mirrored mode is not active. Recheck <code>winver</code> — Windows 11 build 22621 or newer — and the file's location.
    <br /><br />
    Also: <strong>WSL does not follow you onto a new network.</strong> Change Wi-Fi and it keeps the old address, silently transmitting from a dead IP. Run <code>wsl --shutdown</code> after any network change.
  </div>
</div>

### 6.3 Open the firewall — including the Public profile

In an **Administrator** PowerShell:

```powershell
New-NetFirewallRule -DisplayName "ROS2 DDS domain 42" -Direction Inbound `
  -Protocol UDP -LocalPort 17900-17999 -Action Allow -Profile Domain,Private,Public
```

Verify the rule, and which profile your adapter is actually in:

```powershell
Get-NetFirewallRule -DisplayName "ROS2 DDS domain 42" | Select-Object DisplayName, Profile, Enabled
Get-NetConnectionProfile | Select-Object InterfaceAlias, NetworkCategory
```

<div className="custom-note custom-danger">
  <div className="custom-note-title">⚠️ DO NOT OMIT `Public`</div>
  <div>
    A laptop's Wi-Fi is normally classified Public, and a rule scoped <code>Domain,Private</code> lists as <em>enabled</em> while allowing nothing through. The symptom is brutal to diagnose: <strong>the rig sees you and can ping you, while you hear complete silence from it</strong> — discovery is bidirectional and only your inbound half is blocked.
  </div>
</div>

`17900-17999` is domain 42: `7400 + 250 × 42`, plus the per-participant unicast
ports just above it.

### Two more that cost people an hour

- **`&&` does not chain commands in Windows PowerShell 5.1.** Use `;` or separate
  lines.
- **Docker, if you use it, must be WSL2's own Docker Engine** — *not* Docker
  Desktop, which puts containers in its own VM at `192.168.65.x` with no path to
  your LAN.

---

## 7. Run the tests

```bash
for t in cpp/build/*_test; do echo "== $t =="; "$t" || break; done
```

Covers `CaliCompute`, `CaliMath`, `CaliRound`, `Database`, `Measure3d`, `Moil3d`,
`MoilCali`, `PatternGen`, `Regression`, `Xlsx`.

---

## 8. Package a portable build

Self-contained — bundles Qt, OpenCV and the plugins:

```bash
bash cpp/packaging/make_linux_7z.sh 2.0.0     # -> moilcali-2.0.0-linux.7z
```

On the target machine:

```bash
cmake -E tar xf moilcali-2.0.0-linux.7z
cd moilcali-linux && ./run.sh
```

---

## Next

**Installed is not connected.** Go to
[Connect the App to the Rig](../ros/connect-to-rig.md) before expecting anything
to work.

---

## Troubleshooting

| Symptom | Cause |
|---|---|
| Build fails at `moil_interfaces` | `python3-colcon-common-extensions` is missing |
| CMake cannot find `moil_interfaces` | You did not source `~/moil_ros_ws/install/setup.bash` |
| Compiler killed, exit code 137 | Out of memory. Use `-j2` |
| Changed a `.srv`, nothing happened | Rebuild `moil_interfaces` first — the app uses the installed copy |
| *"No round subfolders (1..10) with .xlsx found"* | `unzip` is missing |
| Saving Excel fails | `zip` is missing |
| First window takes a minute | Service probes timing out. That is a missing rig |
| WSL address reads `172.x` | Mirrored networking is not active |
| Rig can ping you, you hear nothing | Firewall rule missing the Public profile |

---

## Screenshots this page needs

Terminal output rather than UI:

| File | What to capture |
|---|---|
| `install-linux-setup-complete.png` | The tail of a successful `./setup.sh` |
| `install-linux-tests-pass.png` | The test loop output, all binaries passing |
| `install-wsl-lan-address.png` | `ip -4 -o addr show eth0` showing a real LAN address, not `172.x` |

---

## Source material

| What | Where |
|---|---|
| Ubuntu and WSL2 routes, verbatim | `README.md` (repo root) |
| Build detail, networking table | `cpp/README.md` |
| The install script | `setup.sh` |
| Portable packaging | `cpp/packaging/make_linux_7z.sh` |
