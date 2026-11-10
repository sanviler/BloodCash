cmake_minimum_required(VERSION 3.20)
# BloodCash version: set only here (window title, Windows file properties); history in CHANGELOG.md.
project(BloodCash VERSION 1.0.0 LANGUAGES CXX)
set(BC_VERSION_SHORT "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH}")
configure_file(src/version/version.h.in ${CMAKE_BINARY_DIR}/generated/version.h @ONLY)

if(VCPKG_TARGET_TRIPLET MATCHES "static")
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(MSVC)
  add_compile_options(/utf-8)
  add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
endif()

option(BC_GAME "Build the game (needs SDL2)" ON)
option(BC_BUILD_TOOLS "Build the developer tools (bcrun)" OFF)
option(BC_LINUX_PORTABLE "Linux: build an executable for other distributions" OFF)

# the common Amiga core (copy of Framework/core: 68000 CPU, A500 chipset, formats, SDL helpers)
if(BC_GAME)
  # found here too, so that SDL2::SDL2main is visible to the game target
  find_package(SDL2 CONFIG REQUIRED)
endif()
set(AMIGACORE_TESTS OFF CACHE BOOL "" FORCE)
set(AMIGACORE_SDL ${BC_GAME} CACHE BOOL "" FORCE)
add_subdirectory(core)

# Blood Money: disk images and loader
add_library(bcgame STATIC src/bc/disks.cpp src/bc/loader.cpp src/bc/texts.cpp)
target_include_directories(bcgame PUBLIC src ${CMAKE_BINARY_DIR}/generated)
target_link_libraries(bcgame PUBLIC amigacore)

if(BC_GAME)
  set(BC_SOURCES src/app/main.cpp)
  if(WIN32)
    configure_file(src/version/bloodcash.rc.in ${CMAKE_BINARY_DIR}/generated/bloodcash.rc @ONLY)
    list(APPEND BC_SOURCES ${CMAKE_BINARY_DIR}/generated/bloodcash.rc)
  endif()
  add_executable(bloodcash WIN32 ${BC_SOURCES})
  target_link_libraries(bloodcash PRIVATE bcgame amigacore_sdl $<TARGET_NAME_IF_EXISTS:SDL2::SDL2main>)
  # Linux release builds: C++ runtime linked in, no dependency on the newest glibc; only SDL2 is needed
  if(BC_LINUX_PORTABLE AND CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_sources(bloodcash PRIVATE core/src/platform/glibc_compat.cpp)
    target_link_options(bloodcash PRIVATE -static-libstdc++)
    set_target_properties(bloodcash PROPERTIES SKIP_BUILD_RPATH ON)
  endif()
endif()

if(BC_BUILD_TOOLS)
  add_executable(bcrun src/tools/bcrun.cpp)
  target_link_libraries(bcrun PRIVATE bcgame)
endif()
