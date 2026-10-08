cmake_minimum_required(VERSION 3.22)

project(kadunce_native VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(ECM 6.26 REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

include(KDEInstallDirs)
install(FILES ../assets/co.goodinput.kadunce-logo.png
    DESTINATION ${KDE_INSTALL_ICONDIR}/hicolor/512x512/apps)
include(KDECMakeSettings)
include(KDECompilerSettings NO_POLICY_SCOPE)

find_package(Qt6 6.10 REQUIRED COMPONENTS Core DBus Gui GuiPrivate Widgets Quick)
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
        src/GestureNote.h
        src/GestureNoteLabel.h
        src/StuckNotes.h
        src/StuckNotesWatcher.cpp
        src/StuckNotesWatcher.h
        src/NoteStackRenderer.h
        src/CardStageController.cpp
        src/CardStageController.h
        src/BentoLayout.cpp
        src/BentoLayout.h
        src/DesktopStageController.cpp
        src/DesktopZones.cpp
        src/DesktopStageController.h
        src/WorkspaceInputRouter.cpp
        src/WorkspaceInputRouter.h
        src/Effect.cpp
        src/Effect.h
        src/PaintResult.h
        src/TableGesture.h
        src/TableLayout.h
        src/TablePresenter.cpp
        src/TablePresenter.h
        src/plugin.cpp
    INSTALL_NAMESPACE "kwin/effects/plugins/"
)

qt_add_resources(kwin4_effect_kadunce "kadunce_table"
    PREFIX "/kadunce"
    BASE src
    FILES src/table/Table.qml
)

target_link_libraries(kwin4_effect_kadunce
    PRIVATE KDecoration3::KDecoration
    XCB::XCB
    Qt6::Core
    Qt6::DBus
    Qt6::Gui
    Qt6::GuiPrivate
    Qt6::Widgets
    Qt6::Quick
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
    add_executable(kadunce-table-gesture-test tests/TableGestureTest.cpp)
    target_include_directories(kadunce-table-gesture-test PRIVATE src)
    target_link_libraries(kadunce-table-gesture-test PRIVATE Qt6::Core)
    add_test(NAME table-gesture COMMAND kadunce-table-gesture-test)
    add_executable(kadunce-table-layout-test tests/TableLayoutTest.cpp)
    target_include_directories(kadunce-table-layout-test PRIVATE src)
    target_link_libraries(kadunce-table-layout-test PRIVATE Qt6::Core)
    add_test(NAME table-layout COMMAND kadunce-table-layout-test)
    add_executable(kadunce-row-page-motion-test tests/RowPageMotionTest.cpp)
    target_include_directories(kadunce-row-page-motion-test PRIVATE src)
    target_link_libraries(kadunce-row-page-motion-test PRIVATE Qt6::Core)
    add_test(NAME row-page-motion COMMAND kadunce-row-page-motion-test)
    add_executable(kadunce-held-tuck-test tests/HeldTuckTest.cpp)
    target_include_directories(kadunce-held-tuck-test PRIVATE src)
    target_compile_options(kadunce-held-tuck-test PRIVATE -UNDEBUG)
    add_test(NAME held-tuck COMMAND kadunce-held-tuck-test)
    add_executable(kadunce-held-pointer-test tests/HeldPointerTest.cpp)
    target_include_directories(kadunce-held-pointer-test PRIVATE src)
    target_link_libraries(kadunce-held-pointer-test PRIVATE Qt6::Core)
    target_compile_options(kadunce-held-pointer-test PRIVATE -UNDEBUG)
    add_test(NAME held-pointer COMMAND kadunce-held-pointer-test)
    add_executable(kadunce-active-step-test tests/ActiveStepTest.cpp)
    target_include_directories(kadunce-active-step-test PRIVATE src)
    target_compile_options(kadunce-active-step-test PRIVATE -UNDEBUG)
    add_test(NAME active-step COMMAND kadunce-active-step-test)
    add_executable(kadunce-row-travel-test tests/RowTravelTest.cpp)
    target_include_directories(kadunce-row-travel-test PRIVATE src)
    target_compile_options(kadunce-row-travel-test PRIVATE -UNDEBUG)
    add_test(NAME row-travel COMMAND kadunce-row-travel-test)
    add_executable(kadunce-pane-arrival-test tests/PaneArrivalTest.cpp)
    target_include_directories(kadunce-pane-arrival-test PRIVATE src)
    target_compile_options(kadunce-pane-arrival-test PRIVATE -UNDEBUG)
    add_test(NAME pane-arrival COMMAND kadunce-pane-arrival-test)
    add_executable(kadunce-carry-row-test tests/CarryRowTest.cpp)
    target_include_directories(kadunce-carry-row-test PRIVATE src)
    target_compile_options(kadunce-carry-row-test PRIVATE -UNDEBUG)
    add_test(NAME carry-row COMMAND kadunce-carry-row-test)
    add_executable(kadunce-keys-test tests/KadunceKeysTest.cpp)
    target_include_directories(kadunce-keys-test PRIVATE src)
    target_link_libraries(kadunce-keys-test PRIVATE Qt6::Core)
    target_compile_options(kadunce-keys-test PRIVATE -UNDEBUG)
    add_test(NAME kadunce-keys COMMAND kadunce-keys-test)
    add_executable(kadunce-stack-browse-motion-test tests/StackBrowseMotionTest.cpp)
    target_include_directories(kadunce-stack-browse-motion-test PRIVATE src)
    add_test(NAME stack-browse-motion COMMAND kadunce-stack-browse-motion-test)
    add_executable(kadunce-motion-time-test tests/MotionTimeTest.cpp)
    target_include_directories(kadunce-motion-time-test PRIVATE src)
    add_test(NAME motion-time COMMAND kadunce-motion-time-test)
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
    add_executable(kadunce-deliberate-edge-entry-test tests/DeliberateEdgeEntryTest.cpp)
    target_include_directories(kadunce-deliberate-edge-entry-test PRIVATE src)
    target_link_libraries(kadunce-deliberate-edge-entry-test PRIVATE Qt6::Core)
    add_test(NAME deliberate-edge-entry COMMAND kadunce-deliberate-edge-entry-test)
    add_executable(kadunce-gesture-note-test tests/GestureNoteTest.cpp)
    target_include_directories(kadunce-gesture-note-test PRIVATE src)
    target_link_libraries(kadunce-gesture-note-test PRIVATE Qt6::Core)
    add_test(NAME gesture-note COMMAND kadunce-gesture-note-test)
    add_executable(kadunce-stuck-notes-test tests/StuckNotesTest.cpp)
    target_include_directories(kadunce-stuck-notes-test PRIVATE src)
    target_link_libraries(kadunce-stuck-notes-test PRIVATE Qt6::Core)
    target_compile_options(kadunce-stuck-notes-test PRIVATE -UNDEBUG)
    add_test(NAME stuck-notes COMMAND kadunce-stuck-notes-test)
    add_executable(kadunce-touch-display-test tests/TouchDisplayTest.cpp)
    target_include_directories(kadunce-touch-display-test PRIVATE src)
    target_link_libraries(kadunce-touch-display-test PRIVATE Qt6::Core)
    add_test(NAME touch-display COMMAND kadunce-touch-display-test)
    add_executable(kadunce-card-ownership-test tests/CardOwnershipTest.cpp)
    target_include_directories(kadunce-card-ownership-test PRIVATE src)
    target_link_libraries(kadunce-card-ownership-test PRIVATE Qt6::Core)
    add_test(NAME card-ownership COMMAND kadunce-card-ownership-test)
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
    add_executable(kadunce-placement-aim-test tests/PlacementAimTest.cpp)
    target_include_directories(kadunce-placement-aim-test PRIVATE src)
    target_link_libraries(kadunce-placement-aim-test PRIVATE Qt6::Core)
    add_test(NAME placement-aim COMMAND kadunce-placement-aim-test)
    add_executable(kadunce-pending-launch-test tests/PendingLaunchTest.cpp)
    target_include_directories(kadunce-pending-launch-test PRIVATE src)
    target_link_libraries(kadunce-pending-launch-test PRIVATE Qt6::Core)
    add_test(NAME pending-launch COMMAND kadunce-pending-launch-test)
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
