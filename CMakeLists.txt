cmake_minimum_required(VERSION 3.16)
project(FileSystemRecoveryTool VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find Qt6
find_package(Qt6 REQUIRED COMPONENTS Core Widgets Charts)

# Auto-generate MOC, UIC, and RCC
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/include)

# Source files
set(CORE_SOURCES
    src/VirtualDisk.cpp
    src/Inode.cpp
    src/Directory.cpp
    src/FileSystem.cpp
)

set(RECOVERY_SOURCES
    src/Journal.cpp
    src/RecoveryManager.cpp
    src/DefragManager.cpp
)

set(UI_SOURCES
    src/MainWindow.cpp
    src/BlockMapWidget.cpp
    src/PerformanceWidget.cpp
    src/ControlPanel.cpp
    src/FileBrowserWidget.cpp
    src/main.cpp
)

set(HEADER_FILES
    include/VirtualDisk.h
    include/Inode.h
    include/Directory.h
    include/FileSystem.h
    include/Journal.h
    include/RecoveryManager.h
    include/DefragManager.h
    include/MainWindow.h
    include/BlockMapWidget.h
    include/PerformanceWidget.h
    include/ControlPanel.h
    include/FileBrowserWidget.h
)

# Executable target
add_executable(file_system_tool
    ${CORE_SOURCES}
    ${RECOVERY_SOURCES}
    ${UI_SOURCES}
    ${HEADER_FILES}
)

# Link Qt libraries
target_link_libraries(file_system_tool
    Qt6::Core
    Qt6::Widgets
    Qt6::Charts
)

# Platform-specific settings
if(APPLE)
    set_target_properties(file_system_tool PROPERTIES
        MACOSX_BUNDLE TRUE
    )
endif()

# Installation
install(TARGETS file_system_tool
    BUNDLE DESTINATION .
    RUNTIME DESTINATION bin
)
