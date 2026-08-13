cmake_minimum_required(VERSION 3.16)
project(FisheyeCalibrationJojo VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Qt6 REQUIRED COMPONENTS Quick)

qt_standard_project_setup(REQUIRES 6.5)
qt_policy(SET QTP0004 NEW)

qt_add_executable(fisheye_cali_jojo
    main.cpp
    Bridge.cpp Bridge.h
)

# Registers qml/Main.qml (and everything else added here later) as the
# "FisheyeCaliJojo" QML module, compiled into the binary instead of read
# from disk at runtime. Add new .qml files to QML_FILES as panels are built.
qt_add_qml_module(fisheye_cali_jojo
    URI FisheyeCaliJojo
    VERSION 1.0
    QML_FILES
        qml/Main.qml
        qml/PanelPlaceholder.qml
        qml/HistogramPanel.qml
)

target_link_libraries(fisheye_cali_jojo PRIVATE
    Qt6::Quick
)
