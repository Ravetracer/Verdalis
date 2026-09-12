cmake_minimum_required(VERSION 3.16)
project(ThunderClap VERSION 1.3.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
   set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

option(THUNDERCLAP_BUILD_TOOLS "Build the offline render/verification host" ON)
option(THUNDERCLAP_BUILD_GUI "Build the plugin window (needs X11 and Cairo)" ON)

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
message(STATUS "ThunderClap: using CLAP headers from ${CLAP_INCLUDE_DIR}")

# ------------------------------------------------------- embedded preset table
set(THUNDERCLAP_PRESET_DIR "${CMAKE_CURRENT_SOURCE_DIR}/presets")
file(GLOB THUNDERCLAP_PRESET_FILES "${THUNDERCLAP_PRESET_DIR}/*.thunderclap")
set(THUNDERCLAP_GENERATED_HEADER "${CMAKE_CURRENT_BINARY_DIR}/generated/presets_generated.h")

add_custom_command(
   OUTPUT "${THUNDERCLAP_GENERATED_HEADER}"
   COMMAND "${CMAKE_COMMAND}"
      -DPRESET_DIR=${THUNDERCLAP_PRESET_DIR}
      -DOUTPUT=${THUNDERCLAP_GENERATED_HEADER}
      -DNAMESPACE=thunderclap
      -DPRESET_EXT=thunderclap
      -P "${VERDALIS_SHARED_DIR}/cmake/embed_presets.cmake"
   DEPENDS ${THUNDERCLAP_PRESET_FILES} "${VERDALIS_SHARED_DIR}/cmake/embed_presets.cmake"
   COMMENT "Embedding ${CMAKE_CURRENT_SOURCE_DIR}/presets into presets_generated.h"
   VERBATIM
)
add_custom_target(thunderclap_presets DEPENDS "${THUNDERCLAP_GENERATED_HEADER}")

# ------------------------------------------------------------------ gui deps
#
# X11 and Cairo only. Both are already on any Linux box that can run a DAW, and
# using them directly keeps ThunderClap a single self-contained .clap file.
set(THUNDERCLAP_GUI_SOURCES "")
# pkg-config on this machine describes this machine, so a cross build must not
# consult it: it would report the host's X11 and Cairo and drag /usr/include
# into a Windows compile. Point THUNDERCLAP_WIN_CAIRO at a Cairo cross-built with
# its win32 backend instead (see ../shared/cmake/build-windows-cairo.sh,
# or run ../setup-winbuild.sh once).
if(THUNDERCLAP_BUILD_GUI AND CMAKE_SYSTEM_NAME STREQUAL "Windows")
   if(NOT THUNDERCLAP_WIN_CAIRO OR NOT EXISTS "${THUNDERCLAP_WIN_CAIRO}/lib/libcairo.a")
      set(THUNDERCLAP_BUILD_GUI OFF)
      message(WARNING "ThunderClap: no Windows Cairo at THUNDERCLAP_WIN_CAIRO, building without a window")
   else()
      set(THUNDERCLAP_GUI_DEPS_INCLUDE_DIRS "${THUNDERCLAP_WIN_CAIRO}/include" "${THUNDERCLAP_WIN_CAIRO}/include/cairo")
      set(THUNDERCLAP_GUI_DEPS_LIBRARY_DIRS "${THUNDERCLAP_WIN_CAIRO}/lib")
      # Cairo's win32 backend draws through GDI and measures text with it.
      set(THUNDERCLAP_GUI_DEPS_LIBRARIES cairo pixman-1 gdi32 msimg32 user32)
      set(THUNDERCLAP_GUI_SOURCES src/gui/gui.cpp)
      # Cairo's headers declare everything dllimport unless told the library is
      # static, which is how it is built here.
      set(THUNDERCLAP_GUI_DEFINES CAIRO_WIN32_STATIC_BUILD)
      set(THUNDERCLAP_GUI_FOUND_WIN TRUE)
      message(STATUS "ThunderClap: building the plugin window (Win32 + Cairo)")
   endif()
endif()

if(THUNDERCLAP_BUILD_GUI AND NOT THUNDERCLAP_GUI_FOUND_WIN)
   find_package(PkgConfig QUIET)
   if(PkgConfig_FOUND)
      pkg_check_modules(THUNDERCLAP_GUI_DEPS QUIET x11 cairo)
   endif()
   if(THUNDERCLAP_GUI_DEPS_FOUND)
      set(THUNDERCLAP_GUI_SOURCES src/gui/gui.cpp)
      message(STATUS "ThunderClap: building the plugin window (X11 + Cairo)")
   else()
      set(THUNDERCLAP_BUILD_GUI OFF)
      message(WARNING "ThunderClap: x11/cairo not found, building without a GUI")
   endif()
endif()

# ------------------------------------------------------------------ shared code
#
# The suite's common parameter model and DSP building blocks. Lives beside the
# plugins at the suite root, so it is reached with an explicit path.
# The window lives in shared/; hand it what this plugin discovered above, since
# a cross build must not let the shared CMakeLists consult this machine.
if(THUNDERCLAP_BUILD_GUI)
   set(VERDALIS_WITH_GUI ON)
   set(VERDALIS_GUI_INCLUDE_DIRS ${THUNDERCLAP_GUI_DEPS_INCLUDE_DIRS})
   set(VERDALIS_GUI_LIBRARY_DIRS ${THUNDERCLAP_GUI_DEPS_LIBRARY_DIRS})
   set(VERDALIS_GUI_LIBRARIES ${THUNDERCLAP_GUI_DEPS_LIBRARIES})
   set(VERDALIS_GUI_DEFINES ${THUNDERCLAP_GUI_DEFINES})
endif()

add_subdirectory("${VERDALIS_SHARED_DIR}" verdalis-shared)

# ------------------------------------------------------------------- the plugin
add_library(ThunderClap MODULE
   src/plugin.cpp
   src/preset_provider.cpp
   src/preset.cpp
   src/params.cpp
   src/dsp/thunder_engine.cpp
   ${THUNDERCLAP_GUI_SOURCES}
   "${THUNDERCLAP_GENERATED_HEADER}"
)
add_dependencies(ThunderClap thunderclap_presets)

target_link_libraries(ThunderClap PRIVATE verdalis::shared)

target_include_directories(ThunderClap PRIVATE
   "${CLAP_INCLUDE_DIR}"
   "${CMAKE_CURRENT_SOURCE_DIR}/src"
   "${CMAKE_CURRENT_BINARY_DIR}/generated"
)

target_compile_options(ThunderClap PRIVATE
   -Wall -Wextra
   -fvisibility=hidden
   # Deliberately NOT -ffast-math: on x86 GCC that links crtfastmath.o, which
   # flips FTZ/DAZ for the whole host process. A plugin has no business doing
   # that to its host.
   -fno-math-errno
)

target_link_libraries(ThunderClap PRIVATE ${CMAKE_DL_LIBS})

if(THUNDERCLAP_BUILD_GUI)
   target_compile_definitions(ThunderClap PRIVATE THUNDERCLAP_WITH_GUI ${THUNDERCLAP_GUI_DEFINES})
   target_include_directories(ThunderClap PRIVATE ${THUNDERCLAP_GUI_DEPS_INCLUDE_DIRS})
   target_link_libraries(ThunderClap PRIVATE ${THUNDERCLAP_GUI_DEPS_LIBRARIES})
   target_link_directories(ThunderClap PRIVATE ${THUNDERCLAP_GUI_DEPS_LIBRARY_DIRS})
   find_package(Threads REQUIRED)
   target_link_libraries(ThunderClap PRIVATE Threads::Threads)
endif()

# Only clap_entry is exported; everything else stays internal to the DSO.
if(WIN32)
   # A version script is an ELF idea. On Windows clap_entry carries dllexport
   # from CLAP_EXPORT, and --exclude-all-symbols keeps the rest of the DLL to
   # itself. The runtime is linked in so the .clap needs no MinGW DLLs beside
   # it on a machine that has never seen a compiler.
   target_link_options(ThunderClap PRIVATE
      "-Wl,--exclude-all-symbols"
      "-Wl,--no-undefined"
      "-static-libgcc" "-static-libstdc++"
      "-Wl,-Bstatic,--whole-archive" "-lwinpthread" "-Wl,--no-whole-archive,-Bdynamic"
   )
else()
   target_link_options(ThunderClap PRIVATE
      "-Wl,--version-script=${VERDALIS_SHARED_DIR}/cmake/clap_entry.version"
      "-Wl,--no-undefined"
   )
endif()

set_target_properties(ThunderClap PROPERTIES
   PREFIX ""
   OUTPUT_NAME "ThunderClap"
   SUFFIX ".clap"
)

# ------------------------------------------------------------- offline test host
if(THUNDERCLAP_BUILD_TOOLS)
   # The preset text format is linked in as well as loaded through the plugin,
   # so the self-test can check that writing a preset and reading it back agree.
   add_executable(thunderclap-render tools/render.cpp src/preset.cpp src/params.cpp)
   target_link_libraries(thunderclap-render PRIVATE verdalis::shared)
   target_include_directories(thunderclap-render PRIVATE
      "${CLAP_INCLUDE_DIR}"
      "${CMAKE_CURRENT_SOURCE_DIR}/src"
   )
   target_compile_options(thunderclap-render PRIVATE -Wall -Wextra)
   target_link_libraries(thunderclap-render PRIVATE ${CMAKE_DL_LIBS})
   if(WIN32)
      # Same reasoning as the plugin: the test host has to run on a machine that
      # has never had a compiler on it.
      target_link_options(thunderclap-render PRIVATE "-static-libgcc" "-static-libstdc++"
         "-Wl,-Bstatic,--whole-archive" "-lwinpthread" "-Wl,--no-whole-archive,-Bdynamic")
   endif()

   # Renders preset files back to back inside one process, which is what makes
   # fitting the library against reference recordings practical. See
   # tools/analysis/.
   if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
      if(THUNDERCLAP_BUILD_GUI)
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
      add_executable(thunderclap-guihost tools/guihost.cpp)
      target_include_directories(thunderclap-guihost PRIVATE "${CLAP_INCLUDE_DIR}" ${GUIHOST_X11_INCLUDE_DIRS})
      target_compile_options(thunderclap-guihost PRIVATE -Wall -Wextra)
      target_link_libraries(thunderclap-guihost PRIVATE ${CMAKE_DL_LIBS} ${GUIHOST_X11_LIBRARIES})
      if(WIN32)
         target_link_options(thunderclap-guihost PRIVATE "-static-libgcc" "-static-libstdc++"
            "-Wl,-Bstatic,--whole-archive" "-lwinpthread" "-Wl,--no-whole-archive,-Bdynamic")
      endif()
   endif()

   # The fitting host and the window host are both part of the Linux analysis
   # workflow and have no reason to exist on Windows.
   if(NOT CMAKE_SYSTEM_NAME STREQUAL "Windows")
   add_executable(thunderclap-fithost "${VERDALIS_SHARED_DIR}/tools/fithost.cpp")
   target_include_directories(thunderclap-fithost PRIVATE "${CLAP_INCLUDE_DIR}")
   target_compile_options(thunderclap-fithost PRIVATE -Wall -Wextra)
   target_link_libraries(thunderclap-fithost PRIVATE ${CMAKE_DL_LIBS})
   endif()
endif()

# ---------------------------------------------------------------------- install
# The plugin looks for a "presets" directory next to its own binary, so both
# land in the same ThunderClap/ folder.
install(TARGETS ThunderClap LIBRARY DESTINATION "ThunderClap")
install(FILES ${THUNDERCLAP_PRESET_FILES} DESTINATION "ThunderClap/presets")

message(STATUS "ThunderClap: 'cmake --install' will write to ${CMAKE_INSTALL_PREFIX}/ThunderClap")
