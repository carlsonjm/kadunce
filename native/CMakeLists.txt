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
find_package(KF6 6.26 REQUIRED COMPONENTS CoreAddons GlobalAccel Config Service)
find_package(KWin 6.7 REQUIRED COMPONENTS kwineffects)
find_package(KDecoration3 REQUIRED)
find_package(XCB REQUIRED COMPONENTS XCB)

kcoreaddons_add_plugin(kwin4_effect_kadunce
    SOURCES
        src/SpreadModel.cpp
        src/SpreadModel.h
        src/SpreadLayout.cpp
        src/SpreadLayout.h
        src/ProjectedCardGeometry.h
        src/BentoCompositeGeometry.h
        src/BentoProjectionSession.h
        src/CardLabelPresentation.h
        src/CardLabelRenderer.h
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
    PRIVATE KDecoration3::KDecoration
    XCB::XCB
    Qt6::Core
    Qt6::DBus
    Qt6::Gui
    Qt6::Widgets
    KF6::GlobalAccel
    KF6::ConfigCore
    KF6::Service
    KWin::kwin
)

include(CTest)
if(BUILD_TESTING)
    add_executable(kadunce-card-label-presentation-test
        tests/CardLabelPresentationTest.cpp)
    target_include_directories(kadunce-card-label-presentation-test PRIVATE src)
    target_link_libraries(kadunce-card-label-presentation-test PRIVATE Qt6::Core)
    add_test(NAME card-label-presentation COMMAND kadunce-card-label-presentation-test)
    add_executable(kadunce-row-page-motion-test tests/RowPageMotionTest.cpp)
    target_include_directories(kadunce-row-page-motion-test PRIVATE src)
    target_link_libraries(kadunce-row-page-motion-test PRIVATE Qt6::Core)
    add_test(NAME row-page-motion COMMAND kadunce-row-page-motion-test)
    add_executable(kadunce-stack-browse-motion-test tests/StackBrowseMotionTest.cpp)
    target_include_directories(kadunce-stack-browse-motion-test PRIVATE src)
    add_test(NAME stack-browse-motion COMMAND kadunce-stack-browse-motion-test)
    add_executable(kadunce-carry-paint-test tests/CarryPaintPlanTest.cpp)
    target_include_directories(kadunce-carry-paint-test PRIVATE src)
    target_link_libraries(kadunce-carry-paint-test PRIVATE Qt6::Core)
    target_compile_options(kadunce-carry-paint-test PRIVATE -UNDEBUG)
    add_test(NAME carry-paint COMMAND kadunce-carry-paint-test)
    add_executable(kadunce-carry-input-route-test tests/CarryInputRouteTest.cpp)
    target_include_directories(kadunce-carry-input-route-test PRIVATE src)
    target_link_libraries(kadunce-carry-input-route-test PRIVATE Qt6::Core)
    add_test(NAME carry-input-route COMMAND kadunce-carry-input-route-test)
    add_executable(kadunce-carry-contacts-test tests/CarryContactsTest.cpp)
    target_include_directories(kadunce-carry-contacts-test PRIVATE src)
    target_link_libraries(kadunce-carry-contacts-test PRIVATE Qt6::Core)
    add_test(NAME carry-contacts COMMAND kadunce-carry-contacts-test)
    add_executable(kadunce-bento-transfer-test tests/BentoSessionTransferTest.cpp src/BentoLayout.cpp)
    target_include_directories(kadunce-bento-transfer-test PRIVATE src)
    target_link_libraries(kadunce-bento-transfer-test PRIVATE Qt6::Core)
    add_test(NAME bento-transfer COMMAND kadunce-bento-transfer-test)
    add_executable(kadunce-carry-session-test tests/CarrySessionTest.cpp)
    target_include_directories(kadunce-carry-session-test PRIVATE src)
    target_link_libraries(kadunce-carry-session-test PRIVATE Qt6::Core)
    add_test(NAME carry-session COMMAND kadunce-carry-session-test)
    add_executable(kadunce-workspace-state-test
        tests/WorkspaceStateTest.cpp src/SpreadModel.cpp)
    target_include_directories(kadunce-workspace-state-test PRIVATE src)
    target_link_libraries(kadunce-workspace-state-test PRIVATE Qt6::Core)
    add_test(NAME workspace-state COMMAND kadunce-workspace-state-test)
    add_executable(kadunce-ownership-handoff-test
        tests/OwnershipHandoffTest.cpp src/SpreadModel.cpp)
    target_include_directories(kadunce-ownership-handoff-test PRIVATE src)
    target_link_libraries(kadunce-ownership-handoff-test PRIVATE Qt6::Core)
    add_test(NAME ownership-handoff COMMAND kadunce-ownership-handoff-test)
    add_executable(kadunce-workspace-snapshot-test
        tests/WorkspaceSnapshotTest.cpp src/SpreadModel.cpp)
    target_include_directories(kadunce-workspace-snapshot-test PRIVATE src)
    target_link_libraries(kadunce-workspace-snapshot-test PRIVATE Qt6::Core)
    add_test(NAME workspace-snapshot COMMAND kadunce-workspace-snapshot-test)
    add_executable(kadunce-panel-input-test tests/PanelInputTest.cpp
        src/WorkspaceInputRouter.cpp src/SpreadLayout.cpp src/SpreadModel.cpp)
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
    add_executable(kadunce-spread-model-test
        tests/SpreadModelTest.cpp
        src/SpreadModel.cpp
    )
    target_include_directories(kadunce-spread-model-test PRIVATE src)
    add_test(NAME spread-model COMMAND kadunce-spread-model-test)

    add_executable(kadunce-spread-layout-test
        tests/SpreadLayoutTest.cpp
        src/SpreadLayout.cpp
    )
    target_include_directories(kadunce-spread-layout-test PRIVATE src)
    add_test(NAME spread-layout COMMAND kadunce-spread-layout-test)

    add_executable(kadunce-projected-card-geometry-test
        tests/ProjectedCardGeometryTest.cpp
        src/SpreadLayout.cpp
    )
    target_include_directories(kadunce-projected-card-geometry-test PRIVATE src)
    add_test(NAME projected-card-geometry COMMAND kadunce-projected-card-geometry-test)

    add_executable(kadunce-bento-composite-geometry-test
        tests/BentoCompositeGeometryTest.cpp
        src/BentoLayout.cpp
        src/SpreadLayout.cpp
    )
    target_include_directories(kadunce-bento-composite-geometry-test PRIVATE src)
    add_test(NAME bento-composite-geometry COMMAND kadunce-bento-composite-geometry-test)

    add_executable(kadunce-bento-projection-session-test
        tests/BentoProjectionSessionTest.cpp
    )
    target_include_directories(kadunce-bento-projection-session-test PRIVATE src)
    target_link_libraries(kadunce-bento-projection-session-test PRIVATE KWin::kwin Qt6::Core)
    add_test(NAME bento-projection-session COMMAND kadunce-bento-projection-session-test)

    add_executable(kadunce-bento-layout-test
        tests/BentoLayoutTest.cpp
        src/BentoLayout.cpp
    )
    target_include_directories(kadunce-bento-layout-test PRIVATE src)
    add_test(NAME bento-layout COMMAND kadunce-bento-layout-test)
endif()
