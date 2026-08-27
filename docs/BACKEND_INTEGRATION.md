# Backend integration: starting with the Server Panel

Session date: 2026-08-25.

The task from the team was to begin integrating the finished QML frontend with the real backend.
They asked for depth over width: prove that **one** page can genuinely talk to the rig, rather than half-wiring every page.
The Server Panel on the main page was the natural starting point, because everything else depends on it.

We did not write any integration code this session.
Instead we found a network-level blocker that would have made any code we wrote look broken for reasons that had nothing to do with the code.
This note records what we found, how we proved it, and what happens next.

---

## Part 0: ROS 2 from zero

Everything below assumes ROS vocabulary that has no equivalent in ordinary client/server work.
This part builds that vocabulary from nothing, so the rest of the note reads as description rather than jargon.

### It is not an operating system

ROS stands for Robot Operating System, which is a lie.
It is a messaging library plus a set of conventions for letting many small programs, on one machine or several, talk to each other.

The familiar model is one backend process that opens a port, clients dial that port, and HTTP carries the request.
One address, one door, one conversation shape.

ROS discards that.
Instead of one server with a door, there is **a room full of programs shouting at each other**.
No doors, no addresses, no port numbers.
Every strange property in Part 2 follows from that single difference.

### A node is just a program

A running program that participates in the messaging is called a **node**.
Not a container, not a thread, not a class.
A process.

The rig has three, which is why the server README insists on one window per node and all three left open:

| Window | Node | Owns |
|---|---|---|
| `run_axis_lan.bat` | axis node | COM3 and COM4 serial ports |
| `run_monitor_lan.bat` | monitor node | the DDC-CI monitors |
| `run_camera_ros_lan.bat` | camera node | the camera |

Each holds a piece of physical hardware open.
Close the window and that hardware goes dark.
That is also why the README demands Ctrl+C rather than Task Manager: force-killing skips `close_serial()`, and the next start reads sensor values that are garbage but look plausible.

Our QML app will be a **fourth node**.
That is the mental shift.
We are not writing a client that connects to a server, we are writing another program that joins the same room.

### Two ways nodes talk: topics and services

**A topic is a radio station.**
One node broadcasts continuously, nobody in particular is addressed, and zero or more listeners tune in.
No reply exists.
If nobody is listening the broadcast still happens and evaporates.

The rig has exactly one:

```
/camera/image_raw/compressed     JPEG frames, streaming forever
```

The camera node pumps frames out whether or not our app is running.
Subscribing joins mid-stream.
There is no way to ask for a particular frame, only to take whatever comes next.

**A service is a vending machine.**
One specific request goes in, exactly one response comes back, and the exchange is over.
This is the closest thing to an HTTP call, and it is what most of the rig speaks: `/axis/move`, `/axis/command`, `/axis/sensor`, `/monitor/show_pattern`, `/monitor/set_brightness`, `/camera/capture`.

The camera turns up in both groups, once as a topic and once as a service, which is not a duplication.
The next subsection unpicks exactly what each one does, because the split is easy to misread.

There is a third shape, an **action**, which the next-but-one subsection covers in full.

### The camera uses both shapes at once

Source: `packages/moil_camera_ros/moil_camera_ros/camera_node.py`.

The camera node is the one place where topics and services meet, so it is worth spelling out.
It offers three endpoints, two topics and one service:

```
/camera/image_raw/compressed     topic    BEST_EFFORT, always streaming
/camera/single_image/compressed  topic    RELIABLE + latched, published on trigger
/camera/capture                  service  std_srvs/Trigger, empty request
```

The trap is that **`/camera/capture` does not return an image**.
Its type is `std_srvs/Trigger`, whose request carries no fields at all and whose response is only `bool success` and `string message`.
Calling it returns something like `success: true, message: "captured 3040x3040 frame"`.
The pixels are nowhere in that reply.

What the service actually does is tell the node to publish the frame it just grabbed onto `/camera/single_image/compressed`.
So the flow is: call the service, wait for `success`, then read the frame off the second topic.
The request travels as a service call and the answer arrives as a broadcast, which is unlike anything in HTTP.

That second topic is **latched**, `TRANSIENT_LOCAL` in DDS terms.
A normal topic only reaches whoever is already listening, so a subscriber joining one millisecond late would miss the frame forever.
Latched means the node keeps the last message and hands it to every new subscriber the moment it connects.
It behaves like a whiteboard rather than a shout: written once, still readable by whoever walks in later.

