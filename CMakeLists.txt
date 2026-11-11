cmake_minimum_required(VERSION 3.16)
project(neko-kernel-manager LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Quick Qml Concurrent)

set(SOURCES
    src/main.cpp
    src/kernel.cpp
    src/utils.cpp
    src/kernel-bridge.cpp
    src/theme-manager.cpp
    resources.qrc
)

add_executable(neko-kernel-manager ${SOURCES})

target_link_libraries(neko-kernel-manager PRIVATE
    Qt6::Widgets
    Qt6::Quick
    Qt6::Qml
    Qt6::Concurrent
)

include(GNUInstallDirs)
install(TARGETS neko-kernel-manager DESTINATION ${CMAKE_INSTALL_BINDIR})
install(PROGRAMS helper/neko-kernel-helper DESTINATION ${CMAKE_INSTALL_LIBEXECDIR}/neko-kernel-manager)
install(FILES policy/org.nekovoid.NekoKernelManager.policy DESTINATION ${CMAKE_INSTALL_DATADIR}/polkit-1/actions)
install(FILES neko-kernel-manager.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
install(FILES Data/logo.png DESTINATION ${CMAKE_INSTALL_DATADIR}/pixmaps RENAME neko-kernel-manager.png)

