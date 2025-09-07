cmake_minimum_required(VERSION 3.21)
project(vpn_gui_clean LANGUAGES C CXX)
add_compile_options(/utf-8)


set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---- 源码 ----
file(GLOB_RECURSE SRC_FILES "src/*.cpp" "src/*.c")
# 或手动添加
list(APPEND SRC_FILES "${CMAKE_SOURCE_DIR}/src/vpn/ProcessRunner.cpp")

# ---- GLAD ----
set(GLAD_INCLUDE "${CMAKE_SOURCE_DIR}/external/glad/include")
set(GLAD_SRC     "${CMAKE_SOURCE_DIR}/external/glad/src/glad.c")
if (EXISTS "${GLAD_SRC}")
    list(APPEND SRC_FILES "${GLAD_SRC}")
else()
    message(WARNING "GLAD source not found at ${GLAD_SRC}")
endif()

# ---- GLFW ----
set(GLFW_INCLUDE "${CMAKE_SOURCE_DIR}/external/glfw/include")

# 预编译库目录（根据你本地版本二选一）
set(GLFW_LIB_DIR "")
if (EXISTS "${CMAKE_SOURCE_DIR}/external/glfw/lib-vc2022")
    set(GLFW_LIB_DIR "${CMAKE_SOURCE_DIR}/external/glfw/lib-vc2022")
elseif (EXISTS "${CMAKE_SOURCE_DIR}/external/glfw/lib-vc2019")
    set(GLFW_LIB_DIR "${CMAKE_SOURCE_DIR}/external/glfw/lib-vc2019")
endif()

# ---- Dear ImGui（按你的截图路径）----
set(IMGUI_DIR         "${CMAKE_SOURCE_DIR}/external/imgui")
set(IMGUI_BACKENDS    "${IMGUI_DIR}/backends")
set(IMGUI_SOURCES
    "${IMGUI_DIR}/imgui.cpp"
    "${IMGUI_DIR}/imgui_draw.cpp"
    "${IMGUI_DIR}/imgui_widgets.cpp"
    "${IMGUI_DIR}/imgui_tables.cpp"
    "${IMGUI_BACKENDS}/imgui_impl_glfw.cpp"
    "${IMGUI_BACKENDS}/imgui_impl_opengl3.cpp"
)
foreach(f IN LISTS IMGUI_SOURCES)
    if (EXISTS "${f}")
        list(APPEND SRC_FILES "${f}")
    else()
        message(WARNING "ImGui file not found: ${f}")
    endif()
endforeach()

add_executable(${PROJECT_NAME} ${SRC_FILES})

# 统一 MSVC 源文件编码为 UTF-8，消除 C4828 警告
add_compile_options($<$<CXX_COMPILER_ID:MSVC>:/utf-8>)

# 把新添的实现文件编进目标，解决 LNK2019
target_sources(VPN_GUI_OpenGL PRIVATE
    src/ui/Panels.cpp
    src/vpn/OpenVpnRunner.cpp
    src/vpn/ProcessRunner.cpp
)


# 头文件可见
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/VPN_GUI_OpenGL
    ${CMAKE_SOURCE_DIR}/external
    ${GLAD_INCLUDE}
    ${GLFW_INCLUDE}
    ${IMGUI_DIR}
    ${IMGUI_BACKENDS}
)

# 让 ImGui 后端使用 GLAD 作为 OpenGL 加载器
target_compile_definitions(${PROJECT_NAME} PRIVATE IMGUI_IMPL_OPENGL_LOADER_GLAD)

# OpenGL
find_package(OpenGL REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE OpenGL::GL)

# Windows 额外库
if (WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE ws2_32)
endif()

# GLFW 链接（动态库：glfw3dll；静态库：glfw3）
if (MSVC AND GLFW_LIB_DIR)
    target_link_directories(${PROJECT_NAME} PRIVATE "${GLFW_LIB_DIR}")
    # 你之前使用的是 dll，所以这里默认链接 import lib：glfw3dll.lib
    target_link_libraries(${PROJECT_NAME} PRIVATE glfw3dll)

    # 运行时复制 glfw3.dll
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${GLFW_LIB_DIR}/glfw3.dll"
            "$<TARGET_FILE_DIR:${PROJECT_NAME}>/glfw3.dll")
endif()

# Suppress warning about character set
if (MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /utf-8)
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
endif()

# Suppress warning C4005: macro redefinition
if(MSVC)
    add_definitions(-DAPIENTRY=)
endif()
