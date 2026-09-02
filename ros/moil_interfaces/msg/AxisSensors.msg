# Status seluruh sensor axis dalam satu pesan.
#
# Lewat HTTP ini adalah 20 panggilan is_sensor_*() terpisah, satu request per
# sensor. Di ROS semuanya dikirim sekaligus secara periodik, jadi client cukup
# satu subscription dan mendapat gambaran yang konsisten pada satu waktu -- bukan
# 20 potret yang diambil pada 20 momen berbeda.

std_msgs/Header header

# Sumbu X
bool x_left
bool x_org
bool x_right
bool x_move

# Sumbu Y
bool y_down
bool y_org
bool y_up
bool y_move

# Sumbu Z
bool z_back
bool z_org
bool z_forward
bool z_move

# Yaw
bool yaw_left
bool yaw_org
bool yaw_right
bool yaw_move

# Pitch
bool pitch_down
bool pitch_org
bool pitch_up
bool pitch_move
