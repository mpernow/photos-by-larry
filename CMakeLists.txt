cmake_minimum_required(VERSION 3.16)
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
)

add_executable(PhotosByLarry ${APP_SOURCES} ${APP_HEADERS})

target_include_directories(PhotosByLarry PRIVATE
    src
    ${OpenCV_INCLUDE_DIRS}
)

target_link_libraries(PhotosByLarry PRIVATE
    Qt6::Widgets
    Qt6::Concurrent
    ${OpenCV_LIBS}
)
