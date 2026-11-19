cmake_minimum_required(VERSION 3.16)
project(MinecraftOpenGL LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(OpenGL REQUIRED)
find_package(glfw3 3.3 REQUIRED)

add_executable(minecraft
    src/main.cpp
    src/world/chunk.cpp
    src/world/world.cpp
    src/vfx/stb_image.cpp
    lib/glad/src/glad.c
)

target_include_directories(minecraft PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(minecraft PRIVATE
    OpenGL::GL
    glfw
    ${CMAKE_DL_LIBS}
)

# Runtime assets (shaders + textures) are resolved relative to cwd.
# Keep the binary next to those paths under src/ for local runs.
set_target_properties(minecraft PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/src
)

if(UNIX AND NOT APPLE)
    find_package(Threads REQUIRED)
    target_link_libraries(minecraft PRIVATE Threads::Threads)
endif()
