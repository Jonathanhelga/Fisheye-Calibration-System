# Dummy server

`tools/dummy_server.py` is a fake stand-in for the three HTTP services the HTTP Server panel probes.
It exists so the connection status dots can be tested on a laptop with no camera, no axis controller, and no network hardware attached.

## Why a fake is enough

`ServerProbe` is deliberately undemanding about what it talks to.
The whole test lives in `answeredHttp()` at `ServerProbe.cpp:22`, which asks one question: did an HTTP status code come back?

A 404 passes.
A 500 passes.
Anything that speaks HTTP at all passes, because the dot means "something is listening there", not "the API is correct".

Only connection-level failures fail: refused, host not found, timeout, aborted.
Those never populate `HttpStatusCodeAttribute`, which is why testing that attribute instead of `error()` is what separates "wrong path" from "nothing is listening".

So the fake does not need to imitate `single_image` or any real endpoint.
It only needs to answer, or refuse to.

## Running it

```bash
python3 tools/dummy_server.py
```

Then in the app, set **Host URL** to `127.0.0.1`, leave the ports at their defaults, and press **Update**.

Python 3 ships with macOS, so there is nothing to install.
Stop the server with ctrl-c.

## Producing each status

The point of the flags is control.
Every state the panel can show is reachable without touching real hardware.

| Command | Axis / Monitor / Camera | Host dot |
| --- | --- | --- |
| `dummy_server.py` | green, green, green | green, `Ok` |
| `dummy_server.py 8000 8001` | green, green, red | amber, `Partial` |
| `dummy_server.py --none` | red, red, red | red, `Failed` |
| `dummy_server.py --slow 8000` | checking then red | checking, then red |
| edit any field in the panel | that dot greys | grey, `Unknown` |

The `--slow` row is the one worth running deliberately.
It stalls a response for 5 seconds, past the 2 second `kTimeoutMs` cap in `ServerProbe.cpp:12`.
That is the only way to watch the deadline `QTimer` at `ServerProbe.cpp:127` actually fire, and the only way to see a dot sit on `Checking` long enough to click other things while a probe is still in flight.

While one port is stalling, press **Update** again.
The second press supersedes the first probe through the `gen_` counter at `ServerProbe.cpp:97`, so the aborted reply is dropped instead of painting the dot red.
If a stale probe could still land, this is where you would see it.

## Notes on the script itself

It binds to `127.0.0.1`, not `0.0.0.0`.
This is a test fixture and has no business being reachable from the rest of the network.

Ports map to names purely for readable terminal logs: 8000 axis, 8001 monitor, 8002 camera.
Any other port number still works, it just logs as `extra`.

If startup prints `could not listen on 8000: address already in use`, a previous run is still alive.
Find it with `lsof -nP -iTCP:8000 -sTCP:LISTEN`, or clear all of them with `pkill -f dummy_server.py`.
