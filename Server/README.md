# Server

The rig-side software. The rig PC clones this repository and runs the current
version from here, so what the client talks to and what is committed stay the same
thing.

```bat
git clone <this repo>
cd Server\v2.1.0
build_server.bat
run_server.bat
```

Afterwards a new version is `git pull` + `build_server.bat`. There is no
configuration step: `scripts\moil_env.bat` searches for Qt, OpenCV, the ROS distro
and MSVC rather than having them written down, and `scripts\moil_env.bat --check`
shows what it resolved. Should it miss something on this machine, copy
`deps.local.bat.example` to `deps.local.bat` and override that one line —
`deps.local.bat` is gitignored, so a pull never stops to reconcile this machine's
Qt location against someone else's.

| Version | Shape | Status |
|---|---|---|
| [**v2.1.0**](v2.1.0/README.md) | One process, eight ROS nodes, one shared context. Goal: the client computes nothing | **Current** — the only version in this tree |

**v2.0.0 was retired from this branch.** It was the older shape: three separate
launchers (axis / monitor / camera), transport only, Python. It carried its own
twelve-file copy of `moil_interfaces` inside `packages/`, which is exactly the
second copy the section below says must not exist — and one of those twelve,
`ShowPatternSpec.srv`, had already drifted from the canonical definition (comments
only, so the type hash still matched, but the divergence was growing).

The source is not lost: it remains on `v2.0_2026_main-cpp-ros`, where every one of
its 55 files is still committed. Retrieve it with `git show` or `git checkout` of
that branch if a rig ever has to be rolled back.

v2.1.0 is finished when every calculation has moved to the server.

## The contract

Client and server agree through **one** package: [`ros/moil_interfaces`](../ros/moil_interfaces)
at the repository root — 37 services, 6 actions, 4 messages. Both sides build from
those same files. `build_server.bat` reaches out to it with
`--paths ..\..\ros\moil_interfaces`; the client builds it as its only ROS package.

That single source is the whole point. ROS matches services by **type hash**, so a
definition differing by one character makes every call of that service fail to
connect — while topics keep flowing, so the rig looks alive and no button works,
and nothing is logged on either side beyond the client not finding a server. There
is now no second copy to keep in step.

If you add or change a definition, both sides pick it up on their next build:
`build_server.bat` on the rig, `colcon build --merge-install --base-paths .` in
`ros/` for the client.

## Domain and discovery

Everything runs on `ROS_DOMAIN_ID=42`, plain LAN multicast, no discovery server.
`ROS_DISCOVERY_SERVER`, `FASTDDS_DEFAULT_PROFILES_FILE`,
`FASTRTPS_DEFAULT_PROFILES_FILE` and `ROS_LOCALHOST_ONLY` are blanked by the
launchers and by the client, because a leftover value makes discovery find nothing
and presents as a broken network.
