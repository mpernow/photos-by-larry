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

find_package(Qt6 REQUIRED COMPONENTS Widgets Concurrent)
find_package(OpenCV REQUIRED COMPONENTS core imgproc imgcodecs)

set(APP_SOURCES
    src/main.cpp

    src/core/EditParameters.cpp
    src/core/ImageConversion.cpp
    src/core/ImageProcessor.cpp
    src/core/Photo.cpp
    src/core/PhotoLibrary.cpp

    src/ui/MainWindow.cpp
    src/ui/ThumbnailModel.cpp
    src/ui/ThumbnailPanel.cpp
    src/ui/ImageViewer.cpp
    src/ui/AdjustmentsPanel.cpp
    src/ui/CropOverlayItem.cpp
)

set(APP_HEADERS
    src/core/EditParameters.h
    src/core/ImageConversion.h
    src/core/ImageProcessor.h
    src/core/Photo.h
    src/core/PhotoLibrary.h

    src/ui/MainWindow.h
    src/ui/ThumbnailModel.h
    src/ui/ThumbnailPanel.h
    src/ui/ImageViewer.h
    src/ui/AdjustmentsPanel.h
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
    Qt6::Widgets
    Qt6::Concurrent
    ${OpenCV_LIBS}
)
