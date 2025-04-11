cmake_minimum_required(VERSION 3.30)
project(tiny-vfs)

set(CMAKE_CXX_STANDARD 20)

find_package(PkgConfig REQUIRED)
pkg_check_modules(FUSE3 REQUIRED fuse3)

add_executable(${PROJECT_NAME}
        main.cpp
        vfs_description.cpp
        vfs_description.h
        fuse_bridge.cpp
        fuse_bridge.h
        common.h)

target_include_directories(${PROJECT_NAME} PRIVATE ${FUSE3_INCLUDE_DIRS})
target_link_libraries(${PROJECT_NAME} PRIVATE ${FUSE3_LIBRARIES})