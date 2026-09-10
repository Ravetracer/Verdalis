cmake_minimum_required(VERSION 3.16)
project(RiverFlow VERSION 0.2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
   set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

option(RIVERFLOW_BUILD_TOOLS "Build the offline render/verification host" ON)
option(RIVERFLOW_BUILD_GUI "Build the plugin window (needs X11 and Cairo)" ON)

# CLAP plugins live in ~/.clap on Linux, so default there rather than /usr/local.
if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
   set(CMAKE_INSTALL_PREFIX "$ENV{HOME}/.clap" CACHE PATH "Install prefix" FORCE)
endif()

# The suite's shared code, beside this plugin at the repository root.
get_filename_component(VERDALIS_SHARED_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../shared" ABSOLUTE)

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
message(STATUS "RiverFlow: using CLAP headers from ${CLAP_INCLUDE_DIR}")

# ------------------------------------------------------- embedded preset table
set(RIVERFLOW_PRESET_DIR "${CMAKE_CURRENT_SOURCE_DIR}/presets")
file(GLOB RIVERFLOW_PRESET_FILES "${RIVERFLOW_PRESET_DIR}/*.riverflow")
set(RIVERFLOW_GENERATED_HEADER "${CMAKE_CURRENT_BINARY_DIR}/generated/presets_generated.h")

add_custom_command(
   OUTPUT "${RIVERFLOW_GENERATED_HEADER}"
   COMMAND "${CMAKE_COMMAND}"
      -DPRESET_DIR=${RIVERFLOW_PRESET_DIR}
      -DOUTPUT=${RIVERFLOW_GENERATED_HEADER}
      -DNAMESPACE=riverflow
      -DPRESET_EXT=riverflow
      -P "${VERDALIS_SHARED_DIR}/cmake/embed_presets.cmake"
   DEPENDS ${RIVERFLOW_PRESET_FILES} "${VERDALIS_SHARED_DIR}/cmake/embed_presets.cmake"
   COMMENT "Embedding ${CMAKE_CURRENT_SOURCE_DIR}/presets into presets_generated.h"
   VERBATIM
)
add_custom_target(riverflow_presets DEPENDS "${RIVERFLOW_GENERATED_HEADER}")

# ------------------------------------------------------------------ gui deps
#
# X11 and Cairo only. Both are already on any Linux box that can run a DAW, and
# using them directly keeps RiverFlow a single self-contained .clap file.
set(RIVERFLOW_GUI_SOURCES "")
# pkg-config on this machine describes this machine, so a cross build must not
# consult it: it would report the host's X11 and Cairo and drag /usr/include
# into a Windows compile. Point RIVERFLOW_WIN_CAIRO at a Cairo cross-built with
# its win32 backend instead (see ../shared/cmake/build-windows-cairo.sh,
# or run ../setup-winbuild.sh once).
if(RIVERFLOW_BUILD_GUI AND CMAKE_SYSTEM_NAME STREQUAL "Windows")
   if(NOT RIVERFLOW_WIN_CAIRO OR NOT EXISTS "${RIVERFLOW_WIN_CAIRO}/lib/libcairo.a")
      set(RIVERFLOW_BUILD_GUI OFF)
      message(WARNING "RiverFlow: no Windows Cairo at RIVERFLOW_WIN_CAIRO, building without a window")
   else()
      set(RIVERFLOW_GUI_DEPS_INCLUDE_DIRS "${RIVERFLOW_WIN_CAIRO}/include" "${RIVERFLOW_WIN_CAIRO}/include/cairo")
      set(RIVERFLOW_GUI_DEPS_LIBRARY_DIRS "${RIVERFLOW_WIN_CAIRO}/lib")
      # Cairo's win32 backend draws through GDI and measures text with it.
      set(RIVERFLOW_GUI_DEPS_LIBRARIES cairo pixman-1 gdi32 msimg32 user32)
      set(RIVERFLOW_GUI_SOURCES src/gui/gui.cpp)
      # Cairo's headers declare everything dllimport unless told the library is
      # static, which is how it is built here.
      set(RIVERFLOW_GUI_DEFINES CAIRO_WIN32_STATIC_BUILD)
      set(RIVERFLOW_GUI_FOUND_WIN TRUE)
      message(STATUS "RiverFlow: building the plugin window (Win32 + Cairo)")
   endif()
endif()

if(RIVERFLOW_BUILD_GUI AND NOT RIVERFLOW_GUI_FOUND_WIN)
   find_package(PkgConfig QUIET)
   if(PkgConfig_FOUND)
      pkg_check_modules(RIVERFLOW_GUI_DEPS QUIET x11 cairo)
   endif()
   if(RIVERFLOW_GUI_DEPS_FOUND)
      set(RIVERFLOW_GUI_SOURCES src/gui/gui.cpp)
      message(STATUS "RiverFlow: building the plugin window (X11 + Cairo)")
   else()
      set(RIVERFLOW_BUILD_GUI OFF)
      message(WARNING "RiverFlow: x11/cairo not found, building without a GUI")
   endif()
endif()

# ------------------------------------------------------------------ shared code
#
# The suite's common parameter model and DSP building blocks. Lives beside the
# plugins at the suite root, so it is reached with an explicit path.
# The window lives in shared/; hand it what this plugin discovered above, since
# a cross build must not let the shared CMakeLists consult this machine.
if(RIVERFLOW_BUILD_GUI)
   set(VERDALIS_WITH_GUI ON)
   set(VERDALIS_GUI_INCLUDE_DIRS ${RIVERFLOW_GUI_DEPS_INCLUDE_DIRS})
   set(VERDALIS_GUI_LIBRARY_DIRS ${RIVERFLOW_GUI_DEPS_LIBRARY_DIRS})
   set(VERDALIS_GUI_LIBRARIES ${RIVERFLOW_GUI_DEPS_LIBRARIES})
   set(VERDALIS_GUI_DEFINES ${RIVERFLOW_GUI_DEFINES})
endif()

add_subdirectory("${VERDALIS_SHARED_DIR}" verdalis-shared)

# ------------------------------------------------------------------- the plugin
add_library(RiverFlow MODULE
   src/plugin.cpp
   src/preset_provider.cpp
   src/preset.cpp
   src/params.cpp
   src/dsp/river_engine.cpp
   ${RIVERFLOW_GUI_SOURCES}
   "${RIVERFLOW_GENERATED_HEADER}"
)
add_dependencies(RiverFlow riverflow_presets)

target_link_libraries(RiverFlow PRIVATE verdalis::shared)

target_include_directories(RiverFlow PRIVATE
   "${CLAP_INCLUDE_DIR}"
   "${CMAKE_CURRENT_SOURCE_DIR}/src"
   "${CMAKE_CURRENT_BINARY_DIR}/generated"
)

target_compile_options(RiverFlow PRIVATE
   -Wall -Wextra
   -fvisibility=hidden
   # Deliberately NOT -ffast-math: on x86 GCC that links crtfastmath.o, which
   # flips FTZ/DAZ for the whole host process. A plugin has no business doing
   # that to its host.
   -fno-math-errno
)

target_link_libraries(RiverFlow PRIVATE ${CMAKE_DL_LIBS})

# The version lives in two places that have to agree: project() above and
# kPluginVersion in src/riverflow.h. Handing the project version to the compiler
# lets riverflow.h static_assert that they match, so a bump that only lands in
# one of them fails the build instead of shipping.
target_compile_definitions(RiverFlow PRIVATE
   RIVERFLOW_CMAKE_VERSION="${PROJECT_VERSION}")

if(RIVERFLOW_BUILD_GUI)
   target_compile_definitions(RiverFlow PRIVATE RIVERFLOW_WITH_GUI ${RIVERFLOW_GUI_DEFINES})
   target_include_directories(RiverFlow PRIVATE ${RIVERFLOW_GUI_DEPS_INCLUDE_DIRS})
   target_link_libraries(RiverFlow PRIVATE ${RIVERFLOW_GUI_DEPS_LIBRARIES})
   target_link_directories(RiverFlow PRIVATE ${RIVERFLOW_GUI_DEPS_LIBRARY_DIRS})
   find_package(Threads REQUIRED)
   target_link_libraries(RiverFlow PRIVATE Threads::Threads)
endif()

# Only clap_entry is exported; everything else stays internal to the DSO.
if(WIN32)
   # A version script is an ELF idea. On Windows clap_entry carries dllexport
   # from CLAP_EXPORT, and --exclude-all-symbols keeps the rest of the DLL to
   # itself. The runtime is linked in so the .clap needs no MinGW DLLs beside
   # it on a machine that has never seen a compiler.
   target_link_options(RiverFlow PRIVATE
      "-Wl,--exclude-all-symbols"
      "-Wl,--no-undefined"
      "-static-libgcc" "-static-libstdc++"
      "-Wl,-Bstatic,--whole-archive" "-lwinpthread" "-Wl,--no-whole-archive,-Bdynamic"
   )
else()
   target_link_options(RiverFlow PRIVATE
      "-Wl,--version-script=${VERDALIS_SHARED_DIR}/cmake/clap_entry.version"
      "-Wl,--no-undefined"
   )
endif()

set_target_properties(RiverFlow PROPERTIES
   PREFIX ""
   OUTPUT_NAME "RiverFlow"
   SUFFIX ".clap"
)

# ------------------------------------------------------------- offline test host
if(RIVERFLOW_BUILD_TOOLS)
   # The preset text format is linked in as well as loaded through the plugin,
   # so the self-test can check that writing a preset and reading it back agree.
   add_executable(riverflow-render tools/render.cpp src/preset.cpp src/params.cpp)
   target_link_libraries(riverflow-render PRIVATE verdalis::shared)
   target_include_directories(riverflow-render PRIVATE
      "${CLAP_INCLUDE_DIR}"
      "${CMAKE_CURRENT_SOURCE_DIR}/src"
   )
   target_compile_options(riverflow-render PRIVATE -Wall -Wextra)
   target_link_libraries(riverflow-render PRIVATE ${CMAKE_DL_LIBS})
   if(WIN32)
      # Same reasoning as the plugin: the test host has to run on a machine that
      # has never had a compiler on it.
      target_link_options(riverflow-render PRIVATE "-static-libgcc" "-static-libstdc++"
         "-Wl,-Bstatic,--whole-archive" "-lwinpthread" "-Wl,--no-whole-archive,-Bdynamic")
   endif()

   # Renders preset files back to back inside one process, which is what makes
   # fitting the library against reference recordings practical. See
   # tools/analysis/.
   if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
      if(RIVERFLOW_BUILD_GUI)
         set(GUIHOST_X11_FOUND TRUE)
         set(GUIHOST_X11_INCLUDE_DIRS "")
         set(GUIHOST_X11_LIBRARIES user32 gdi32)
      endif()
   else()
      find_package(PkgConfig QUIET)
      if(PkgConfig_FOUND)
         pkg_check_modules(GUIHOST_X11 QUIET x11)
      endif()
   endif()
   if(GUIHOST_X11_FOUND)
      add_executable(riverflow-guihost tools/guihost.cpp)
      target_include_directories(riverflow-guihost PRIVATE "${CLAP_INCLUDE_DIR}" ${GUIHOST_X11_INCLUDE_DIRS})
      target_compile_options(riverflow-guihost PRIVATE -Wall -Wextra)
      target_link_libraries(riverflow-guihost PRIVATE ${CMAKE_DL_LIBS} ${GUIHOST_X11_LIBRARIES})
      if(WIN32)
         target_link_options(riverflow-guihost PRIVATE "-static-libgcc" "-static-libstdc++"
            "-Wl,-Bstatic,--whole-archive" "-lwinpthread" "-Wl,--no-whole-archive,-Bdynamic")
      endif()
   endif()

   # The fitting host and the window host are both part of the Linux analysis
   # workflow and have no reason to exist on Windows.
   if(NOT CMAKE_SYSTEM_NAME STREQUAL "Windows")
   add_executable(riverflow-fithost "${VERDALIS_SHARED_DIR}/tools/fithost.cpp")
   target_include_directories(riverflow-fithost PRIVATE "${CLAP_INCLUDE_DIR}")
   target_compile_options(riverflow-fithost PRIVATE -Wall -Wextra)
   target_link_libraries(riverflow-fithost PRIVATE ${CMAKE_DL_LIBS})
   endif()
endif()

# ---------------------------------------------------------------------- install
# The plugin looks for a "presets" directory next to its own binary, so both
# land in the same RiverFlow/ folder.
install(TARGETS RiverFlow LIBRARY DESTINATION "RiverFlow")
install(FILES ${RIVERFLOW_PRESET_FILES} DESTINATION "RiverFlow/presets")

message(STATUS "RiverFlow: 'cmake --install' will write to ${CMAKE_INSTALL_PREFIX}/RiverFlow")