Why split it this way at all, rather than streaming only?
The live topic is deliberately lossy.
It is `BEST_EFFORT` with `KEEP_LAST(1)`, meaning a frame that cannot be delivered in time is dropped rather than queued, which is right for a preview and wrong for calibration.
The captured frame is `RELIABLE`, so it arrives intact or not at all.
Preview and measurement are different jobs, and they get different guarantees on the same hardware.

The two image topics are deliberately opposite:

| | `/camera/image_raw/compressed` | `/camera/single_image/compressed` |
|---|---|---|
| Reliability | `BEST_EFFORT` | `RELIABLE` |
| Durability | volatile | `TRANSIENT_LOCAL` (latched) |
| When | continuously, on a timer | only when `/camera/capture` is called |
| Purpose | live preview | the frame we calibrate against |

There is a hard reason for the lossy stream, in the node's own header at `camera_node.py:7`.
A raw 3040x3040 BGR8 stream is **6.65 Gbit/s**, over six times what a 1 Gbit/s link can carry.
Hence JPEG, and hence best effort.

**One consequence for our QML client.**
QoS has to match or the connection silently never forms.
Subscribing to `/camera/image_raw/compressed` with a default `RELIABLE` profile connects to nothing, produces no error, and looks exactly like the rig being down.
The node's own docstring says so: "it must match the subscriber or the connection silently never forms".
Our preview subscriber must be `BEST_EFFORT` + `KEEP_LAST(1)`, and our capture subscriber must be `RELIABLE` + `TRANSIENT_LOCAL`.

### The third shape: an action

Source: `../moil-fisheye-calibration-system/ros/moil_interfaces/action/AxisLimitMove.action` and `cpp/src/models/device/axis_ros_client.cpp:160`.

"A service that takes a long time" is the short version and it undersells the difference.
An action is a **separate wire protocol** with three parts, not a slow service.

A service is one request and one response.
An action has three parts:

| Part | What it is | Service equivalent |
|---|---|---|
| **Goal** | what you want done, which the server may **reject** | the request |
| **Feedback** | a stream of progress messages while the work runs | none |
| **Result** | the final outcome, which can report **cancelled** | the response |

Cancellation and rejection are the parts a service simply does not have.
A service call, once sent, cannot be called back.

Vending machine versus service counter.
A service is the machine: coin in, can out, no conversation.
An action is handing a job over a counter, where the clerk can refuse it, tell you how it is going while you wait, and let you call it off halfway.

### Reading an `.action` file

The `.srv` files have two sections split by `---`, request and response.
An `.action` file has **three**, in that order: goal, result, feedback.
`AxisLimitMove.action`, trimmed to the fields:

```
string axis        # "x" | "y" | "z" | "yaw" | "pitch"
bool   high_side   # true = high side (right / up / forward)
string speed       # "low" | "mid" | "high"
---
bool   success
bool   reached_sensor
string coordinate
bool   cancelled
string message
---
int32  sensor      # 1 triggered, 0 clear, -1 unreadable
string coordinate
string stage
```

The third block is the one with no service equivalent.
While the axis is driving, the server keeps publishing that struct, so the UI can show the live sensor state and coordinate rather than a frozen spinner.

Note `reached_sensor` in the result, separate from `success`.
The file explains why: success with `reached_sensor` false means the run ended for a reason that was not the limit switch, which the operator needs to see.

### Why this one is an action, and it is not about duration

The comment at the top of `AxisLimitMove.action` gives the real reason, and it is worth quoting:

> There are LCD panels mounted around this rig and an axis driven past its limit destroys them.
> The controller's firmware limit stop is the real protection; this loop is the software backstop, and a backstop that depends on a network link staying up is not one, so the loop runs on the server, beside the serial port.

That is the argument.
Driving to a limit is a control loop: move a chunk, read that end's sensor, stop the moment it reads triggered.
If that loop lived in our QML client, every iteration would cross the network, and a dropped Wi-Fi packet at the wrong moment leaves an axis moving with nothing watching it.
So the loop runs on the rig, next to the serial port, and the action is how we ask for it and watch it.

There is a second, duller reason as well.
A single `/axis/move` request is capped at 487.5 mm by the protocol, while Z's travel is about 530 mm.
One command physically cannot cross it, so the server issues repeated legs.
The file ends with "Do not simplify that back into one move."

