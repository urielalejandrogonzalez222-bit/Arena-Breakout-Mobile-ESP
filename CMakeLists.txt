cmake_minimum_required(VERSION 3.22.1)
project("mono")

set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_FLAGS_RELEASE "-Os -DNDEBUG")
set(CMAKE_SHARED_LINKER_FLAGS "-s")
set(CMAKE_BUILD_TYPE Release)

file(GLOB CXX_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ImGui/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/include/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
)

add_library(${CMAKE_PROJECT_NAME} SHARED
    ${CXX_SOURCES}
)

target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ImGui
    ${CMAKE_CURRENT_SOURCE_DIR}/src/include
    ${ANDROID_NDK}/sources/android/native_app_glue
)

target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE
    android
    EGL
    GLESv3
    log
)
