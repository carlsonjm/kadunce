cmake_minimum_required(VERSION 3.22)

project(kadunce_native VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(ECM 6.26 REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

include(KDEInstallDirs)
install(FILES ../assets/studio.warbler.kadunce-logo.png
    DESTINATION ${KDE_INSTALL_ICONDIR}/hicolor/512x512/apps)
include(KDECMakeSettings)
include(KDECompilerSettings NO_POLICY_SCOPE)

find_package(Qt6 6.10 REQUIRED COMPONENTS Core DBus Gui Widgets)
find_package(KF6 6.26 REQUIRED COMPONENTS CoreAddons GlobalAccel Config)
find_package(KWin 6.7 REQUIRED COMPONENTS kwineffects)

kcoreaddons_add_plugin(kwin4_effect_kadunce
    SOURCES
        src/CardLineModel.cpp
        src/CardLineModel.h
        src/CardLineLayout.cpp
        src/CardLineLayout.h
        src/CardStageController.cpp
        src/CardStageController.h
        src/BentoLayout.cpp
        src/BentoLayout.h
        src/DesktopStageController.cpp
        src/DesktopStageController.h
        src/WorkspaceInputRouter.cpp
        src/WorkspaceInputRouter.h
        src/Effect.cpp
        src/Effect.h
        src/plugin.cpp
    INSTALL_NAMESPACE "kwin/effects/plugins/"
)

target_link_libraries(kwin4_effect_kadunce
    Qt6::Core
    Qt6::DBus
    Qt6::Gui
    Qt6::Widgets
    KF6::GlobalAccel
    KF6::ConfigCore
    KWin::kwin
)

include(CTest)
if(BUILD_TESTING)
    add_executable(kadunce-panel-input-test tests/PanelInputTest.cpp
        src/WorkspaceInputRouter.cpp src/CardLineLayout.cpp)
    target_include_directories(kadunce-panel-input-test PRIVATE src)
    target_link_libraries(kadunce-panel-input-test PRIVATE KWin::kwin Qt6::Core)
    add_test(NAME panel-input COMMAND kadunce-panel-input-test)
    find_package(Qt6 REQUIRED COMPONENTS Test)
    find_program(DBUS_RUN_SESSION dbus-run-session REQUIRED)
    add_executable(kadunce-window-handling-test tests/WindowHandlingTest.cpp)
    target_include_directories(kadunce-window-handling-test PRIVATE src)
    target_link_libraries(kadunce-window-handling-test PRIVATE KWin::kwin KF6::ConfigCore Qt6::Test)
    add_test(NAME window-handling COMMAND ${DBUS_RUN_SESSION} -- $<TARGET_FILE:kadunce-window-handling-test>)
    add_executable(kadunce-launch-identity-test tests/LaunchIdentityTest.cpp)
    target_include_directories(kadunce-launch-identity-test PRIVATE src)
    target_link_libraries(kadunce-launch-identity-test PRIVATE Qt6::Core)
    add_test(NAME launch-identity COMMAND kadunce-launch-identity-test)
    add_executable(kadunce-card-line-model-test
        tests/CardLineModelTest.cpp
        src/CardLineModel.cpp
    )
    target_include_directories(kadunce-card-line-model-test PRIVATE src)
    add_test(NAME card-line-model COMMAND kadunce-card-line-model-test)

    add_executable(kadunce-card-line-layout-test
        tests/CardLineLayoutTest.cpp
        src/CardLineLayout.cpp
    )
    target_include_directories(kadunce-card-line-layout-test PRIVATE src)
    add_test(NAME card-line-layout COMMAND kadunce-card-line-layout-test)

    add_executable(kadunce-bento-layout-test
        tests/BentoLayoutTest.cpp
        src/BentoLayout.cpp
    )
    target_include_directories(kadunce-bento-layout-test PRIVATE src)
    add_test(NAME bento-layout COMMAND kadunce-bento-layout-test)
endif()