### What calling an action looks like

From the old client, `axis_ros_client.cpp:160`:

```cpp
if (!client->wait_for_action_server(std::chrono::seconds(5))) return false;   // is anyone serving it
auto goalFuture = client->async_send_goal(goal);                              // offer the job
auto handle = goalFuture.get();
if (!handle) return false;                                                    // server rejected it
auto resultFuture = client->async_get_result(handle);                         // wait for the outcome
```

Three round trips where a service has one: is the server there, was the goal accepted, what was the result.
This client ignores the feedback stream, which is allowed, feedback is optional to subscribe to.

Two details in that function are worth carrying into our port.
It sets no tight timeout budget, because a legitimate Z drive takes several legs.
And it is the one call in that class deliberately not serialised behind the client's mutex, because holding that lock for the whole drive would block the sensor polling the UI uses to show progress while it runs.

### Which actions actually exist

`ros/moil_interfaces/action/` in the reference repo holds six:

```
AxisLimitMove.action     drive one axis to a limit switch
AxisHome.action          home axes and wait until they are really there
AutoFrame.action
AutoCalibrate.action
CaliJob.action
RunCompute.action
```

`AxisHome.action` is the clearest illustration of why actions exist at all.
The service `/axis/command` with "home" sends the ORG command and returns immediately, which is right for a button meaning "start homing" and wrong for the All Home flow, which must know when homing has finished before it may do anything else.
The action waits for the origin sensor to read triggered and the moving lamp to read clear for several consecutive polls, then reports.
Same hardware operation, different question: "start it" versus "tell me when it is done".

Caution: these definitions come from the reference repo, not from `Server/v2.0.0/packages/`, which ships no actions at all.
That is the same Known gap recorded in Part 2.
`/axis/limit_move` was confirmed working against the real rig on 2026-08-18; the rest of this list is unverified until someone runs `ros2 action list` against the running rig.

### The three shapes side by side

Everything ROS offers is one of these three.
Learning which to reach for is most of learning ROS.

| | Topic | Service | Action |
|---|---|---|---|
| Analogy | radio station | vending machine | job handed over a counter |
| Direction | one to many, one way | one to one, round trip | one to one, ongoing |
| Reply | none | exactly one | goal ack, feedback stream, final result |
| Can be refused | no | no | yes, the server can reject a goal |
| Can be cancelled | no | no | yes |
| Progress while running | no | no | yes, the feedback stream |
| Late joiner sees it | only if latched | not applicable | not applicable |
| File extension | `.msg` | `.srv` | `.action` |
| Sections in the file | 1 | 2, request and response | 3, goal, result, feedback |
| Blocking | never | until the response | until the result, but cancellable |
| On this rig | camera frames | axis, monitor, camera capture | `/axis/limit_move` |

The decision rule, in one line each:

- **Topic** when the data exists whether or not anyone asked, and missing one is survivable.
- **Service** when a question has one answer and it arrives quickly.
- **Action** when the work takes long enough that the operator needs to see progress, or cancel it, or the server needs the right to say no.

### Why there are no addresses

When our app wants `/axis/move` it never looks up where the axis node lives and never learns an IP.
On startup, every ROS node broadcasts a "here I am, here is what I offer" packet to the whole local network, and listens for everyone else's.
This is called **discovery**.

Picture a warehouse where people find each other by walking in and shouting "I am the axis, I can move things" every few seconds.
Anyone needing an axis moved hears it and taps them on the shoulder.
Nobody has a desk number and nobody has a phone.

The shouting mechanism is **multicast**, a network feature where one packet is addressed to everyone on the subnet who is interested rather than to a single machine.
That is what "LAN multicast, no discovery server" means.

It also explains the three blanked variables in the launchers.
Setting a variable to nothing in a `.bat` file deletes it.
There is an alternative mode where one machine acts as a phone book at a fixed address, which is what the Tailscale launcher uses, and those three lines wipe that configuration out so it cannot engage by accident.
The Indonesian comment in `run_axis_lan.bat` says exactly that: "Pastikan mode lama: tanpa discovery server, tanpa pinning".

The practical consequence is that shouting only carries so far.
Multicast does not cross routers, does not cross VPNs, and does not cross most Wi-Fi access points.
Both machines must sit on the same subnet or they are two people shouting in two different warehouses.
That is precisely the blocker documented in Part 3.

