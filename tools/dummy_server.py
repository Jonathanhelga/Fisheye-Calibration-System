#!/usr/bin/env python3
"""Fake stand-in for the three HTTP services the app probes.

ServerProbe only asks "did an HTTP status code come back?" (ServerProbe.cpp,
answeredHttp), so nothing here needs to imitate the real Axis or Camera API.
The point of this script is control: choose which ports answer, so every
status the panel can show is reachable on a laptop with no hardware attached.

    python3 tools/dummy_server.py                 # all three answer  -> Ok
    python3 tools/dummy_server.py 8000 8001       # camera silent     -> Partial
    python3 tools/dummy_server.py --none          # nothing answers   -> Failed
    python3 tools/dummy_server.py --slow 8000     # 8000 stalls       -> Checking, then Failed

Host field in the app: 127.0.0.1
"""

import argparse
import http.server
import threading
import time

DEFAULT_PORTS = [8000, 8001, 8002]

# Labels are cosmetic, only so the terminal log is readable.
NAMES = {8000: "axis", 8001: "monitor", 8002: "camera"}


class Handler(http.server.BaseHTTPRequestHandler):
    # Set per-port by make_server below.
    label = "?"
    stall_seconds = 0.0

    def do_GET(self):
        if self.stall_seconds:
            # Longer than ServerProbe's kTimeoutMs (2000) will trip the deadline
            # timer, which is how you watch a dot sit on Checking and then fail.
            time.sleep(self.stall_seconds)
        body = f"{self.label} ok\n".encode()
        self.send_response(200)
        self.send_header("Content-Type", "text/plain")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def log_message(self, fmt, *args):
        print(f"  [{self.label}:{self.server.server_port}] {fmt % args}")


def make_server(port, stall_seconds):
    handler = type(
        "PortHandler",
        (Handler,),
        {"label": NAMES.get(port, "extra"), "stall_seconds": stall_seconds},
    )
    # 127.0.0.1, not 0.0.0.0: this is a test fixture, it has no business being
    # reachable from the rest of the network.
    return http.server.ThreadingHTTPServer(("127.0.0.1", port), handler)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("ports", nargs="*", type=int, default=None,
                        help=f"ports to answer on (default: {DEFAULT_PORTS})")
    parser.add_argument("--none", action="store_true",
                        help="answer on nothing, so every probe fails")
    parser.add_argument("--slow", nargs="*", type=int, default=[],
                        help="ports that stall past the 2s probe timeout")
    args = parser.parse_args()

    ports = [] if args.none else (args.ports or DEFAULT_PORTS)

    servers = []
    for port in ports:
        stall = 5.0 if port in args.slow else 0.0
        try:
            servers.append(make_server(port, stall))
        except OSError as exc:
            # Almost always "address already in use": a previous run is still up.
            print(f"could not listen on {port}: {exc}")
            return 1

    if not servers:
        print("listening on nothing. every probe should go red.")
    for server in servers:
        port = server.server_port
        tag = " (slow)" if port in args.slow else ""
        print(f"listening on http://127.0.0.1:{port}/  as {NAMES.get(port, 'extra')}{tag}")
        threading.Thread(target=server.serve_forever, daemon=True).start()

    print("\nset the app's Host URL to 127.0.0.1, then press Update. ctrl-c to stop.\n")
    try:
        while True:
            time.sleep(3600)
    except KeyboardInterrupt:
        print("\nstopping")
        for server in servers:
            server.shutdown()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
