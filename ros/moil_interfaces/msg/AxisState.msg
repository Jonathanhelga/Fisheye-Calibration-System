# One axis, as a stream: its four lamps and its position, together.
#
# This is the topic the sensor lamps and the coordinate fields read. It replaces
# the polling loop each client used to run -- four sensor services and a position
# service per axis, every ~20 ms, per client -- with ONE poll on the server that
# every client hears.
#
# That change is not only tidier, it is the difference between one operator and
# two. The polls are blocking serial round trips on ports the motion commands
# share; N clients polling meant N times the traffic on the one link the stage is
# driven over. Here the cost is fixed no matter how many people are watching.
#
# It is OPT-IN and per axis -- see AxisWatch.srv. Nothing is polled until someone
# asks, because polling all five axes continuously is 25 round trips per sweep on
# a link that a move command also needs.

std_msgs/Header header

# "x" | "y" | "z" | "yaw" | "pitch"
string axis

# Tri-state, and the third state is the point: 1 triggered, 0 clear, -1 could not
# be read. An unreadable limit switch must never arrive as "not triggered" --
# that is how a drive-to-end keeps driving.
int8 sensor_low     # left / down / back
int8 sensor_org
int8 sensor_high    # right / up / forward
int8 sensor_moving

# The controller's raw counter, verbatim. Empty when the read failed, which is
# NOT the same as "0".
string raw

# Millimetres or degrees from the software zero, already converted and already
# formatted -- the client prints this. See AxisPosition.srv.
string coordinate

# False when this axis has never been homed: `coordinate` is then the raw counter
# scaled, and the field showing it should say so rather than implying a reference
# that does not exist.
bool has_zero