### Domain ID is the channel number

Since discovery is shouting at everyone, something has to stop two unrelated robots in the same building from hearing each other.
The domain ID does that.
It is a number from 0 to 101 that maps to a distinct set of network ports.

Same warehouse, but everyone wears a walkie-talkie tuned to a channel.
Domain 42 hears only domain 42.

Our app must set `ROS_DOMAIN_ID=42` before creating any ROS object.
Getting this wrong produces no error, no timeout, and no warning.
The node comes up perfectly happy and discovers nobody.
It is the most common silent failure in ROS.

### RMW is which brand of radio

ROS 2 does not implement the networking itself.
It defines an interface called **RMW**, ROS MiddleWare, and swappable implementations plug in underneath.
The rig uses Fast DDS, formerly FastRTPS, selected by `RMW_IMPLEMENTATION=rmw_fastrtps_cpp`.

ROS is the language everyone speaks, DDS is the radio hardware carrying the voice.
Matching that string on our side is the safe move.
The `failed to load shared library 'rmw_fastrtps_cpp.dll'` error quoted in the launcher is only Windows failing to find that radio's DLL, which is why the script relaunches itself inside pixi first.

### Interfaces are the shape of the form

A `.msg` file describes a message.
A `.srv` file describes a request and its matching response.

These are not JSON.
They compile, through a generator called rosidl, into real C++ structs on both sides, and the bytes on the wire are a fixed binary layout.
So both ends must be built from the same interface definitions.
If the rig's `AxisMove.srv` carries a field ours does not, there is no friendly parse error, only a type mismatch and silence.

This is why `moil_interfaces` is a shared package that both server and client need, and why `build_ros.bat` fails with `Permission denied` when a node is still running: the `moil_interfaces` DLL is open and Windows will not overwrite an open file.

### Asking the rig instead of trusting the document

Because every node advertises what it offers, a live ROS system can be interrogated directly.
Once we are on the same subnet with domain 42 set, from any machine in the room:

```
ros2 service list
ros2 topic list
ros2 interface show moil_interfaces/srv/AxisMove
```

That asks the running rig what it actually serves right now and what the exact field layout of each request is.
This is the answer to the Known gap in Part 2: treat that output as truth and the server README as a hint.

### A worked trace: what our client will actually do

Putting every piece above into one sequence, this is the life of our QML node from launch to a captured frame.
Nothing here is written yet, it is the shape the code has to take.

**1. Before anything else, set the environment.**
`ROS_DOMAIN_ID=42` and `RMW_IMPLEMENTATION=rmw_fastrtps_cpp`, and they must be set before the first ROS object is constructed, not after.
Set them late and the node has already picked a channel.

**2. Initialise and create the node.**
Our process becomes the fourth participant in the room.
At this instant it begins broadcasting "here I am" over multicast and listening for the other three.

**3. Discovery happens on its own, and takes a moment.**
Nobody is dialled.
The axis, monitor and camera nodes are found because they are shouting on domain 42 on the same subnet.
This is why a freshly created client cannot call a service on its very first line: the other side may not have been heard from yet.
`wait_for_service` and `wait_for_action_server` exist for exactly this, and are the correct way to drive the Server Panel's status dots.

**4. The status dots become answerable.**
Each dot is really the question "has this node been discovered, and is its service reachable".
Axis, monitor and camera each get their own wait with a timeout.
A timeout here means one of four things and nothing else: wrong domain, wrong subnet, the node's window is closed, or a name mismatch.

**5. Subscribe to the preview stream.**
`/camera/image_raw/compressed`, with `BEST_EFFORT` and `KEEP_LAST(1)`.
Get this wrong and the subscription silently never pairs.

**6. Capturing a frame is three steps, not one.**
Subscribe to `/camera/single_image/compressed` with `RELIABLE` and `TRANSIENT_LOCAL`, call the `/camera/capture` service, and read the frame off that topic when `success` comes back.
Because that topic is latched, subscribing after the call still works, but subscribing first is the cleaner order.

**7. Moving an axis is a service call.**
`/axis/move` with direction, distance and speed, and one response.

**8. Driving to a limit is an action.**
Send a goal, let the server run its own control loop next to the serial port, and either watch feedback or just wait for the result.
Do not reimplement that loop client-side, for the reason the `.action` file spells out.

