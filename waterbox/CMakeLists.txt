# The Cemu core: Cemu's own sources (the lists in cemu-sources.cmake, read
# from upstream's CMakeLists.txt files by gen-sources.py) and this
# repository's driver, built the same way for both flavours:
#
#   native  - the reference, for the host (waterbox/build-native.sh)
#   guest   - the core, with miniBox's toolchain (waterbox/build-guest.sh;
#             guest-toolchain.cmake defines CHIMERA_GUEST)
#
# The libraries Cemu needs come from build/deps/<flavour> (build-deps.sh).
cmake_minimum_required(VERSION 3.21)
project(cemu-chimera C CXX ASM)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CEMU_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../extern/cemu" CACHE PATH "the Cemu checkout")
set(CEMU_SRC "${CEMU_ROOT}/src")
set(CEMU_DEPS "${CEMU_ROOT}/dependencies")
set(DRIVER "${CMAKE_CURRENT_SOURCE_DIR}/driver")
option(CHIMERA_GUEST_BUILD "build for miniBox's guest" OFF)

include(${CMAKE_CURRENT_SOURCE_DIR}/cemu-sources.cmake)

find_package(fmt 12.1 REQUIRED)
find_package(pugixml REQUIRED)
find_package(glm REQUIRED)
find_package(RapidJSON REQUIRED)
find_package(ZLIB REQUIRED)
find_package(PNG REQUIRED)
find_package(OpenSSL REQUIRED COMPONENTS Crypto SSL)
find_library(ZSTD_LIBRARY NAMES libzstd.a zstd REQUIRED)
find_path(ZSTD_INCLUDE_DIR zstd.h REQUIRED)
find_path(BOOST_INCLUDE_DIR boost/predef.h REQUIRED)
find_path(CURL_INCLUDE_DIR curl/curl.h REQUIRED)

# What every Cemu source is compiled with. Upstream's ENABLE_OPENGL is the
# only renderer; CHIMERA marks this core's changes (patches/).
set(CEMU_DEFINES
  CHIMERA=1
  ENABLE_OPENGL
  EMULATOR_VERSION_MAJOR=2
  EMULATOR_VERSION_MINOR=0
  EMULATOR_VERSION_PATCH=0
  EMULATOR_HASH=chimera
  PUBLIC_RELEASE
  BOOST_ALL_NO_LIB
  HAS_DSU=0
)
set(CEMU_OPTIONS
  -Wno-multichar -Wno-invalid-offsetof -Wno-switch -Wno-ignored-attributes
  -Wno-deprecated-enum-enum-conversion -fno-strict-aliasing
)

set(CEMU_INCLUDES
  ${DRIVER}/include
  ${CEMU_SRC}
  ${CEMU_SRC}/gui/interface
  ${CEMU_SRC}/Cafe
  ${CEMU_SRC}/Cemu
  ${CEMU_SRC}/Common
  ${CEMU_SRC}/config
  ${CEMU_SRC}/util
  ${CEMU_SRC}/input
  ${CEMU_SRC}/audio
  ${CEMU_SRC}/imgui
  ${CEMU_DEPS}/imgui
  ${CEMU_DEPS}/ZArchive/include
  ${CEMU_DEPS}/ih264d/common
  ${CEMU_DEPS}/ih264d/decoder
  ${BOOST_INCLUDE_DIR}
  ${ZSTD_INCLUDE_DIR}
  ${CURL_INCLUDE_DIR}
)

# ih264d, the H.264 decoder the console's video library uses
file(STRINGS ${CEMU_DEPS}/ih264d/CMakeLists.txt IH264D_LINES REGEX "^\"(common|decoder)/(x86/)?[a-z0-9_]+\\.c\"")
set(IH264D_SOURCES)
foreach(line ${IH264D_LINES})
  string(REPLACE "\"" "" f "${line}")
  list(APPEND IH264D_SOURCES ${CEMU_DEPS}/ih264d/${f})
endforeach()
add_library(ih264d STATIC ${IH264D_SOURCES})
target_include_directories(ih264d PRIVATE ${CEMU_DEPS}/ih264d/common ${CEMU_DEPS}/ih264d/decoder
  ${CEMU_DEPS}/ih264d/common/x86 ${CEMU_DEPS}/ih264d/decoder/x86)
target_compile_options(ih264d PRIVATE -w)

# ZArchive, the reader of .wua files
add_library(zarchive STATIC
  ${CEMU_DEPS}/ZArchive/src/zarchivereader.cpp
  ${CEMU_DEPS}/ZArchive/src/zarchivewriter.cpp
  ${CEMU_DEPS}/ZArchive/src/sha_256.c)
target_include_directories(zarchive PUBLIC ${CEMU_DEPS}/ZArchive/include PRIVATE ${ZSTD_INCLUDE_DIR})

