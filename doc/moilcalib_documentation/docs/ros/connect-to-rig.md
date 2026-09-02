---
id: connect-to-rig
slug: /ros/connect-to-rig
title: Connect the App to the Rig
---

# Connect the App to the Rig

Confirming the client can actually see the rig, and what to do when it cannot.

**Do this before expecting Capture or axis motion to work.** An application that
installed perfectly and finds nothing is the normal first outcome, and treating
it as an application fault is how people lose an afternoon here.

<div className="custom-note custom-warning">
  <div className="custom-note-title">⚠️ THERE IS NOTHING TO TYPE ANY MORE — WHICH IS THE PROBLEM</div>
  <div>
    Version 1.1 had three URL fields on the main window. If one was wrong, you could see it. Version 2.0 discovers the rig automatically, so a client that connects to nothing looks <strong>identical</strong> to one that connects perfectly, right up until you press Capture.
  </div>
</div>

---

## 1. The check

```bash
source /opt/ros/jazzy/setup.bash
ROS_DOMAIN_ID=42 ros2 node list
```

You should see all three:

```
/moil_axis
/moil_camera
/moil_monitor
```

**If they appear, the application will connect.** If the list is empty, the
problem is on the network — work through §3 rather than reinstalling.

### Signs from the application itself

| Sign | Means |
|---|---|
| **Every sensor LED is white** after the startup read | The axis node is not answering |
| **The first window took about a minute** | It timed out on each service probe — 5 seconds each — before drawing |
| Capture warns *"No image received from camera server"* | The camera node is not publishing |

---

## 2. Quick confirmation once connected

- **Capture** shows a frame → the camera node is alive.
- Type a distance — say `10` — into an axis box and click a move arrow → the axis
  moves.

<div className="custom-note">
  <div className="custom-note-title">📌 A DISTANCE OF `0` IS SILENTLY IGNORED</div>
  <div>
    The click does nothing at all: no message, no movement. <strong>That is intended, not a fault</strong>, and it is the most common false report of a broken connection.
  </div>
</div>

---

## 3. Diagnosis, by deployment

Start with the row that matches how you are running the client.

| Situation | What to do |
|---|---|
| **Same LAN as the rig** — the normal case | Work through the table in §4 |
| **Windows, installed `.exe`** | Start Menu → **Check connection to rig** — it runs `moil-wsl-check` inside the distro and reports the settings in use, whether mirrored networking is active, whether the rig answers, and what the last session's service calls did. Work through anything marked `[FAIL]` from the top |
| **Docker** | `docker compose exec moilcali moil-check` — it tests discovery, the ten services, and the camera stream **separately**, and says which one broke |
| **Different physical LANs you cannot merge** | Not supported out of the box. You need a VPN or overlay network (Tailscale, WireGuard) putting both machines on one subnet first — DDS has to be able to send data *back* to you |
| **Nothing discovered, and you want to see who is announcing** | `Windows/v2.0.0/source/docker/dds-sniff.py` listens on the DDS ports and prints the participants it hears |

<div className="custom-note custom-danger">
  <div className="custom-note-title">⚠️ DO NOT DIAGNOSE THIS WITH PING OR A PORT SCAN</div>
  <div>
    An open UDP port and a blocked one look identical from outside. <code>ping</code> working proves nothing about whether DDS traffic gets through — which is exactly why this failure is misread so often.
  </div>
</div>

---

## 4. The network causes, in order of how often they bite

### 4.1 A firewall rule that omits the Public profile

**The symptom is distinctive: the rig can see you and can ping you, while you
hear complete silence from it.** Discovery is bidirectional, and only your
inbound half is blocked.

A laptop's Wi-Fi is normally classified **Public**, and a rule scoped
`Domain,Private` lists as *enabled* while allowing nothing through.

```powershell
New-NetFirewallRule -DisplayName "ROS2 DDS domain 42" -Direction Inbound `
  -Protocol UDP -LocalPort 17900-17999 -Action Allow -Profile Domain,Private,Public