Read top to bottom, every failure mode in Part 3 lands somewhere in step 3.
Discovery is the fragile link, and everything after it is ordinary programming.

### Glossary, one line each

| Term | Meaning |
|---|---|
| **ROS 2** | a messaging library and conventions, not an operating system |
| **node** | one running program that participates in the messaging |
| **topic** | one way broadcast, many listeners, no reply |
| **service** | one request, one response, like an HTTP call |
| **action** | goal plus feedback stream plus result, rejectable and cancellable |
| **message / `.msg`** | the field layout of one topic message |
| **`.srv` / `.action`** | field layouts for a service pair, or a goal/result/feedback triple |
| **rosidl** | the generator that compiles those files into real C++ structs |
| **discovery** | how nodes find each other, here by multicast shouting |
| **multicast** | one packet addressed to everyone interested on the subnet |
| **discovery server** | the alternative, a fixed address acting as a phone book, disabled here |
| **`ROS_DOMAIN_ID`** | the channel number, 42 here, mismatches fail silently |
| **DDS** | the transport underneath ROS 2, here Fast DDS |
| **RMW** | the swappable interface ROS uses to talk to a DDS implementation |
| **QoS** | the delivery contract, which must match on both ends or no connection forms |
| **`BEST_EFFORT`** | drop rather than queue, correct for a live preview |
| **`RELIABLE`** | arrives intact or not at all, correct for a calibration frame |
| **`TRANSIENT_LOCAL`** | latched, late subscribers still receive the last message |
| **namespace** | the `/axis`, `/monitor`, `/camera` prefix on a name |
| **pixi** | the environment manager the rig uses to install ROS at `C:\dev\lyrical` |

### The one thing to internalize

We are not writing a client that dials a server.
We are writing a fourth program that walks into the same room, on the same channel, with the same radio, holding the same forms.
Everything that goes wrong in ROS is one of those four things not matching, and it almost always fails silently.

---

## Part 1: where the Server Panel actually stands

In ROS mode, `qml/panels/ServerConfigPanel.qml` shows four fields, ROS Domain ID, axis namespace, monitor namespace and camera topic, each with a status dot beside it, plus an Update button.

### The Update button goes nowhere

Pressing Update reaches `ServerConfigPanel.qml:211`, which sets all three status dots to `Checking` and then emits a signal:

```qml
root.rosUpdateRequested(root.domainId, root.axisNamespace,
                        root.monitorNamespace, root.cameraTopic)
```

Nothing listens to that signal.
Grepping the whole `qml/` tree finds zero handlers, and `qml/windows/Main.qml:50` instantiates the panel bare:

```qml
ServerConfigPanel {  Layout.fillWidth: true }
```

So in ROS mode the three dots would pulse grey forever.
The bell has no wire behind it.

**Closing that gap is the actual job.**

---

## Part 2: what the backend actually is

Source: `../moil-fisheye-calibration-system/Server/v2.0.0/`, specifically `README.md` and the three `.bat` launchers.

| Property | Value |
|---|---|
| Machine | Windows, at `192.168.103.56` |
| Hardware | COM3 (Arduino X/Y/Z), COM4 (CRUX yaw/pitch), DDC-CI monitors |
| ROS distro | **Lyrical**, installed via `pixi` at `C:\dev\lyrical` |
| Domain | `ROS_DOMAIN_ID=42` |
| Middleware | `RMW_IMPLEMENTATION=rmw_fastrtps_cpp` |
| Discovery | LAN multicast, **no** discovery server |
| Shape | Three separate console windows, one node each |

The LAN launchers deliberately blank out the alternatives:

```bat
set "ROS_DISCOVERY_SERVER="
set "FASTDDS_DEFAULT_PROFILES_FILE="
set "FASTRTPS_DEFAULT_PROFILES_FILE="
```

That is the important line for us.
It means discovery is plain multicast and nothing else.

### The services on offer

```
/axis/move            move an axis
/axis/command         home / stop
/axis/sensor          read one of the 20 sensors

/monitor/show_pattern            show a calibration pattern
/monitor/close_pattern           close one, or "all"
/monitor/set_brightness          one direction, or "all"
/monitor/get_brightness
/monitor/set_display_direction   map display numbers to directions
/monitor/get_display_direction
/monitor/command                 show_display_number

/camera/image_raw/compressed     topic,   JPEG frames, BEST_EFFORT, always streaming
/camera/camera_info              topic,   CameraInfo, always
/camera/single_image/compressed  topic,   RELIABLE + latched, published on trigger
/camera/capture                  service, std_srvs/Trigger, publishes to single_image
```

