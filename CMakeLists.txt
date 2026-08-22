cmake_minimum_required(VERSION 3.16)

# Must be set before project() to take effect. Only defaults it if the user
# (or a toolchain file) hasn't already picked one - e.g. via
# -DCMAKE_OSX_DEPLOYMENT_TARGET=... on the cmake command line.
if(APPLE AND NOT DEFINED CMAKE_OSX_DEPLOYMENT_TARGET)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "12.0" CACHE STRING "Minimum macOS version to support" FORCE)
endif()

project(PhotosByLarry LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Concurrent)
find_package(OpenCV REQUIRED COMPONENTS core imgproc imgcodecs)

# src/core/ - UI-independent model and processing logic (Qt Core + OpenCV,
# no Qt Widgets) - split into its own static library so tests/ can link
# against just this, without pulling in Widgets or the app's main().
set(CORE_SOURCES
    src/core/EditParameters.cpp
    src/core/ImageConversion.cpp
    src/core/ImageProcessor.cpp
    src/core/Photo.cpp
    src/core/PhotoLibrary.cpp
)

set(CORE_HEADERS
    src/core/EditParameters.h
    src/core/ImageConversion.h
    src/core/ImageProcessor.h
    src/core/Photo.h
    src/core/PhotoLibrary.h
)

add_library(PhotosByLarryCore STATIC ${CORE_SOURCES} ${CORE_HEADERS})

target_include_directories(PhotosByLarryCore PUBLIC
    src
    ${OpenCV_INCLUDE_DIRS}
)

target_link_libraries(PhotosByLarryCore PUBLIC
    Qt6::Core
    Qt6::Gui # ImageConversion.h uses QImage, which lives in Gui, not Core
    ${OpenCV_LIBS}
)

set(APP_SOURCES
    src/main.cpp

    src/ui/MainWindow.cpp
    src/ui/ThumbnailModel.cpp
    src/ui/ThumbnailPanel.cpp
    src/ui/ImageViewer.cpp
    src/ui/AdjustmentsPanel.cpp
    src/ui/CollapsibleSection.cpp
    src/ui/CropOverlayItem.cpp
)

set(APP_HEADERS
    src/ui/MainWindow.h
    src/ui/ThumbnailModel.h
    src/ui/ThumbnailPanel.h
    src/ui/ImageViewer.h
    src/ui/AdjustmentsPanel.h
    src/ui/CollapsibleSection.h
    src/ui/CropOverlayItem.h
)

# MACOSX_BUNDLE only takes effect on Apple platforms (a real .app bundle
# instead of a bare executable); it's a silent no-op everywhere else, so
# there's no need to condition it on APPLE.
add_executable(PhotosByLarry MACOSX_BUNDLE ${APP_SOURCES} ${APP_HEADERS})

if(APPLE)
    # Fills in the bundle's Info.plist. No custom app icon yet - add an
    # .icns file, set MACOSX_BUNDLE_ICON_FILE to its name, and add it to the
    # add_executable() sources with MACOSX_PACKAGE_LOCATION "Resources" to
    # give the bundle one.
    set_target_properties(PhotosByLarry PROPERTIES
        MACOSX_BUNDLE_BUNDLE_NAME "Photos by Larry"
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.example.photosbylarry"
        MACOSX_BUNDLE_BUNDLE_VERSION "1.0"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "1.0"
    )
endif()

target_include_directories(PhotosByLarry PRIVATE
    src
    ${OpenCV_INCLUDE_DIRS}
)

target_link_libraries(PhotosByLarry PRIVATE
    PhotosByLarryCore
    Qt6::Widgets
    Qt6::Concurrent
    ${OpenCV_LIBS}
)

# Tests build against PhotosByLarryCore only (src/core/, no Qt Widgets, no
# app main()). Off switch for anyone who wants a plain app build with no
# network dependency - GoogleTest isn't reliably available as a system
# package, so it's fetched via CMake's FetchContent instead.
option(BUILD_TESTS "Build the GoogleTest-based test suite" ON)

if(BUILD_TESTS)
    enable_testing()

    include(FetchContent)
    if(POLICY CMP0135)
        cmake_policy(SET CMP0135 NEW) # timestamp extracted files at extraction time, not from the archive
    endif()
    FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/refs/tags/v1.17.0.zip
    )
    # Don't let GoogleTest's own install() rules tangle with this project's.
    set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)

    add_subdirectory(tests)
endif()
