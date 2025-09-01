cmake_minimum_required(VERSION 3.21)
project(vpn_gui_clean LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

file(GLOB_RECURSE SRC_FILES "src/*.cpp")

add_executable(${PROJECT_NAME} ${SRC_FILES})

if (WIN32)
    target_link_libraries(${PROJECT_NAME} ws2_32)
endif()
