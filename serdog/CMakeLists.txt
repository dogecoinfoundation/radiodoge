# CMakeList.txt : CMake project for serdog, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.8)

project ("serdog")

# Add source to this project's executable.
add_executable (serdog "serdog.c" "serdog.h" "rdas.h" "radioDogeTypes.h" "rdasutils.h" "libdogecoin.h" "consolehelper.h" "consolehelper.c" "fileHelper.h" "fileHelper.c")
set(CMAKE_THREAD_LIBS_INIT "-lpthread")
set(CMAKE_HAVE_THREADS_LIBRARY 1)
set(CMAKE_USE_WIN32_THREADS_INIT 0)
set(CMAKE_USE_PTHREADS_INIT 1)
set(THREADS_PREFER_PTHREAD_FLAG ON)
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -lunistring")
find_package(Threads REQUIRED)
target_link_libraries(serdog PRIVATE "${CMAKE_SOURCE_DIR}/libdogecoin.a" unistring Threads::Threads)

target_include_directories(serdog PUBLIC "${CMAKE_SOURCE_DIR}/include")

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET serdog PROPERTY CXX_STANDARD 20)
endif()

add_custom_command(
        TARGET serdog POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
                ${CMAKE_SOURCE_DIR}/asciiDoge.txt
                ${CMAKE_CURRENT_BINARY_DIR}/asciiDoge.txt)
# TODO: Add tests and install targets if needed.