Note that `/camera/capture` returns only `success` and `message`, never pixels.
The captured frame appears on `/camera/single_image/compressed`.
See "The camera uses both shapes at once" in Part 0 for why, and for the QoS matching that our subscribers must get right.

The server README carries its own warning under "Known gap":
the client in `cpp/` was updated on 2026-08-18 against `/axis/position` and `/axis/limit_move`, which this folder's packages do not document serving.
Both were confirmed working against the real rig, but whether the rig runs this exact code or a newer diverged copy was never established.
Treat the list above as possibly stale until checked against the running rig.
`/axis/limit_move` is an **action**, not a service; see "The third shape: an action" in Part 0 for what that changes.

### No addresses anywhere

Notice what is missing from the whole picture: **there is no host and no port**.
That is not an oversight.
In multicast discovery, addresses genuinely do not exist.
Nodes find each other by shouting, not by dialling.

---

## Part 3: the blocker

**This Mac cannot receive multicast, and ROS 2 LAN discovery is nothing but multicast.**

### How it was proved

| Test | Result | What it proves |
|---|---|---|
| TCP to rig `:135` and `:445` | **open** | Rig is alive; unicast works both directions |
| mDNS query with unicast-reply bit set | **13 replies from 8 hosts** | Our outbound multicast escapes; replies reach us |
| Listen on `224.0.0.251:5353` (mDNS) | **0 packets in 15s** | Inbound multicast is dead |
| Listen on `239.255.0.1`, all domains 0 to 60 | **0 packets in 25s** | DDS discovery specifically is dead |
| Both listeners again, sandbox disabled | identical | Not the Claude Code sandbox |
| Multicast sent to self, loopback | **received** | Not the socket code, not a permission issue |
| `netstat -g` | `224.0.0.251` joined on `en0` | The group membership genuinely registered |

The decisive row is the second one.
Those 8 hosts answered us, so they are switched on, and hosts like that announce themselves over multicast constantly, all day long.
We received **none** of it.
That proves the blockage without the rig being involved at all.
Even with all three launcher windows running perfectly, this Mac could not have heard them.

### The machines involved

- Mac: `192.168.103.209` on `en0`, which is **Wi-Fi**.
- Rig: `192.168.103.56`, present in the ARP table at MAC `4:d4:c4:47:ff:e3`.

Same subnet, `/24`, which is exactly what the LAN launchers require.
The subnet was never the problem.

The rig does not answer ping, but that means nothing.
Windows Firewall drops ICMP by default.
The open TCP ports are the real proof of life.

### What is happening, in plain terms

ROS 2 discovery works like someone standing in the middle of an office shouting **"I am the axis node, I am here!"** every second.
That is multicast.
One shout, everyone in the room hears it, and nobody needs to know anybody's address in advance.

Unicast is the opposite.
It is a phone call to one specific person whose number you already have.

This Mac is on Wi-Fi.
The access point sits between the wired office and the wireless room like a doorman with rules:

- Phone calls, either direction? Carried. This is why ports 135 and 445 answered.
- Your shouts, going into the office? Carried. This is why 8 hosts replied to us.
- Shouts coming out of the office, into your room? **Dropped.**

That last rule is not a fault or a misconfiguration.
Broadcasting over the air is expensive, so nearly every access point throws multicast away on purpose.
The laptop is standing outside a soundproof window, watching people shout.

### Where the DDS port numbers come from

Fast DDS announces participants on a multicast address and a port derived from the domain ID:

```
SPDP multicast port = 7400 + (250 * domain_id)
```

For `ROS_DOMAIN_ID=42` that is `7400 + 10500 = 17900`, on group `239.255.0.1`.
The listener swept domains 0 to 60 rather than just 42, so a wrong domain ID on either side would still have shown up.
It did not.

---

## Part 4: two ways out

### Route 1: plug in an Ethernet cable

A wired client sits in the same broadcast domain as the rig, so the shouting reaches it.
This Mac already has Ethernet adapters registered as `en4`, `en5` and `en6`, none of which currently hold an address.

This is by far the cheapest fix.
It requires no code, no configuration, and no change on the rig.

