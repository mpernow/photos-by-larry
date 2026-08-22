# Test suite for src/core/ - links PhotosByLarryCore directly, so these run
# as plain console binaries with no Qt Widgets/QApplication and no image
# files on disk beyond what individual tests create themselves in a
# QTemporaryDir.
add_executable(PhotosByLarryTests
    EditParametersTest.cpp
    ImageProcessorTest.cpp
    PhotoTest.cpp
)

target_link_libraries(PhotosByLarryTests PRIVATE
    PhotosByLarryCore
    GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(PhotosByLarryTests)
