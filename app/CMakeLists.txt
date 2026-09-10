cmake_minimum_required(VERSION 3.24)
project(DevManager LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

# Qt prebuilts (msvc2022_64) are v143 ABI. Build with -T v143 (see CMakePresets.json).
find_package(Qt6 REQUIRED COMPONENTS Core Widgets Concurrent Network)

# ---- core library: pure QtCore, no Widgets, so it stays unit-testable ----
add_library(devmanager_core STATIC
    core/model.cpp
    core/process_runner.cpp
    core/path_util.cpp
    core/json_io.cpp
    core/fs_ops.cpp
    core/config/scan_config.cpp
    core/scan/skill_scanner.cpp
    core/scan/env_scanner.cpp
    core/service/service_probe.cpp
    core/service/service_lifecycle.cpp
    core/snapshot/snapshot_index.cpp
    core/snapshot/snapshot_preview.cpp
    core/snapshot/restore_preview.cpp
    core/snapshot/snapshot_executor.cpp
    core/snapshot/snapshot_coordinator.cpp
    core/snapshot/snapshot_verify.cpp
    core/snapshot/snapshot_diff.cpp
    core/snapshot/restore_executor.cpp
)
target_include_directories(devmanager_core PUBLIC core)
target_link_libraries(devmanager_core PUBLIC Qt6::Core Qt6::Network)

# ---- Phase 1 CLI: read-only environment scan -> inventory json ----
add_executable(devmanager-scan cli/main.cpp)
target_link_libraries(devmanager-scan PRIVATE devmanager_core)

# ---- GUI shell (Qt Widgets): dashboard over the read-only scan ----
add_executable(devmanager-gui WIN32
    gui/main.cpp
    gui/app_controller.cpp
    gui/main_window.cpp
    gui/theme.cpp
    gui/widgets/icons.cpp
    gui/widgets/status_badge.cpp
    gui/widgets/summary_card.cpp
    gui/widgets/sidebar.cpp
    gui/widgets/top_bar.cpp
    gui/widgets/segmented_control.cpp
    gui/widgets/machine_card.cpp
    gui/widgets/right_panel.cpp
    gui/widgets/tag_badge.cpp
    gui/pages/environment_page.cpp
    gui/pages/placeholder_page.cpp
    gui/pages/skills_page.cpp
    gui/pages/plugins_page.cpp
    gui/pages/packages_page.cpp
    gui/pages/env_vars_page.cpp
    gui/pages/snapshot_page.cpp
    gui/pages/restore_page.cpp
)
target_include_directories(devmanager-gui PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(devmanager-gui PRIVATE
    devmanager_core Qt6::Widgets Qt6::Concurrent)

# ---- tests ----
option(DEVMANAGER_BUILD_TESTS "Build unit tests" ON)
if(DEVMANAGER_BUILD_TESTS)
    find_package(Qt6 REQUIRED COMPONENTS Test)
    enable_testing()
    add_executable(tst_skill_scanner tests/tst_skill_scanner.cpp)
    target_link_libraries(tst_skill_scanner PRIVATE devmanager_core Qt6::Test)
    add_test(NAME skill_scanner COMMAND tst_skill_scanner)

    add_executable(tst_snapshot_executor tests/tst_snapshot_executor.cpp)
    target_link_libraries(tst_snapshot_executor PRIVATE devmanager_core Qt6::Test)
    add_test(NAME snapshot_executor COMMAND tst_snapshot_executor)

    add_executable(tst_service_lifecycle tests/tst_service_lifecycle.cpp)
    target_link_libraries(tst_service_lifecycle PRIVATE devmanager_core Qt6::Test)
    add_test(NAME service_lifecycle COMMAND tst_service_lifecycle)

    add_executable(tst_restore_executor tests/tst_restore_executor.cpp)
    target_link_libraries(tst_restore_executor PRIVATE devmanager_core Qt6::Test)
    add_test(NAME restore_executor COMMAND tst_restore_executor)

    add_executable(tst_snapshot_coordinator tests/tst_snapshot_coordinator.cpp)
    target_link_libraries(tst_snapshot_coordinator PRIVATE devmanager_core Qt6::Test)
    add_test(NAME snapshot_coordinator COMMAND tst_snapshot_coordinator)

    add_executable(tst_snapshot_verify tests/tst_snapshot_verify.cpp)
    target_link_libraries(tst_snapshot_verify PRIVATE devmanager_core Qt6::Test)
    add_test(NAME snapshot_verify COMMAND tst_snapshot_verify)

    add_executable(tst_snapshot_diff tests/tst_snapshot_diff.cpp)
    target_link_libraries(tst_snapshot_diff PRIVATE devmanager_core Qt6::Test)
    add_test(NAME snapshot_diff COMMAND tst_snapshot_diff)
endif()
