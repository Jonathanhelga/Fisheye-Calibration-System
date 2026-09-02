# Is the server actually working? Published once a second by moil_supervisor.
#
# This is the message the server's console window prints, and the one the client
# shows in its status bar. It answers the question that a ROS system is
# otherwise bad at answering: not "did discovery find a node" -- discovery finds
# nodes that are wedged -- but "did each subsystem do its job just now".

std_msgs/Header header

# Seconds since the server came up.
float64 uptime

# Per subsystem, parallel arrays. Names: "axis", "camera", "monitor", "compute",
# "session", "jobs".
string[] subsystems

# 0 = OK, 1 = DEGRADED (working, something is wrong), 2 = FAILED, 3 = STARTING.
uint8[] states

# What to print next to the name: "COM3/COM4", "mf 3040x3040 @ 19 Hz",
# "7 screens, 5 mapped", "self-test OK", "3 sessions", "idle".
string[] details

# THE CALCULATION CHECK. The supervisor runs a known calibration op against a
# baked-in fixture and compares the answer to the recorded baseline. This is the
# only field here that says the MATHS is right rather than merely running --
# every other check would pass on a compute node that answers instantly with
# nonsense.
bool compute_selftest_passed

# When it last ran, and how far off the worst value was. A non-zero deviation
# with passed still true means it is inside tolerance but drifting, which is
# worth seeing before it crosses.
float64 compute_selftest_age
float64 compute_selftest_max_deviation

# Clients currently connected, by node name.
string[] clients
