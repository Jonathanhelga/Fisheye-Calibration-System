cmake_minimum_required(VERSION 3.16)
project(moil_server VERSION 2.1.0 LANGUAGES CXX)

# The server: the rig, and everything that computes.
#
# One executable hosting eight ROS nodes -- see server_main.cpp for why the
# process is not split. This package is ONLY the ROS surface: eight nodes, a
# context that owns the devices, and a self-test. Everything computational is in
# moil_common (../../common), which knows nothing about ROS and is the same
# source the v2.0 single-process app was built from.
#
# So the division is: if it can be wrong about arithmetic, it is in moil_common.
# If it can be wrong about a service name, it is here.

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Visual Studio is a MULTI-CONFIG generator: it puts every configuration in the
# same build tree and the choice is made at build time, so CMAKE_BUILD_TYPE means
# nothing to it. colcon already passes --config Release, and the binary that comes
# out is Release -- but the -DCMAKE_BUILD_TYPE=Release on the command line goes
# unread, and CMake says so:
#
#   CMake Warning: Manually-specified variables were not used by the project:
#     CMAKE_BUILD_TYPE
#
# which reads like the build type was ignored and the server was built unoptimised.
# It was not. Reading the variable here settles the warning, and the default below
# is what makes the flag do its job on the generators where it IS the mechanism
# (Ninja, Makefiles) -- those default to no optimisation at all when it is unset.
if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(rclcpp_action REQUIRED)
find_package(std_msgs REQUIRED)
find_package(sensor_msgs REQUIRED)
find_package(moil_interfaces REQUIRED)

# The engine, the device layer and the 3D subsystem. It lives outside this
# package because it is not a ROS package and because the client links part of it
# too, so it is added by path with its own binary directory.
#
# Every include path, the Qt and OpenCV targets, /bigobj, _USE_MATH_DEFINES and
# NOMINMAX arrive with it as PUBLIC usage requirements -- they are declared in
# ../../common/CMakeLists.txt beside the code that needs them, not repeated here.
set(MOIL_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../..)
add_subdirectory(${MOIL_ROOT}/common ${CMAKE_CURRENT_BINARY_DIR}/common)

add_executable(moil_server
    src/server_main.cpp
    src/server_context.cpp
    src/axis_node.cpp
    src/camera_node.cpp
    src/monitor_node.cpp
    src/compute_node.cpp
    src/compute_selftest.cpp
    src/session_node.cpp
    # Measure3dNode is two files; measure3d_node_p.h says what is in which.
    src/measure3d_node.cpp
    src/measure3d_detect.cpp
    src/jobs_node.cpp
    src/supervisor_node.cpp
)

target_include_directories(moil_server PRIVATE src)

# ROS dependencies are linked as ordinary CMake targets, NOT with
# ament_target_dependencies().
#
# That macro has been removed from this distro (ROS 2 Rolling / lyrical): the
# package ament_cmake_target_dependencies still exists but now ships only
# ament_get_recursive_properties, so calling it fails with "Unknown CMake
# command" -- which reads like a typo rather than a removed API.
#
# The replacement is the modern idiom: rclcpp::rclcpp for libraries, and
# ${<pkg>_TARGETS} for message/service packages. It is also what upstream
# recommends now, so this does not need revisiting on the next distro.
target_link_libraries(moil_server PRIVATE
    moil_common

    rclcpp::rclcpp
    rclcpp_action::rclcpp_action
    ${std_msgs_TARGETS}
    ${sensor_msgs_TARGETS}
    ${moil_interfaces_TARGETS}
)

install(TARGETS moil_server DESTINATION lib/${PROJECT_NAME})

# ---------------------------------------------------------------------------
# The configuration the running binary reads.
#
# It is found relative to the executable -- DeviceConfig::bundledPath() and
# MonitorDevice's prepared-pattern root both build their paths from
# QCoreApplication::applicationDirPath() -- so it has to be installed BESIDE the
# binary, under lib/moil_server/config, and not merely exist in the source tree.
#
# devices.json is the one that matters and it was missing here.
#
# It holds the COM ports this rig is actually wired with (COM3 = Arduino,
# COM4 = CRUX, settled by reading Win32_SerialPort on 2026-08-10). The compiled
# defaults in device_config.h are COM4 and COM5, inherited from
# axis_module_yuanman.py. Without this file a fresh install therefore comes up on
# the wrong ports and reports "COM5 is not found", which sends whoever sees it to
# look at the cable. It only ever worked on the rig because an operator had
# already applied a mapping once and left a per-user copy behind.
#
# Installing it is safe: the server never writes back to this path. save() writes
# to DeviceConfig::userPath() -- %LOCALAPPDATA%/MoilLab/FisheyeCalisys -- because
# the install directory is not writable on a real Windows install, and load()
# prefers that per-user file whenever it exists. So this copy is a read-only
# template and re-installing it cannot discard an operator's display mapping.
# (The older comment here said the opposite and was the reason the rule was left
# out.)
set(MOIL_CONFIG_DIR ${MOIL_ROOT}/config)
install(DIRECTORY ${MOIL_CONFIG_DIR}/cali_system
        DESTINATION lib/${PROJECT_NAME}/config)
install(FILES ${MOIL_CONFIG_DIR}/camera_parameters.json
              ${MOIL_CONFIG_DIR}/devices.json
        DESTINATION lib/${PROJECT_NAME}/config)

ament_package()
