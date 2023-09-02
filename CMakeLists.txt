cmake_minimum_required(VERSION 3.20)
project(VoxelGame VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
#set(CMAKE_EXE_LINKER_FLAGS  "/FORCE")
set(CMAKE_EXPORT_COMPILE_COMMANDS 1)
# find_package(SDL2 CONFIG REQUIRED)
#find_package(sharderc_shared CONFIG REQUIRED);

file(GLOB_RECURSE source_files "${PROJECT_SOURCE_DIR}/src/*.cpp")
add_executable(${PROJECT_NAME} ${source_files})

target_include_directories(${PROJECT_NAME} PUBLIC "include" "lib")

# target_link_libraries(${PROJECT_NAME} PRIVATE SDL2::SDL2 SDL2::SDL2main)

target_include_directories(${PROJECT_NAME} PUBLIC "/usr/include")
target_link_libraries(${PROJECT_NAME} PUBLIC "/usr/lib/x86_64-linux-gnu/libvulkan.so")
target_link_libraries(${PROJECT_NAME} PUBLIC "/usr/lib/x86_64-linux-gnu/libSDL2.so")
target_link_libraries(${PROJECT_NAME} PUBLIC "/usr/lib/x86_64-linux-gnu/libSDL2main.a")
target_link_libraries(${PROJECT_NAME} PUBLIC "/usr/lib/x86_64-linux-gnu/libshaderc_shared.so")

if (UNIX)
    # target_link_libraries(${PROJECT_NAME} pthread)
endif (UNIX)

file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/assets DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
#file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/shaders DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