list(TRANSFORM CEMU_COMMON_SOURCES PREPEND ${CEMU_SRC}/)
list(TRANSFORM CEMU_CAFE_SOURCES PREPEND ${CEMU_SRC}/)
list(TRANSFORM CEMU_COMPONENTS_SOURCES PREPEND ${CEMU_SRC}/)
list(TRANSFORM CEMU_CONFIG_SOURCES PREPEND ${CEMU_SRC}/)
list(TRANSFORM CEMU_UTIL_SOURCES PREPEND ${CEMU_SRC}/)
list(TRANSFORM CEMU_INPUT_SOURCES PREPEND ${CEMU_SRC}/)
list(TRANSFORM CEMU_AUDIO_SOURCES PREPEND ${CEMU_SRC}/)
list(TRANSFORM CEMU_IMGUI_SOURCES PREPEND ${CEMU_SRC}/)
list(TRANSFORM CEMU_RESOURCE_SOURCES PREPEND ${CEMU_SRC}/)

file(GLOB DRIVER_CEMU_SOURCES ${DRIVER}/cemu/*.cpp ${DRIVER}/cemu/*.S)

# Cemu's per-game profiles (bin/gameProfiles/default, part of Cemu and under
# its licence), compiled in as bytes: the driver writes them into the
# machine's data folder, where Cemu reads the one of the title it runs
# (driver/cemu/profiles.cpp). Names in lower case, as Cemu asks for them.
file(GLOB CEMU_GAME_PROFILES ${CEMU_ROOT}/bin/gameProfiles/default/*.ini)
list(SORT CEMU_GAME_PROFILES)
set(GAME_PROFILES_INC ${CMAKE_CURRENT_BINARY_DIR}/generated/game_profiles.inc)
set(_profiles "")
foreach(_file ${CEMU_GAME_PROFILES})
  get_filename_component(_name ${_file} NAME)
  string(TOLOWER ${_name} _name)
  file(READ ${_file} _hex HEX)
  string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," _hex "${_hex}")
  string(APPEND _profiles "{\"${_name}\", {${_hex}}},\n")
endforeach()
file(WRITE ${GAME_PROFILES_INC}.new "${_profiles}")
configure_file(${GAME_PROFILES_INC}.new ${GAME_PROFILES_INC} COPYONLY)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${CEMU_GAME_PROFILES})
set_source_files_properties(${DRIVER}/cemu/profiles.cpp PROPERTIES
  INCLUDE_DIRECTORIES ${CMAKE_CURRENT_BINARY_DIR}/generated SKIP_PRECOMPILE_HEADERS ON)

add_library(cemu STATIC
  ${CEMU_COMMON_SOURCES}
  ${CEMU_CAFE_SOURCES}
  ${CEMU_COMPONENTS_SOURCES}
  ${CEMU_CONFIG_SOURCES}
  ${CEMU_UTIL_SOURCES}
  ${CEMU_INPUT_SOURCES}
  ${CEMU_AUDIO_SOURCES}
  ${CEMU_IMGUI_SOURCES}
  ${CEMU_RESOURCE_SOURCES}
  ${DRIVER_CEMU_SOURCES}
)
target_compile_definitions(cemu PUBLIC ${CEMU_DEFINES})
target_compile_options(cemu PRIVATE ${CEMU_OPTIONS})
target_include_directories(cemu PUBLIC ${CEMU_INCLUDES})
target_precompile_headers(cemu PRIVATE ${CEMU_SRC}/Common/precompiled.h)
# the font is embedded with .incbin, found beside the .S file
set_source_files_properties(${CEMU_SRC}/resource/embedded/fontawesome.S PROPERTIES INCLUDE_DIRECTORIES ${CEMU_SRC}/resource)
set_source_files_properties(${CEMU_DEPS}/imgui/imgui.cpp ${CEMU_DEPS}/imgui/imgui_draw.cpp
  ${CEMU_DEPS}/imgui/imgui_tables.cpp ${CEMU_DEPS}/imgui/imgui_widgets.cpp
  PROPERTIES SKIP_PRECOMPILE_HEADERS ON)
target_link_libraries(cemu PUBLIC
  zarchive ih264d fmt::fmt pugixml::pugixml glm::glm PNG::PNG ZLIB::ZLIB OpenSSL::SSL OpenSSL::Crypto ${ZSTD_LIBRARY})
if(CHIMERA_GUEST_BUILD)
  # names of the host's headers the guest does not have (X11's types)
  target_include_directories(cemu PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/guest/include ${CMAKE_CURRENT_SOURCE_DIR}/glad/include)
  target_compile_options(cemu PUBLIC "$<$<COMPILE_LANGUAGE:C,CXX>:SHELL:-include ${CMAKE_CURRENT_SOURCE_DIR}/guest/include/chimera_guest_compat.h>")
endif()
if(NOT CHIMERA_GUEST_BUILD)
  find_package(Threads REQUIRED)
  target_link_libraries(cemu PUBLIC Threads::Threads ${CMAKE_DL_LIBS})
endif()

if(EXISTS ${DRIVER}/CMakeLists.txt)
  add_subdirectory(${DRIVER} driver)
endif()
