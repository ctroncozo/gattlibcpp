# Find dependencies
find_package(GTest REQUIRED)
find_package(fmt REQUIRED)
find_package(spdlog REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_search_module(PCRE REQUIRED libpcre)
pkg_search_module(GLIB REQUIRED glib-2.0)

# Create test executable
add_executable(gattlib_cpp_tests
  gattlib_tests.cpp
  mock_gattlib.cpp
)

add_executable(gmain_loop_test
  gmain_loop_tests.cpp
)

target_link_libraries(gattlib_cpp_tests
  PRIVATE
    GTest::GTest
    GTest::Main
    GTest::gmock
    GTest::gmock_main
    gattlib_cpp
    fmt::fmt
    spdlog::spdlog
    ${PCRE_LIBRARIES}
    ${GLIB_LIBRARIES}
)

target_link_libraries(gmain_loop_test
  PRIVATE
    GTest::GTest
    GTest::Main
    GTest::gmock
    GTest::gmock_main
    gattlib_cpp
    fmt::fmt
    spdlog::spdlog
    ${PCRE_LIBRARIES}
    ${GLIB_LIBRARIES}
)

target_include_directories(gattlib_cpp_tests PRIVATE
  ${CMAKE_SOURCE_DIR}/cpp/include
  ${CMAKE_SOURCE_DIR}/include
  ${CMAKE_SOURCE_DIR}/src
  ${GLIB_INCLUDE_DIRS}
)

# Add test to CTest
add_test(NAME gattlib_cpp_tests COMMAND gattlib_cpp_tests)
add_test(NAME gmain_loop_test COMMAND gmain_loop_test)
