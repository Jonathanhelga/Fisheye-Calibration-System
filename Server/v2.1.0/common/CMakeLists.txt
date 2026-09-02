cmake_minimum_required(VERSION 3.16)

# moil_common -- the calibration engine, the device layer and the 3D subsystem.
#
# Everything here is the SAME source the v2.0 single-process app was built from.
# Nothing in it re-implements a formula, so there is no second copy to drift,
# which is the property the v2.0 design was built to protect and the one thing a
# client/server split most easily loses.
#
# WHY THIS IS A LIBRARY AND NOT THREE LISTS IN moil_server/CMakeLists.txt.
#
# It used to be three flat set(...) lists of ../../common/... paths, plus nine
# ../../common/... include directories, all written out in the package that
# consumes them. That put the answer to "what does this code need to compile"
# somewhere other than the code, and it meant a second consumer -- the client
# already reaches in here for CaliTableData.cpp -- had to copy the list and keep
# it right.
#
# The dependencies are declared PUBLIC on purpose: a consumer links moil_common
# and inherits the include paths, the Qt and OpenCV targets, and the MSVC flags
# this code cannot compile without. That is the whole contract, in one place.
#
# NOT a ROS package: it knows nothing about rclcpp and must not. Everything that
# speaks ROS lives in packages/moil_server/src. NOT installed either -- it is
# linked into the server binary.

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets SerialPort Concurrent)
find_package(OpenCV REQUIRED)
find_package(Eigen3 REQUIRED)

# Qt ships its bundled zlib's headers under include/QtZlib and exports the
# symbols from QtCore (as z_*, see its zconf.h). XlsxIO uses them to pack and
# unpack .xlsx in-process; it used to shell out to `zip`/`unzip`, which do not
# exist on Windows, so saving a calibration result there always failed.
get_filename_component(QT_ZLIB_INCLUDE_DIR
    "${Qt6_DIR}/../../../include/QtZlib" ABSOLUTE)
if(NOT EXISTS "${QT_ZLIB_INCLUDE_DIR}/zlib.h")
    message(FATAL_ERROR
        "zlib.h not found at ${QT_ZLIB_INCLUDE_DIR}. XlsxIO needs Qt's bundled "
        "zlib headers; point QT_ZLIB_INCLUDE_DIR at a zlib include directory.")
endif()

add_library(moil_common STATIC
    # the calibration maths
    engine/cali/CaliTableData.cpp
    # CaliCompute is five files; CaliCompute_p.h says what is in which.
    engine/cali/CaliCompute.cpp
    engine/cali/CaliComputePipeline.cpp
    engine/cali/CaliComputeNoise.cpp
    engine/cali/CaliComputeSearch.cpp
    engine/cali/CaliComputePlots.cpp
    engine/cali/Regression.cpp
    engine/cali/CaliMath.cpp
    engine/cali/CaliRound.cpp
    engine/algorithm/moilcali_algorithm.cpp
    engine/pattern/moil_pattern_gennerator.cpp
    engine/compute/ComputeDetectOps.cpp
    engine/compute/ComputeCaliOps.cpp
    engine/compute/ComputeRenderOps.cpp
    engine/session/CaliSession.cpp
    engine/session/session_registry.cpp
    engine/session/session_services.cpp

    # the hardware: two COM ports, a camera, five screens
    device/device_config.cpp
    # AxisDevice is four files; axis_backend.h says what is in which.
    device/axis_device.cpp
    device/axis_serial.cpp
    device/axis_yuanman.cpp
    device/axis_yinda.cpp
    device/axis_origin.cpp
    device/camera_device.cpp
    device/camera_mf.cpp
    # MonitorDevice is four files; monitor_device_p.h says what is in which.
    device/monitor_device.cpp
    device/monitor_screens.cpp
    device/monitor_brightness.cpp
    device/monitor_patterns.cpp
    device/compute.cpp
    device/session_store.cpp

    # the fisheye model, 3D verification, and the spreadsheet it writes
    moildev/Moildev.cpp
    measure3d/moil_3d_algorithm.cpp
    measure3d/Measure3dTypes.cpp
    measure3d/AnypointChessboard.cpp
    measure3d/PlaneFit3dViz.cpp
    io/XlsxIO.cpp
)

# MonitorDevice and SessionStore are QObjects, so this target needs moc. The
# server's own sources do not -- none of the eight nodes is a QObject -- which is
# why AUTOMOC is set HERE rather than globally: a target-level property keeps moc
# off translation units that would only pay for it.
set_target_properties(moil_common PROPERTIES AUTOMOC ON)

# The headers are addressed by bare name from inside this tree (#include
# "CaliCompute.h"), so every source directory is a search path. PUBLIC: the
# server's nodes include these same headers.
target_include_directories(moil_common PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/engine/cali
    ${CMAKE_CURRENT_SOURCE_DIR}/engine/algorithm
    ${CMAKE_CURRENT_SOURCE_DIR}/engine/compute
    ${CMAKE_CURRENT_SOURCE_DIR}/engine/pattern
    ${CMAKE_CURRENT_SOURCE_DIR}/engine/session
    ${CMAKE_CURRENT_SOURCE_DIR}/device
    ${CMAKE_CURRENT_SOURCE_DIR}/measure3d
    ${CMAKE_CURRENT_SOURCE_DIR}/moildev
    ${CMAKE_CURRENT_SOURCE_DIR}/io
    ${OpenCV_INCLUDE_DIRS}
    ${QT_ZLIB_INCLUDE_DIR}
)

target_link_libraries(moil_common PUBLIC
    Qt6::Core Qt6::Gui Qt6::Widgets Qt6::SerialPort Qt6::Concurrent
    ${OpenCV_LIBS}
    Eigen3::Eigen
)

# Monitor brightness over DDC/CI, and the Media Foundation camera backend -- the
# two Windows APIs the device layer reaches directly. PRIVATE because no header
# exposes them; CMake still forwards them to whoever links this static library.
target_link_libraries(moil_common PRIVATE
    $<$<PLATFORM_ID:Windows>:Dxva2>
    $<$<PLATFORM_ID:Windows>:mfplat>
    $<$<PLATFORM_ID:Windows>:mf>
    $<$<PLATFORM_ID:Windows>:mfreadwrite>
    $<$<PLATFORM_ID:Windows>:mfuuid>
)

target_compile_features(moil_common PUBLIC cxx_std_17)

if(MSVC)
    # PUBLIC, all three, because a consumer compiling these headers needs the
    # same settings and finding that out from the error message is expensive:
    #
    #   /bigobj      the rosidl-generated headers plus Qt's templates overflow
    #                the default section limit in the nodes that include several
    #                service types
    #   _USE_MATH_DEFINES
    #                MSVC only exposes M_PI and friends when this is set. The
    #                engine, the Moildev port and the 3D subsystem all use it;
    #                without it they fail with "'M_PI': undeclared identifier"
    #                in files that have nothing obviously wrong
    #   NOMINMAX     windows.h defines min/max as MACROS, which breaks std::min
    #                in the engine. Reached here through the Media Foundation
    #                camera backend and dxva2
    target_compile_options(moil_common PUBLIC /bigobj /utf-8)
    target_compile_definitions(moil_common PUBLIC
        _USE_MATH_DEFINES NOMINMAX WIN32_LEAN_AND_MEAN)
endif()
