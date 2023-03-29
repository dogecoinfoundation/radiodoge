# CMakeList.txt : CMake project for serdog, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.8)

project ("serdog")

# Add source to this project's executable.
add_executable (serdog "serdog.c" "serdog.h" "rdas.h" "radioDogeTypes.h" "rdasutils.h" "libdogecoin.h")

target_link_libraries(serdog "${CMAKE_SOURCE_DIR}/libdogecoin.a")

target_include_directories(serdog PUBLIC "${CMAKE_SOURCE_DIR}/include")

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET serdog PROPERTY CXX_STANDARD 20)
endif()

# TODO: Add tests and install targets if needed.
