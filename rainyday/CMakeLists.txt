cmake_minimum_required(VERSION 3.16)
project(RainyDay VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
   set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

option(RAINYDAY_BUILD_TOOLS "Build the offline render/verification host" ON)
option(RAINYDAY_BUILD_GUI "Build the plugin window (needs X11 and Cairo)" ON)

# CLAP plugins live in ~/.clap on Linux, so default there rather than /usr/local.
if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
   set(CMAKE_INSTALL_PREFIX "$ENV{HOME}/.clap" CACHE PATH "Install prefix" FORCE)
endif()

# ---------------------------------------------------------------- CLAP headers
#
# Point CLAP_INCLUDE_DIR at the include/ directory of a CLAP checkout, or let
# the search below find one.
set(CLAP_INCLUDE_DIR "" CACHE PATH "Directory containing clap/clap.h")

if(NOT CLAP_INCLUDE_DIR)
   set(_clap_hints
      "${CMAKE_CURRENT_SOURCE_DIR}/../CLAP/clap/include"
      "${CMAKE_CURRENT_SOURCE_DIR}/../clap/include"
      "${CMAKE_CURRENT_SOURCE_DIR}/external/clap/include"
      "$ENV{HOME}/projects/private_stuff/CLAP/clap/include"
      "/usr/local/include"
      "/usr/include"
   )
   foreach(_hint ${_clap_hints})
      get_filename_component(_abs "${_hint}" ABSOLUTE)
      if(EXISTS "${_abs}/clap/clap.h")
         set(CLAP_INCLUDE_DIR "${_abs}" CACHE PATH "Directory containing clap/clap.h" FORCE)
         break()
      endif()
   endforeach()
endif()

if(NOT CLAP_INCLUDE_DIR OR NOT EXISTS "${CLAP_INCLUDE_DIR}/clap/clap.h")
   message(FATAL_ERROR
      "Could not find clap/clap.h. Configure with -DCLAP_INCLUDE_DIR=/path/to/clap/include")
endif()
message(STATUS "RainyDay: using CLAP headers from ${CLAP_INCLUDE_DIR}")

# ------------------------------------------------------- embedded preset table
set(RAINYDAY_PRESET_DIR "${CMAKE_CURRENT_SOURCE_DIR}/presets")
file(GLOB RAINYDAY_PRESET_FILES "${RAINYDAY_PRESET_DIR}/*.rainyday")
set(RAINYDAY_GENERATED_HEADER "${CMAKE_CURRENT_BINARY_DIR}/generated/presets_generated.h")

add_custom_command(
   OUTPUT "${RAINYDAY_GENERATED_HEADER}"
   COMMAND "${CMAKE_COMMAND}"
      -DPRESET_DIR=${RAINYDAY_PRESET_DIR}
      -DOUTPUT=${RAINYDAY_GENERATED_HEADER}
      -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/embed_presets.cmake"
   DEPENDS ${RAINYDAY_PRESET_FILES} "${CMAKE_CURRENT_SOURCE_DIR}/cmake/embed_presets.cmake"
   COMMENT "Embedding ${CMAKE_CURRENT_SOURCE_DIR}/presets into presets_generated.h"
   VERBATIM
)
add_custom_target(rainyday_presets DEPENDS "${RAINYDAY_GENERATED_HEADER}")

# ------------------------------------------------------------------ gui deps
#
# X11 and Cairo only. Both are already on any Linux box that can run a DAW, and
# using them directly keeps RainyDay a single self-contained .clap file.
set(RAINYDAY_GUI_SOURCES "")
if(RAINYDAY_BUILD_GUI)
   find_package(PkgConfig QUIET)
   if(PkgConfig_FOUND)
      pkg_check_modules(RAINYDAY_GUI_DEPS QUIET x11 cairo)
   endif()
   if(RAINYDAY_GUI_DEPS_FOUND)
      set(RAINYDAY_GUI_SOURCES src/gui/gui.cpp)
      message(STATUS "RainyDay: building the plugin window (X11 + Cairo)")
   else()
      set(RAINYDAY_BUILD_GUI OFF)
      message(WARNING "RainyDay: x11/cairo not found, building without a GUI")
   endif()
endif()

# ------------------------------------------------------------------- the plugin
add_library(RainyDay MODULE
   src/plugin.cpp
   src/preset_provider.cpp
   src/preset.cpp
   src/params.cpp
   src/dsp/rain_engine.cpp
   ${RAINYDAY_GUI_SOURCES}
   "${RAINYDAY_GENERATED_HEADER}"
)
add_dependencies(RainyDay rainyday_presets)

target_include_directories(RainyDay PRIVATE
   "${CLAP_INCLUDE_DIR}"
   "${CMAKE_CURRENT_SOURCE_DIR}/src"
   "${CMAKE_CURRENT_BINARY_DIR}/generated"
)

target_compile_options(RainyDay PRIVATE
   -Wall -Wextra
   -fvisibility=hidden
   # Deliberately NOT -ffast-math: on x86 GCC that links crtfastmath.o, which
   # flips FTZ/DAZ for the whole host process. A plugin has no business doing
   # that to its host.
   -fno-math-errno
)

target_link_libraries(RainyDay PRIVATE ${CMAKE_DL_LIBS})

if(RAINYDAY_BUILD_GUI)
   target_compile_definitions(RainyDay PRIVATE RAINYDAY_WITH_GUI)
   target_include_directories(RainyDay PRIVATE ${RAINYDAY_GUI_DEPS_INCLUDE_DIRS})
   target_link_libraries(RainyDay PRIVATE ${RAINYDAY_GUI_DEPS_LIBRARIES})
   target_link_directories(RainyDay PRIVATE ${RAINYDAY_GUI_DEPS_LIBRARY_DIRS})
   find_package(Threads REQUIRED)
   target_link_libraries(RainyDay PRIVATE Threads::Threads)
endif()

# Only clap_entry is exported; everything else stays internal to the DSO.
target_link_options(RainyDay PRIVATE
   "-Wl,--version-script=${CMAKE_CURRENT_SOURCE_DIR}/cmake/rainyday.version"
   "-Wl,--no-undefined"
)

set_target_properties(RainyDay PROPERTIES
   PREFIX ""
   OUTPUT_NAME "RainyDay"
   SUFFIX ".clap"
)

# ------------------------------------------------------------- offline test host
if(RAINYDAY_BUILD_TOOLS)
   add_executable(rainyday-render tools/render.cpp)
   target_include_directories(rainyday-render PRIVATE
      "${CLAP_INCLUDE_DIR}"
      "${CMAKE_CURRENT_SOURCE_DIR}/src"
   )
   target_compile_options(rainyday-render PRIVATE -Wall -Wextra)
   target_link_libraries(rainyday-render PRIVATE ${CMAKE_DL_LIBS})

   # Renders preset files back to back inside one process, which is what makes
   # fitting the library against reference recordings practical. See
   # tools/analysis/.
   add_executable(rainyday-fithost tools/fithost.cpp)
   target_include_directories(rainyday-fithost PRIVATE "${CLAP_INCLUDE_DIR}")
   target_compile_options(rainyday-fithost PRIVATE -Wall -Wextra)
   target_link_libraries(rainyday-fithost PRIVATE ${CMAKE_DL_LIBS})
endif()

# ---------------------------------------------------------------------- install
# The plugin looks for a "presets" directory next to its own binary, so both
# land in the same RainyDay/ folder.
install(TARGETS RainyDay LIBRARY DESTINATION "RainyDay")
install(FILES ${RAINYDAY_PRESET_FILES} DESTINATION "RainyDay/presets")

message(STATUS "RainyDay: 'cmake --install' will write to ${CMAKE_INSTALL_PREFIX}/RainyDay")
