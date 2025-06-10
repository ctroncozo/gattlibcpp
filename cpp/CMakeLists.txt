cmake_minimum_required(VERSION 3.10)

project(gattlib_cpp)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find required packages
find_package(PkgConfig REQUIRED)
pkg_check_modules(GLIB REQUIRED glib-2.0)
find_package(spdlog REQUIRED)

# Set root directory for gattlib
set(GATTLIB_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/..")

# Add gattlib include directory
set(GATTLIB_INCLUDE_DIRS ${GATTLIB_ROOT_DIR}/include)
set(GATTLIB_LIBRARIES ${GATTLIB_ROOT_DIR}/build/dbus/libgattlib.so)

# Create library target
add_library(gattlib_cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/gattlib_scanner_impl.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/gmain_loop_manager.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/gattlib_utils.cpp
)

# Add include directories
target_include_directories(gattlib_cpp PUBLIC
  ${CMAKE_SOURCE_DIR}/cpp/include
  ${CMAKE_SOURCE_DIR}/include
  ${CMAKE_SOURCE_DIR}/src
  ${GATTLIB_INCLUDE_DIRS}
  ${GLIB_INCLUDE_DIRS}
)

# Link libraries
target_link_libraries(gattlib_cpp PUBLIC
  ${GATTLIB_LIBRARIES}
  ${GLIB_LIBRARIES}
  spdlog::spdlog
)

# Enable testing
enable_testing()

# Add tests subdirectory
add_subdirectory(tests)