```

Check the rule, and which profile your adapter is actually in:

```powershell
Get-NetFirewallRule -DisplayName "ROS2 DDS domain 42" | Select-Object DisplayName, Profile, Enabled
Get-NetConnectionProfile | Select-Object InterfaceAlias, NetworkCategory
```

On the packaged Windows build the rule is named `MoilCali ROS 2 DDS`.

On Ubuntu: `sudo ufw allow proto udp from <subnet>`.

> `17900-17999` is domain 42 — `7400 + 250 × 42`, plus the per-participant
> unicast ports just above it.

### 4.2 WSL2 still on a `172.x` address

Mirrored networking is not active. WSL sits behind a NAT by default, and DDS
cannot work from there — it advertises the address it expects replies on, and the
rig must be able to start traffic back to you.

```powershell
wsl --shutdown
wsl -d Ubuntu-24.04 -- ip -4 -o addr show eth0
```

The address must be on the rig's subnet. **If it still reads `172.x`, nothing
later will work** — recheck `winver` (Windows 11 build 22621+) and that
`%USERPROFILE%\.wslconfig` is in the right place with:

```ini
[wsl2]
networkingMode=mirrored
dnsTunneling=true
firewall=true
vmIdleTimeout=-1
```

<div className="custom-note custom-warning">
  <div className="custom-note-title">⚠️ WSL DOES NOT FOLLOW YOU ONTO A NEW NETWORK</div>
  <div>
    Change Wi-Fi and it keeps the old address, silently transmitting from a dead IP. Run <code>wsl --shutdown</code> after <strong>any</strong> network change.
  </div>
</div>

### 4.3 Domain ID mismatch

It must equal the rig's `ROS_DOMAIN_ID`. The application forces `42` on itself;
for CLI checks, export it.

On the packaged Windows build both the address and the domain live in
`\\wsl.localhost\MoilCali\root\.config\moilcali\env` — edit and restart, or re-run
the installer, which remembers your previous answers and keeps your data.

### 4.4 A NAT in the path

| Cause | Fix |
|---|---|
| Docker **bridge** network | Use `--network host`. A bridge breaks DDS discovery |
| **Docker Desktop** on Windows | Its VM sits at `192.168.65.x` with no path to your LAN. Use WSL2's own Docker Engine |

### 4.5 Multicast blocked — common on office Wi-Fi

Set a static peer on **both** ends:

```bash
export ROS_STATIC_PEERS=<rig-ip>
```

### 4.6 RMW implementation mismatch

Both ends must use the same middleware:

```bash
export RMW_IMPLEMENTATION=rmw_fastrtps_cpp
```

### 4.7 A Fast DDS discovery server

Only if your site added one — the rig's launchers in `Server/v2.0.0/` run in
LAN/multicast mode. Set on **both** ends:

```bash
export ROS_DISCOVERY_SERVER=<rig-ip>:11811
export RMW_IMPLEMENTATION=rmw_fastrtps_cpp
```

---

## 5. Summary table

| Symptom | Cause | Fix |
|---|---|---|
| Rig can ping you, you hear nothing | Firewall missing the Public profile | §4.1 |
| WSL address reads `172.x` | Mirrored networking off | §4.2 |
| Worked yesterday, not today, after changing Wi-Fi | WSL kept the old address | `wsl --shutdown` |
| Nothing discovered, different subnets | Different LANs | VPN / overlay |
| Container sees nothing | Bridge network, or Docker Desktop | §4.4 |
| Office Wi-Fi, nothing discovered | Multicast blocked | §4.5 |
| Nodes visible from the CLI but not the app | Domain or RMW mismatch | §4.3, §4.6 |
| Every LED white, first window slow | Any of the above | Start at §1 |

---

## 6. Logs

| Deployment | Log |
|---|---|
| Windows installer | `\\wsl.localhost\MoilCali\root\moilcali.log` |
| Linux build | The terminal you launched from |

Lines starting `[monitor]` or `[axis]` name the **exact ROS service** that was
called and whether it answered — which is a much sharper signal than "the rig is
not responding".

---

## Screenshots this page needs

Mostly terminal output rather than UI, but three would help:

| File | What to capture |
|---|---|
| `connect-ros2-node-list.png` | A terminal showing `ros2 node list` returning all three nodes |
| `connect-check-rig-windows.png` | The Start Menu **Check connection to rig** output, with at least one `[FAIL]` line |
| `connect-firewall-profile.png` | The PowerShell output showing a rule's `Profile` next to `Get-NetConnectionProfile` — the Public-profile trap made visible |

---

## Source material

| What | Where |
|---|---|
| Node list, per-deployment diagnosis | `README.md` → *Connect the app to the rig* |
| Subnet / firewall / multicast / RMW / domain table | `cpp/README.md` → *Running on another device (networking)* |
| Windows-specific causes and the checker | `Windows/v2.0.0/RELEASE-v2.0.0.md` |
| DDS participant sniffer | `Windows/v2.0.0/source/docker/dds-sniff.py` |
| Docker walkthrough | `Windows/v2.0.0/source/docker/LAUNCH.md` |
