cmake_minimum_required(VERSION 4.0)
project(ifs)

set(CMAKE_CXX_STANDARD 14)

find_package(PkgConfig REQUIRED)
pkg_check_modules(GTK REQUIRED gtk+-3.0)

include_directories(ifs)
include_directories(../_headers)
include_directories(../_headers/imgui)
include_directories(../_headers/nfd)
include_directories(${GTK_INCLUDE_DIRS})


link_libraries(OpenCL
        glfw
        GL
        pthread
        dl
        X11
        Xrandr
        Xi)

link_directories(../_lib)
link_libraries(nfd)

link_directories(${GTK_LIBRARY_DIRS})

add_definitions(${GTK_CFLAGS_OTHER})


add_executable(ifs
        ifs/Camera.cpp
        ifs/Camera.h
        ifs/Camera2D.cpp
        ifs/Camera2D.h
        ifs/CLManager.h
        ifs/common_def.h
        ifs/filedialog.cpp
        ifs/filedialog.h
        ifs/ifs.cpp
        ifs/ifs.h
        ifs/KernelRString.h
        ifs/kernels.cpp
        ifs/kernels.h
        ifs/Key.cpp
        ifs/Key.h
        ifs/main.cpp
        ifs/ShaderProgram.cpp
        ifs/ShaderProgram.h
        ifs/stb_image_write.h
        ../_src/glad.c
        ../_src/imgui/imgui.cpp
        ../_src/imgui/imgui_demo.cpp
        ../_src/imgui/imgui_draw.cpp
        ../_src/imgui/imgui_impl_glfw.cpp
        ../_src/imgui/imgui_impl_opengl3.cpp
        ../_src/imgui/imgui_stdlib.cpp
        ../_src/imgui/imgui_tables.cpp
        ../_src/imgui/imgui_widgets.cpp)

target_link_libraries(ifs ${GTK_LIBRARIES})
