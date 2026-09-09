cmake_minimum_required(VERSION 3.22)

project(kadunce_control VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(ECM 6.26 REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDECompilerSettings NO_POLICY_SCOPE)

find_package(Qt6 6.10 REQUIRED COMPONENTS Core Gui Widgets)
find_package(KF6 6.26 REQUIRED COMPONENTS Config StatusNotifierItem)

qt_add_executable(kadunce-control
    src/main.cpp
)

qt_add_resources(kadunce-control "control_icons"
    PREFIX "/icons"
    FILES
        assets/kadunce-enabled.svg
        assets/kadunce-disabled.svg
)

target_link_libraries(kadunce-control PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    KF6::ConfigCore
    KF6::StatusNotifierItem
)

install(TARGETS kadunce-control RUNTIME DESTINATION ${KDE_INSTALL_BINDIR})
