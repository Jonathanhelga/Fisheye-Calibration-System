---
id: client-docker
slug: /installation/client/docker
title: Client in Docker
---

# Client in Docker

Running the client in a container instead of installing ROS 2 and its
dependencies on the host.

New in version 2.0 — there was no Docker route in v1.1.

---

## The one rule that decides everything

<div className="custom-note custom-danger">
  <div className="custom-note-title">⛔ THE CONTAINER MUST SHARE THE NETWORK STACK OF A MACHINE REALLY ON THE RIG'S LAN</div>
  <div>
    ROS 2 discovers nodes with UDP multicast, and DDS peers advertise <strong>their own IP addresses</strong>. Put a NAT or a bridge in between and the client either sees nothing, or sees the nodes and never receives data.
    <br /><br />
    That is why every command below uses <code>--network host</code>. It is not a convenience.
  </div>
</div>

---

## 1. Install Docker Engine (Ubuntu)

```bash
sudo apt update && sudo apt install -y ca-certificates curl
sudo install -m 0755 -d /etc/apt/keyrings
sudo curl -fsSL https://download.docker.com/linux/ubuntu/gpg -o /etc/apt/keyrings/docker.asc
sudo chmod a+r /etc/apt/keyrings/docker.asc
echo "deb [arch=$(dpkg --print-architecture) signed-by=/etc/apt/keyrings/docker.asc] https://download.docker.com/linux/ubuntu $(. /etc/os-release && echo $VERSION_CODENAME) stable" \
  | sudo tee /etc/apt/sources.list.d/docker.list > /dev/null
sudo apt update
sudo apt install -y docker-ce docker-ce-cli containerd.io docker-buildx-plugin docker-compose-plugin
sudo usermod -aG docker "$USER" && newgrp docker   # run docker without sudo
docker run --rm hello-world                         # verify
```

---

## 2. Build the image

From the repository root:

```bash
docker build -t moilcali-ros .        # ~10-15 min the first time
```

---

## 3. Run, with GUI and ROS

```bash
xhost +local:root                     # let the container reach your X server
docker run --rm -it \
  --network host \
  -e DISPLAY="$DISPLAY" \
  -e ROS_DOMAIN_ID=42 \
  -v /tmp/.X11-unix:/tmp/.X11-unix \
  moilcali-ros
```

| Flag | Why |
|---|---|
| `--network host` | **Mandatory.** A bridge network breaks DDS discovery |
| `-e DISPLAY` + the X11 mount | Puts the Qt window on your desktop |
| `-e ROS_DOMAIN_ID=42` | Matches the rig |

### Two variations

- **Wayland session:** `xhost +local:root` and the X11 mount still work, through
  XWayland.
- **3D viewer:** add `--device /dev/dri` for GPU rendering. Without it you get
  software GL — it works, but the
  [3D View](../verification/3d-verification.md) is slow to rotate.

The rig's nodes must already be running on domain 42. Check from the host:

```bash
ROS_DOMAIN_ID=42 ros2 node list
```

---

## 4. On Windows

<div className="custom-note custom-danger">
  <div className="custom-note-title">⛔ USE WSL2's OWN DOCKER ENGINE — NOT DOCKER DESKTOP</div>
  <div>
    Docker Desktop puts containers in <strong>its own VM at <code>192.168.65.x</code></strong>, with no path to your LAN. The container builds, starts and runs perfectly, and discovers nothing — which reads as a broken image rather than a networking choice.
  </div>
</div>

The full walkthrough with verified output at each step, including the
browser-based (noVNC) variant the packaged build uses, is in
`Windows/v2.0.0/source/docker/LAUNCH.md`.

---

## 5. Built-in diagnosis: `moil-check`

The container ships with a checker that is **more specific than anything the host
tools give you**:

```bash
docker compose exec moilcali moil-check
```

It tests three things **separately** and says which one broke:

| Stage | What it proves |
|---|---|
| Discovery | The client can see the rig's nodes at all |
| The ten services | Each axis and monitor service answers |
| The camera stream | Frames are actually arriving |

That separation matters: "discovery works but the camera stream is dead" is a
completely different problem from "nothing is discovered", and the host-side
`ros2 node list` cannot tell them apart.

---

## 6. What Docker does not solve

| | |
|---|---|
| `zip` / `unzip` | Included in the image |
| Same-subnet requirement | **Unchanged.** The container inherits the host's network, so the host must be on the rig's LAN |
| Multicast blocked on the network | **Unchanged** — see [Connect the App to the Rig](../ros/connect-to-rig.md) |
| Persistent data | Captures live inside the container unless you mount a volume |

---

## Troubleshooting

| Symptom | Cause |
|---|---|
| Container starts, discovers nothing | Missing `--network host`, or Docker Desktop on Windows |
| No window appears | `xhost +local:root` not run, or the X11 socket not mounted |
| 3D View very slow | Software GL. Add `--device /dev/dri` |
| `moil-check` fails at discovery | A network problem — [Connect the App to the Rig](../ros/connect-to-rig.md) |
| `moil-check` passes discovery, fails the camera | The camera node is not publishing. The rig, not the network |
| Captures vanish when the container exits | No volume mounted. `--rm` removes the container and its filesystem |

---

## Screenshots this page needs

Terminal output rather than UI:

| File | What to capture |
|---|---|
| `docker-moil-check-pass.png` | `moil-check` with all three stages passing |
| `docker-moil-check-fail.png` | The same with one stage failing — it shows how specific the output is |
| `docker-app-running.png` | The Qt window on the host desktop, drawn from the container |

---

## Source material

| What | Where |
|---|---|
| The image | `Dockerfile` (repo root) |
| Full walkthrough with expected output | `Windows/v2.0.0/source/docker/LAUNCH.md` |
| Build context and compose file | `Windows/v2.0.0/source/docker/` |
| DDS participant sniffer | `Windows/v2.0.0/source/docker/dds-sniff.py` |