**This is the recommended route, and it should be tried first.**

### Route 2: stop waiting for shouts, place a phone call

Fast DDS can be told to skip multicast announcements entirely and contact a known address directly.
That is the `initialPeersList` setting, supplied through an XML profile and pointed at by the `FASTDDS_DEFAULT_PROFILES_FILE` environment variable.

Rough shape:

```xml
<participant profile_name="unicast_to_rig" is_default_profile="true">
  <rtps>
    <builtin>
      <initialPeersList>
        <locator>
          <udpv4><address>192.168.103.56</address></udpv4>
        </locator>
      </initialPeersList>
    </builtin>
  </rtps>
</participant>
```

The client then sends its announcements straight to the rig by unicast, the rig learns the client exists, and replies come back by unicast.
Every step of that path is one we have already proved works over this Wi-Fi.

Only the client needs this file.
The rig can stay exactly as it is.

This is not an exotic workaround.
The team has already walked this road once: the comments in `run_axis_lan.bat` describe a Tailscale variant using `ROS_DISCOVERY_SERVER` at `100.90.24.70`, which solves the same problem in the same spirit.
The LAN launchers exist as the fallback for when Tailscale is down.

---

## Part 5: still unknown

### Will a Kilted client talk to a Lyrical server?

The rig runs ROS 2 **Lyrical**.
The newest RoboStack build available for macOS is **Kilted**; the channel `robostack-lyrical` returns 404, so it does not exist yet.

Checked directly against the package index:

```
robostack-kilted/ros-kilted-rclcpp    29.5.7    osx-arm64 available
robostack-jazzy/ros-jazzy-rclcpp      28.1.18   osx-arm64 available
robostack-lyrical                     404       does not exist
```

So a Mac client would run one distro behind the rig.
Whether the two interoperate depends on whether the message and service type hashes come out identical.
This is a question to answer by testing, not by reading, and the test only becomes possible once multicast or unicast discovery is working.

### A correction worth recording

Early in the session the claim was made that getting ROS 2 onto a Mac means a fragile build from source.
That was wrong.
The rig itself installs ROS through `pixi`, a conda-style package manager, and the same mechanism publishes ROS 2 for macOS.
ROS on this MacBook is an ordinary package install.

---

## Part 6: what this means for the UI

Look at the ROS clipboard again: Domain ID, axis namespace, monitor namespace, camera topic.

**There is nowhere to type the rig's address.**

That is correct for multicast, where addresses do not exist.
But if the project ends up on Route 2, the client must know the rig's IP, and the panel will need a field for it.

Nothing should change yet.
This is recorded only to show that the network decision reaches all the way back into the QML.

---

## Part 7: next steps

1. Get this Mac onto wired Ethernet, on the same network as the rig.
2. Ask a colleague to start all three launchers: `run_axis_lan.bat`, `run_monitor_lan.bat`, `run_camera_ros_lan.bat`.
3. Rerun the DDS listener in Appendix A. Packets arriving from `192.168.103.56` closes the network question for good.
4. If no cable is possible, set up the unicast profile from Route 2 instead, and rerun the same listener.
5. Install ROS 2 Kilted on the Mac via pixi, and confirm `ros2 node list` shows the rig's three nodes.
6. Only then start writing the client, following `../moil-fisheye-calibration-system/cpp/src/models/device/axis_ros_client.{h,cpp}` as the reference.
7. Wire `onRosUpdateRequested` in `Main.qml` to whatever that client exposes, so the three ROS dots finally mean something.

Steps 1 through 5 are network and environment work.
No line of application code should be written until step 5 passes, because until then every failure looks identical.

---

## Appendix A: the DDS discovery listener

Requires no root, no ROS install, and writes nothing.
It joins the discovery multicast group across every domain from 0 to 60 and reports what arrives.

Change `IFACE_IP` to whatever `ipconfig getifaddr en0` prints.

