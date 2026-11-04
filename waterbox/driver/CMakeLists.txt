# The driver: the machine and its exports (both flavours), and the native
# runner. MINIBOX_DIR names the miniBox checkout whose GPU bridge the core
# speaks; build/generated-gl holds both halves of it (waterbox/gen-gl.sh).
set(MINIBOX_DIR "$ENV{MINIBOX_DIR}" CACHE PATH "the miniBox checkout")
if(NOT EXISTS "${MINIBOX_DIR}/source/gl/gl-bridge.h")
  message(FATAL_ERROR "set MINIBOX_DIR to a miniBox checkout")
endif()
set(GENERATED_GL "${CMAKE_CURRENT_SOURCE_DIR}/../../build/generated-gl")
set(GLAD "${CMAKE_CURRENT_SOURCE_DIR}/../glad")

add_library(cemu-driver STATIC
  ${CMAKE_CURRENT_SOURCE_DIR}/cemu_driver.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/wbx-entry.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/memfs.cpp
  ${GENERATED_GL}/gl-bridge-guest.cpp
)
target_link_libraries(cemu-driver PUBLIC cemu)
target_compile_options(cemu-driver PRIVATE ${CEMU_OPTIONS})
target_include_directories(cemu-driver PRIVATE ${MINIBOX_DIR}/source/gl ${GENERATED_GL}
  ${MINIBOX_DIR}/source/guest/include ${MINIBOX_DIR}/extern/jsmn)
target_precompile_headers(cemu-driver REUSE_FROM cemu)
set_source_files_properties(
  ${CMAKE_CURRENT_SOURCE_DIR}/memfs.cpp ${CMAKE_CURRENT_SOURCE_DIR}/wbx-entry.cpp ${GENERATED_GL}/gl-bridge-guest.cpp
  PROPERTIES SKIP_PRECOMPILE_HEADERS ON)
set_source_files_properties(${GENERATED_GL}/gl-bridge-guest.cpp PROPERTIES INCLUDE_DIRECTORIES ${GLAD}/include)
if(CHIMERA_GUEST_BUILD)
  target_sources(cemu-driver PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../guest/guest-libc.cpp)
  set_source_files_properties(${CMAKE_CURRENT_SOURCE_DIR}/../guest/guest-libc.cpp PROPERTIES SKIP_PRECOMPILE_HEADERS ON COMPILE_DEFINITIONS CHIMERA_MEMFS=1)
  target_include_directories(cemu-driver PRIVATE ${MINIBOX_DIR}/extern/emulibc)
endif()

if(NOT CHIMERA_GUEST_BUILD)
  add_executable(run-native
    ${CMAKE_CURRENT_SOURCE_DIR}/native/run-native.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../gl-host.c
    ${GLAD}/src/gl.c)
  target_include_directories(run-native PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/.. ${MINIBOX_DIR}/source/gl ${GENERATED_GL} ${GLAD}/include)
  target_compile_definitions(run-native PRIVATE CHIMERA_GL_BRIDGE)
  set_target_properties(run-native PROPERTIES ENABLE_EXPORTS ON)
  target_link_libraries(run-native PRIVATE -Wl,--whole-archive cemu-driver -Wl,--no-whole-archive cemu EGL)
endif()
