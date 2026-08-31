cmake_minimum_required(VERSION 3.16)
project(FisheyeCalibrationJojo VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# QuickControls2 is needed for QQuickStyle (selecting the "Basic" style in main.cpp).
# Network is needed for the reachability probes in ServerProbe.
find_package(Qt6 REQUIRED COMPONENTS Quick QuickControls2 Network)

option(FISHEYE_ENABLE_SUBAPPS
       "Build the ported QWidget sub-apps (3D verification, center setup)" ON)

# The sub-apps are QWidget code ported from v2.0_2026_main-cpp-ros. They are the
# only thing here that needs Widgets/Concurrent/OpenGLWidgets, OpenCV and Eigen,
# and they were written against the OpenCV 4 API, so the search asks for 4
# explicitly rather than taking whatever OpenCV happens to be installed.
if(FISHEYE_ENABLE_SUBAPPS)
    if(APPLE AND EXISTS "/opt/homebrew/opt/opencv@4/lib/cmake/opencv4")
        list(APPEND CMAKE_PREFIX_PATH "/opt/homebrew/opt/opencv@4")
    endif()

    find_package(Qt6 QUIET COMPONENTS Widgets Concurrent OpenGLWidgets)
    find_package(OpenCV 4 QUIET)
    find_package(Eigen3 QUIET)

    set(FISHEYE_SUBAPP_MISSING "")
    if(NOT Qt6Widgets_FOUND OR NOT Qt6Concurrent_FOUND OR NOT Qt6OpenGLWidgets_FOUND)
        list(APPEND FISHEYE_SUBAPP_MISSING "Qt6 Widgets/Concurrent/OpenGLWidgets")
    endif()
    if(NOT OpenCV_FOUND)
        list(APPEND FISHEYE_SUBAPP_MISSING "OpenCV 4")
    endif()
    if(NOT Eigen3_FOUND)
        list(APPEND FISHEYE_SUBAPP_MISSING "Eigen3")
    endif()

    if(FISHEYE_SUBAPP_MISSING)
        list(JOIN FISHEYE_SUBAPP_MISSING ", " FISHEYE_SUBAPP_MISSING)
        message(STATUS
            "Sub-apps disabled, missing: ${FISHEYE_SUBAPP_MISSING}. "
            "The QML app still builds; install the above and reconfigure to get them back.")
        set(FISHEYE_ENABLE_SUBAPPS OFF)
    else()
        message(STATUS "Sub-apps enabled, using OpenCV ${OpenCV_VERSION}")
    endif()
endif()

if(UNIX AND NOT APPLE)
    set(FISHEYE_ROS_DEFAULT ON)
else()
    set(FISHEYE_ROS_DEFAULT OFF)
endif()

option(FISHEYE_ENABLE_ROS "Build the ROS 2 server probe" ${FISHEYE_ROS_DEFAULT})

# ROS is the live rig backend. It defaults ON for Linux because that is where the
# MiniPC runs, but a Linux box without a ROS 2 overlay sourced must still be able
# to configure, so the lookup is QUIET and switches itself off rather than
# aborting. RosServerProbe and AxisController already compile to inert stubs when
# FISHEYE_ROS_ENABLED is absent.
if(FISHEYE_ENABLE_ROS)
    find_package(rclcpp QUIET)
    find_package(moil_interfaces QUIET)

    set(FISHEYE_ROS_MISSING "")
    if(NOT rclcpp_FOUND)
        list(APPEND FISHEYE_ROS_MISSING "rclcpp")
    endif()
    if(NOT moil_interfaces_FOUND)
        list(APPEND FISHEYE_ROS_MISSING "moil_interfaces")
    endif()

    if(FISHEYE_ROS_MISSING)
        list(JOIN FISHEYE_ROS_MISSING ", " FISHEYE_ROS_MISSING)
        message(STATUS
            "ROS backend disabled, missing: ${FISHEYE_ROS_MISSING}. "
            "The app builds and runs with the rig features inert. "
            "Source your ROS 2 overlay and reconfigure to get them back.")
        set(FISHEYE_ENABLE_ROS OFF)
    endif()
endif()

qt_standard_project_setup(REQUIRES 6.5)
qt_policy(SET QTP0004 NEW)

qt_add_executable(moil_fisheye_cali
    src/main.cpp
)


set_source_files_properties(qml/Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

qt_add_qml_module(moil_fisheye_cali
    URI FisheyeCaliJojo
    VERSION 1.0
    QML_FILES
        qml/Theme.qml

        qml/windows/Main.qml
        qml/windows/PatternAndMonitor.qml
        qml/windows/MoilCalibrationResult.qml

        qml/panels/CameraPanel.qml
        qml/panels/HistogramPanel.qml
        qml/panels/AxisControlPanel.qml
        qml/panels/CenteringPanel.qml
        qml/panels/LiveCameraPanel.qml
        qml/panels/ServerConfigPanel.qml
        qml/panels/ConcentricPanel.qml
        qml/panels/StriplinePanel.qml
        qml/panels/ChessboardPanel.qml
        qml/panels/DpadMonitorViewer.qml
        qml/panels/MonitorSlotPanel.qml
        qml/panels/CaliResultDataPanel.qml
        qml/panels/CaliResultParameterPanel.qml

        qml/controls/ScaledCanvas.qml
        qml/controls/ActionButton.qml
        qml/controls/GhostButton.qml
        qml/controls/ColorSwatch.qml
        qml/controls/ValueField.qml
        qml/controls/DecimalValidator.qml
        qml/controls/LabeledField.qml
        qml/controls/StatusDot.qml
        qml/controls/SegmentedControl.qml
        qml/controls/PatternToggleSwitch.qml
        qml/controls/PatternColorButton.qml
        qml/controls/DpadDirectionControl.qml
        qml/controls/DirectionRosette.qml
        qml/controls/ImagePreview.qml
        qml/controls/CurveColorPopup.qml
        qml/controls/MonitorDirectionDialog.qml
        qml/controls/ConcentricLayerRow.qml
        qml/controls/StripelineLayerRow.qml
        qml/controls/CaliResultRow.qml
        qml/controls/HistogramPlotView.qml
        qml/controls/IhRangeControl.qml
        qml/controls/AxisReadout.qml
        qml/controls/AxisStateReadout.qml
        qml/controls/SectionFrame.qml
        qml/controls/PanelPlaceholder.qml

    RESOURCES
        assets/sample_shot.png

    SOURCES
        src/Bridge.cpp src/Bridge.h
        src/ServerProbe.cpp src/ServerProbe.h
        src/RosServerProbe.cpp src/RosServerProbe.h
        src/AxisState.cpp src/AxisState.h
        src/AxisController.cpp src/AxisController.h
)


if(FISHEYE_ENABLE_SUBAPPS)

    add_library(subapp_3d_verification STATIC
        subapp_3d_verification/app/Help.cpp
        subapp_3d_verification/controllers/controller_auto_3d_measurement.cpp
        subapp_3d_verification/core/measure3d/AnypointChessboard.cpp
        subapp_3d_verification/core/measure3d/CbDetect.cpp
        subapp_3d_verification/core/measure3d/Measure3dSession.cpp
        subapp_3d_verification/core/measure3d/Measure3dTypes.cpp
        subapp_3d_verification/core/measure3d/moil_3d_algorithm.cpp
        subapp_3d_verification/core/measure3d/PanelCornerRecovery.cpp
        subapp_3d_verification/core/measure3d/PlaneFit3dViz.cpp
        subapp_3d_verification/core/moildev/Moildev.cpp
        subapp_3d_verification/views/Point3dGlView.cpp
    )


    target_include_directories(subapp_3d_verification PUBLIC
        subapp_3d_verification/app
        subapp_3d_verification/controllers
        subapp_3d_verification/core/measure3d
        subapp_3d_verification/core/moildev
        subapp_3d_verification/views
        subapp_3d_verification/views/widgets
        ${OpenCV_INCLUDE_DIRS}
    )


    target_compile_definitions(subapp_3d_verification PRIVATE _USE_MATH_DEFINES NOMINMAX)

    target_link_libraries(subapp_3d_verification PUBLIC
        Qt6::Widgets Qt6::Concurrent Qt6::OpenGLWidgets
        ${OpenCV_LIBS} Eigen3::Eigen
    
        $<$<PLATFORM_ID:Windows>:opengl32>
    )

    # ---- Center Setup sub-app, copied from v2.0_2026_main-cpp-ros ---------------

    add_library(subapp_center_setup STATIC
        subapp_center_setup/controllers/controller_center_setup.cpp
        subapp_center_setup/ui/center_setup.ui
    )

    # AUTOUIC looks for the .ui next to the source that includes its header, and
    # controller_center_setup.cpp lives in controllers/ while center_setup.ui lives
    # in ui/. AUTOUIC_SEARCH_PATHS is what bridges that -- the old repo does the
    # same thing globally with CMAKE_AUTOUIC_SEARCH_PATHS.
    set_target_properties(subapp_center_setup PROPERTIES
        AUTOUIC ON
        AUTOUIC_SEARCH_PATHS "${CMAKE_CURRENT_SOURCE_DIR}/subapp_center_setup/ui"
    )

    target_include_directories(subapp_center_setup PUBLIC
        subapp_center_setup/controllers
    )

    target_compile_definitions(subapp_center_setup PRIVATE _USE_MATH_DEFINES NOMINMAX)


    target_link_libraries(subapp_center_setup PUBLIC subapp_3d_verification)

endif()


target_include_directories(moil_fisheye_cali PRIVATE src)

target_link_libraries(moil_fisheye_cali PRIVATE
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::Network
)

if(FISHEYE_ENABLE_SUBAPPS)
    target_link_libraries(moil_fisheye_cali PRIVATE
        subapp_3d_verification
        subapp_center_setup
    )
    target_compile_definitions(moil_fisheye_cali PRIVATE FISHEYE_SUBAPPS_ENABLED)
endif()

if(FISHEYE_ENABLE_ROS)
    target_link_libraries(moil_fisheye_cali PRIVATE
        rclcpp::rclcpp
        ${moil_interfaces_TARGETS}
    )
    target_compile_definitions(moil_fisheye_cali PRIVATE FISHEYE_ROS_ENABLED)

    # Use RPATH (not RUNPATH) so transitive deps like
    # libmoil_interfaces__rosidl_generator_c.so resolve without sourcing
    # the ROS overlay at runtime.
    #
    # ELF-only: this is a GNU ld flag. MSVC's link.exe rejects it outright, so
    # it must be gated on the platform and not just on FISHEYE_ENABLE_ROS --
    # the Windows ROS build (ROS 2 Lyrical, see README section 5) turns the
    # option on too, and resolves its DLLs through PATH rather than an RPATH.
    if(UNIX AND NOT APPLE)
        target_link_options(moil_fisheye_cali PRIVATE "LINKER:--disable-new-dtags")
    endif()
endif()

# ---- Configuration summary -------------------------------------------------
#
# Only Qt6 Quick/QuickControls2/Network are mandatory. Everything below is
# optional, so a machine with none of it still gets a browsable UI.

if(FISHEYE_ENABLE_SUBAPPS)
    set(FISHEYE_SUMMARY_SUBAPPS "yes (OpenCV ${OpenCV_VERSION})")
else()
    set(FISHEYE_SUMMARY_SUBAPPS "no, 3D Verification and Center Setup are unavailable")
endif()

if(FISHEYE_ENABLE_ROS)
    set(FISHEYE_SUMMARY_ROS "yes")
else()
    set(FISHEYE_SUMMARY_ROS "no, the rig controls are inert")
endif()

message(STATUS "")
message(STATUS "  moil_fisheye_cali configuration")
message(STATUS "    QML UI ............ yes")
message(STATUS "    QWidget sub-apps .. ${FISHEYE_SUMMARY_SUBAPPS}")
message(STATUS "    ROS backend ....... ${FISHEYE_SUMMARY_ROS}")
message(STATUS "")