```python
import socket, select, sys, time, collections

IFACE_IP = "192.168.103.209"
GROUP = "239.255.0.1"
DOMAINS = range(0, 61)          # SPDP port = 7400 + 250*domain
DURATION = float(sys.argv[1]) if len(sys.argv) > 1 else 25.0

socks = {}
for d in DOMAINS:
    port = 7400 + 250 * d
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM, socket.IPPROTO_UDP)
    s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    try:
        s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEPORT, 1)
    except OSError:
        pass
    try:
        s.bind(("", port))
    except OSError as e:
        print(f"bind {port} failed: {e}")
        s.close()
        continue
    mreq = socket.inet_aton(GROUP) + socket.inet_aton(IFACE_IP)
    try:
        s.setsockopt(socket.IPPROTO_IP, socket.IP_ADD_MEMBERSHIP, mreq)
    except OSError as e:
        print(f"join {GROUP} on port {port} failed: {e}")
        s.close()
        continue
    socks[s] = d

print(f"listening on {len(socks)} domains via {IFACE_IP}, group {GROUP}, {DURATION:.0f}s")

hits = collections.Counter()
deadline = time.time() + DURATION
while time.time() < deadline:
    r, _, _ = select.select(list(socks), [], [], max(0.0, deadline - time.time()))
    for s in r:
        data, addr = s.recvfrom(65535)
        rtps = data[:4] == b"RTPS"
        ver = f"{data[4]}.{data[5]}" if rtps and len(data) > 5 else "?"
        hits[(addr[0], socks[s], rtps, ver)] += 1

if not hits:
    print("NOTHING RECEIVED on any domain 0-60.")
else:
    print(f"{'source':<20} {'domain':>6} {'RTPS':>5} {'ver':>5} {'packets':>8}")
    for (ip, d, rtps, ver), n in sorted(hits.items(), key=lambda kv: -kv[1]):
        print(f"{ip:<20} {d:>6} {str(rtps):>5} {ver:>5} {n:>8}")
```

Reading the output:

- Rows with source `192.168.103.56`, domain `42`, RTPS `True` mean discovery traffic is arriving. That is the green light.
- `NOTHING RECEIVED` means multicast still is not reaching this machine. Run Appendix B before blaming the rig.

## Appendix B: the multicast sanity check

This is the control experiment.
It asks whether this machine can hear **any** multicast at all, with the rig completely out of the picture.

It sends mDNS queries with the QU bit set, which asks responders to reply by unicast.
That splits the two directions apart so they can be judged separately.

```python
import socket, struct, time, collections

IFACE = "192.168.103.209"

def q(name, qtype=12):
    p = struct.pack("!6H", 0, 0, 1, 0, 0, 0)
    for lab in name.split("."):
        if lab:
            p += bytes([len(lab)]) + lab.encode()
    return p + b"\x00" + struct.pack("!HH", qtype, 0x8001)   # 0x8001 = QU bit set

s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
try:
    s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEPORT, 1)
except OSError:
    pass
s.bind((IFACE, 0))
s.setsockopt(socket.IPPROTO_IP, socket.IP_MULTICAST_IF, socket.inet_aton(IFACE))
s.setsockopt(socket.IPPROTO_IP, socket.IP_MULTICAST_TTL, 4)
s.settimeout(1.0)

for n in ("_services._dns-sd._udp.local", "_ipp._tcp.local", "_airplay._tcp.local"):
    s.sendto(q(n), ("224.0.0.251", 5353))

hits = collections.Counter()
end = time.time() + 12
while time.time() < end:
    try:
        d, a = s.recvfrom(9000)
        hits[a[0]] += 1
    except socket.timeout:
        pass

if hits:
    print(f"GOT {sum(hits.values())} replies from {len(hits)} hosts")
    for ip, n in hits.most_common(10):
        print(f"  {ip:<18} {n}")
else:
    print("ZERO replies. Outbound multicast is not escaping either.")
```

Interpreting the pair of appendices:

| Appendix B | Appendix A | Diagnosis |
|---|---|---|
| replies | packets | Everything works. Proceed. |
| replies | nothing | Outbound fine, inbound multicast blocked. **This is the current situation.** |
| nothing | nothing | Multicast broken in both directions, or this machine is isolated from the LAN. |

## Appendix C: useful one-liners

```bash
# Which interface holds the LAN address, and is it Wi-Fi or wired
ipconfig getifaddr en0
networksetup -listallhardwareports

# Is the rig alive at layer 2 (works even when it ignores ping)
ping -c 2 192.168.103.255 >/dev/null; arp -an | grep 192.168.103.56

# Which multicast groups this machine has actually joined
netstat -g -f inet

# Confirm unicast to the rig works (135 and 445 are standard Windows ports)
nc -z -w 3 192.168.103.56 135 && echo open
```
