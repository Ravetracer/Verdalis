cmake_minimum_required(VERSION 3.16)
project(InsectSwarm VERSION 0.2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
   set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

option(INSECTSWARM_BUILD_TOOLS "Build the offline render/verification host" ON)
option(INSECTSWARM_BUILD_GUI "Build the plugin window (needs X11 and Cairo)" ON)
option(INSECTSWARM_BUILD_VST3 "Also build a VST3 via the clap-wrapper" OFF)

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
message(STATUS "InsectSwarm: using CLAP headers from ${CLAP_INCLUDE_DIR}")

# ------------------------------------------------------- embedded preset table
set(INSECTSWARM_PRESET_DIR "${CMAKE_CURRENT_SOURCE_DIR}/presets")
file(GLOB INSECTSWARM_PRESET_FILES "${INSECTSWARM_PRESET_DIR}/*.insectswarm")
set(INSECTSWARM_GENERATED_HEADER "${CMAKE_CURRENT_BINARY_DIR}/generated/presets_generated.h")

add_custom_command(
   OUTPUT "${INSECTSWARM_GENERATED_HEADER}"
   COMMAND "${CMAKE_COMMAND}"
      -DPRESET_DIR=${INSECTSWARM_PRESET_DIR}
      -DOUTPUT=${INSECTSWARM_GENERATED_HEADER}
      -DNAMESPACE=insectswarm
      -DPRESET_EXT=insectswarm
      -P "${VERDALIS_SHARED_DIR}/cmake/embed_presets.cmake"
   DEPENDS ${INSECTSWARM_PRESET_FILES} "${VERDALIS_SHARED_DIR}/cmake/embed_presets.cmake"
   COMMENT "Embedding ${CMAKE_CURRENT_SOURCE_DIR}/presets into presets_generated.h"
   VERBATIM
)
add_custom_target(insectswarm_presets DEPENDS "${INSECTSWARM_GENERATED_HEADER}")

# ------------------------------------------------------------------ gui deps
#
# X11 and Cairo only. Both are already on any Linux box that can run a DAW, and
# using them directly keeps InsectSwarm a single self-contained .clap file.
set(INSECTSWARM_GUI_SOURCES "")
# pkg-config on this machine describes this machine, so a cross build must not
# consult it: it would report the host's X11 and Cairo and drag /usr/include
# into a Windows compile. Point INSECTSWARM_WIN_CAIRO at a Cairo cross-built with
# its win32 backend instead (see ../shared/cmake/build-windows-cairo.sh,
# or run ../setup-winbuild.sh once).
if(INSECTSWARM_BUILD_GUI AND CMAKE_SYSTEM_NAME STREQUAL "Windows")
   if(NOT INSECTSWARM_WIN_CAIRO OR NOT EXISTS "${INSECTSWARM_WIN_CAIRO}/lib/libcairo.a")
      set(INSECTSWARM_BUILD_GUI OFF)
      message(WARNING "InsectSwarm: no Windows Cairo at INSECTSWARM_WIN_CAIRO, building without a window")
   else()
      set(INSECTSWARM_GUI_DEPS_INCLUDE_DIRS "${INSECTSWARM_WIN_CAIRO}/include" "${INSECTSWARM_WIN_CAIRO}/include/cairo")
      set(INSECTSWARM_GUI_DEPS_LIBRARY_DIRS "${INSECTSWARM_WIN_CAIRO}/lib")
      # Cairo's win32 backend draws through GDI and measures text with it.
      set(INSECTSWARM_GUI_DEPS_LIBRARIES cairo pixman-1 gdi32 msimg32 user32)
      set(INSECTSWARM_GUI_SOURCES src/gui/gui.cpp)
      # Cairo's headers declare everything dllimport unless told the library is
      # static, which is how it is built here.
      set(INSECTSWARM_GUI_DEFINES CAIRO_WIN32_STATIC_BUILD)
      set(INSECTSWARM_GUI_FOUND_WIN TRUE)
      message(STATUS "InsectSwarm: building the plugin window (Win32 + Cairo)")
   endif()
endif()

if(INSECTSWARM_BUILD_GUI AND NOT INSECTSWARM_GUI_FOUND_WIN)
   find_package(PkgConfig QUIET)
   if(PkgConfig_FOUND)
      pkg_check_modules(INSECTSWARM_GUI_DEPS QUIET x11 cairo)
   endif()
   if(INSECTSWARM_GUI_DEPS_FOUND)
      set(INSECTSWARM_GUI_SOURCES src/gui/gui.cpp)
      message(STATUS "InsectSwarm: building the plugin window (X11 + Cairo)")
   else()
      set(INSECTSWARM_BUILD_GUI OFF)
      message(WARNING "InsectSwarm: x11/cairo not found, building without a GUI")
   endif()
endif()

# ------------------------------------------------------------------ shared code
#
# The suite's common parameter model and DSP building blocks. Lives beside the
# plugins at the suite root, so it is reached with an explicit path.
# The window lives in shared/; hand it what this plugin discovered above, since
# a cross build must not let the shared CMakeLists consult this machine.
if(INSECTSWARM_BUILD_GUI)
   set(VERDALIS_WITH_GUI ON)
   set(VERDALIS_GUI_INCLUDE_DIRS ${INSECTSWARM_GUI_DEPS_INCLUDE_DIRS})
   set(VERDALIS_GUI_LIBRARY_DIRS ${INSECTSWARM_GUI_DEPS_LIBRARY_DIRS})
   set(VERDALIS_GUI_LIBRARIES ${INSECTSWARM_GUI_DEPS_LIBRARIES})
   set(VERDALIS_GUI_DEFINES ${INSECTSWARM_GUI_DEFINES})
endif()

add_subdirectory("${VERDALIS_SHARED_DIR}" verdalis-shared)

# --------------------------------------------------------- the implementation
#
# The plugin itself is a static library rather than the module directly, so that
# more than one plugin format can be built from one copy of it. The .clap below
# and the .vst3 further down each add only their own entry point and link this.
add_library(InsectSwarm-impl STATIC
   src/plugin.cpp
   src/preset_provider.cpp
   src/preset.cpp
   src/params.cpp
   src/dsp/swarm_engine.cpp
   ${INSECTSWARM_GUI_SOURCES}
   "${INSECTSWARM_GENERATED_HEADER}"
)
add_dependencies(InsectSwarm-impl insectswarm_presets)

target_link_libraries(InsectSwarm-impl PUBLIC verdalis::shared)

target_include_directories(InsectSwarm-impl PUBLIC
   "${CLAP_INCLUDE_DIR}"
   "${CMAKE_CURRENT_SOURCE_DIR}/src"
   "${CMAKE_CURRENT_BINARY_DIR}/generated"
)

target_compile_options(InsectSwarm-impl PRIVATE
   -Wall -Wextra
   -fvisibility=hidden
   # Deliberately NOT -ffast-math: on x86 GCC that links crtfastmath.o, which
   # flips FTZ/DAZ for the whole host process. A plugin has no business doing
   # that to its host.
   -fno-math-errno
)

target_link_libraries(InsectSwarm-impl PUBLIC ${CMAKE_DL_LIBS})

# The version lives in two places that have to agree: project() above and
# kPluginVersion in src/insectswarm.h. Handing the project version to the compiler
# lets insectswarm.h static_assert that they match, so a bump that only lands in
# one of them fails the build instead of shipping.
target_compile_definitions(InsectSwarm-impl PRIVATE
   INSECTSWARM_CMAKE_VERSION="${PROJECT_VERSION}")

if(INSECTSWARM_BUILD_GUI)
   target_compile_definitions(InsectSwarm-impl PRIVATE INSECTSWARM_WITH_GUI ${INSECTSWARM_GUI_DEFINES})
   target_include_directories(InsectSwarm-impl PUBLIC ${INSECTSWARM_GUI_DEPS_INCLUDE_DIRS})
   target_link_libraries(InsectSwarm-impl PUBLIC ${INSECTSWARM_GUI_DEPS_LIBRARIES})
   target_link_directories(InsectSwarm-impl PUBLIC ${INSECTSWARM_GUI_DEPS_LIBRARY_DIRS})
   find_package(Threads REQUIRED)
   target_link_libraries(InsectSwarm-impl PUBLIC Threads::Threads)
endif()


# --------------------------------------------------------------- the CLAP module
add_library(InsectSwarm MODULE src/entry.cpp)
target_link_libraries(InsectSwarm PRIVATE InsectSwarm-impl)
target_compile_options(InsectSwarm PRIVATE -Wall -Wextra -fvisibility=hidden)

# Only clap_entry is exported; everything else stays internal to the DSO.
if(WIN32)
   # A version script is an ELF idea. On Windows clap_entry carries dllexport
   # from CLAP_EXPORT, and --exclude-all-symbols keeps the rest of the DLL to
   # itself. The runtime is linked in so the .clap needs no MinGW DLLs beside
   # it on a machine that has never seen a compiler.
   target_link_options(InsectSwarm PRIVATE
      "-Wl,--exclude-all-symbols"
      "-Wl,--no-undefined"
      "-static-libgcc" "-static-libstdc++"
      "-Wl,-Bstatic,--whole-archive" "-lwinpthread" "-Wl,--no-whole-archive,-Bdynamic"
   )
else()
   target_link_options(InsectSwarm PRIVATE
      "-Wl,--version-script=${VERDALIS_SHARED_DIR}/cmake/clap_entry.version"
      "-Wl,--no-undefined"
   )
endif()

set_target_properties(InsectSwarm PROPERTIES
   PREFIX ""
   OUTPUT_NAME "InsectSwarm"
   SUFFIX ".clap"
)

# ------------------------------------------------------------- offline test host
if(INSECTSWARM_BUILD_TOOLS)
   # The preset text format is linked in as well as loaded through the plugin,
   # so the self-test can check that writing a preset and reading it back agree.
   add_executable(insectswarm-render tools/render.cpp src/preset.cpp src/params.cpp)
   target_link_libraries(insectswarm-render PRIVATE verdalis::shared)
   target_include_directories(insectswarm-render PRIVATE
      "${CLAP_INCLUDE_DIR}"
      "${CMAKE_CURRENT_SOURCE_DIR}/src"
   )
   target_compile_options(insectswarm-render PRIVATE -Wall -Wextra)
   target_link_libraries(insectswarm-render PRIVATE ${CMAKE_DL_LIBS})
   if(WIN32)
      # Same reasoning as the plugin: the test host has to run on a machine that
      # has never had a compiler on it.
      target_link_options(insectswarm-render PRIVATE "-static-libgcc" "-static-libstdc++"
         "-Wl,-Bstatic,--whole-archive" "-lwinpthread" "-Wl,--no-whole-archive,-Bdynamic")
   endif()

   # Renders preset files back to back inside one process, which is what makes
   # fitting the library against reference recordings practical. See
   # tools/analysis/.
   if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
      if(INSECTSWARM_BUILD_GUI)
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
      add_executable(insectswarm-guihost tools/guihost.cpp)
      target_include_directories(insectswarm-guihost PRIVATE "${CLAP_INCLUDE_DIR}" ${GUIHOST_X11_INCLUDE_DIRS})
      target_compile_options(insectswarm-guihost PRIVATE -Wall -Wextra)
      target_link_libraries(insectswarm-guihost PRIVATE ${CMAKE_DL_LIBS} ${GUIHOST_X11_LIBRARIES})
      if(WIN32)
         target_link_options(insectswarm-guihost PRIVATE "-static-libgcc" "-static-libstdc++"
            "-Wl,-Bstatic,--whole-archive" "-lwinpthread" "-Wl,--no-whole-archive,-Bdynamic")
      endif()
   endif()

   # The fitting host and the window host are both part of the Linux analysis
   # workflow and have no reason to exist on Windows.
   if(NOT CMAKE_SYSTEM_NAME STREQUAL "Windows")
   add_executable(insectswarm-fithost "${VERDALIS_SHARED_DIR}/tools/fithost.cpp")
   target_include_directories(insectswarm-fithost PRIVATE "${CLAP_INCLUDE_DIR}")
   target_compile_options(insectswarm-fithost PRIVATE -Wall -Wextra)
   target_link_libraries(insectswarm-fithost PRIVATE ${CMAKE_DL_LIBS})
   endif()
endif()

# ------------------------------------------------------------------------ VST3
#
# The VST3 is the same plugin, not a port: the clap-wrapper implements a CLAP
# host against the VST3 API and loads InsectSwarm-impl through it, so there is one
# engine, one parameter table and one window behind both formats.
#
# It is off by default because it needs two more checkouts beside CLAP/clap --
# CLAP/clap-wrapper and CLAP/vst3sdk (VST 3.8 or newer, which is MIT licensed).
if(INSECTSWARM_BUILD_VST3)
   get_filename_component(INSECTSWARM_CLAP_DIR "${CLAP_INCLUDE_DIR}/../.." ABSOLUTE)
   set(INSECTSWARM_WRAPPER_DIR "${INSECTSWARM_CLAP_DIR}/clap-wrapper"
      CACHE PATH "Directory containing a clap-wrapper checkout")
   set(INSECTSWARM_VST3_SDK_DIR "${INSECTSWARM_CLAP_DIR}/vst3sdk"
      CACHE PATH "Directory containing a VST3 SDK checkout")

   if(NOT EXISTS "${INSECTSWARM_WRAPPER_DIR}/cmake/wrap_vst3.cmake")
      message(FATAL_ERROR "InsectSwarm: no clap-wrapper at ${INSECTSWARM_WRAPPER_DIR}")
   endif()
   if(NOT EXISTS "${INSECTSWARM_VST3_SDK_DIR}/public.sdk/source/main/pluginfactory.cpp")
      message(FATAL_ERROR "InsectSwarm: no VST3 SDK at ${INSECTSWARM_VST3_SDK_DIR}")
   endif()

   # The wrapper resolves both SDKs itself; hand it the checkouts beside this
   # plugin rather than letting it download its own.
   set(CLAP_SDK_ROOT "${INSECTSWARM_CLAP_DIR}/clap" CACHE PATH "" FORCE)
   set(VST3_SDK_ROOT "${INSECTSWARM_VST3_SDK_DIR}" CACHE PATH "" FORCE)
   set(CLAP_WRAPPER_DOWNLOAD_DEPENDENCIES FALSE CACHE BOOL "" FORCE)
   set(CLAP_WRAPPER_OUTPUT_NAME "InsectSwarm" CACHE STRING "" FORCE)
   set(CLAP_WRAPPER_BUILD_TESTS FALSE CACHE BOOL "" FORCE)

   add_subdirectory("${INSECTSWARM_WRAPPER_DIR}" clap-wrapper EXCLUDE_FROM_ALL)

   # The bundle is assembled in place, so point it somewhere predictable rather
   # than at whatever the generator picks per configuration.
   set(INSECTSWARM_VST3_ASSET_DIR "${CMAKE_BINARY_DIR}/vst3")

   add_library(InsectSwarm-vst3 MODULE src/entry.cpp)
   target_link_libraries(InsectSwarm-vst3 PRIVATE InsectSwarm-impl)
   target_compile_options(InsectSwarm-vst3 PRIVATE -Wall -Wextra)

   target_add_vst3_wrapper(
      TARGET InsectSwarm-vst3
      OUTPUT_NAME "InsectSwarm"
      BUNDLE_IDENTIFIER "de.ravetracer.insectswarm.vst3"
      BUNDLE_VERSION "${PROJECT_VERSION}"
      ASSET_OUTPUT_DIRECTORY "${INSECTSWARM_VST3_ASSET_DIR}"
   )

   # Two things the wrapper leaves to the plugin on a mingw cross build.
   if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
      # CMake gives a MODULE library a "lib" prefix with the GNU toolchain, and
      # the wrapper only clears it on the single-file path. A VST3 bundle's
      # binary has to be named exactly like the bundle.
      set_target_properties(InsectSwarm-vst3 PROPERTIES PREFIX "")
      # Same reasoning as the .clap: the runtime is linked in so the plugin
      # needs no MinGW DLLs beside it. GetPluginFactory carries dllexport from
      # SMTG_EXPORT_SYMBOL, so unlike the .clap there is no version script and
      # no --exclude-all-symbols here.
      target_link_options(InsectSwarm-vst3 PRIVATE
         "-Wl,--no-undefined"
         "-static-libgcc" "-static-libstdc++"
         "-Wl,-Bstatic,--whole-archive" "-lwinpthread" "-Wl,--no-whole-archive,-Bdynamic"
      )
   endif()

   # factoryPresetDir() checks ../Resources/presets, which is where a VST3
   # bundle keeps its data.
   add_custom_command(TARGET InsectSwarm-vst3 POST_BUILD
      COMMAND "${CMAKE_COMMAND}" -E make_directory
         "${INSECTSWARM_VST3_ASSET_DIR}/InsectSwarm.vst3/Contents/Resources/presets"
      COMMAND "${CMAKE_COMMAND}" -E copy_if_different ${INSECTSWARM_PRESET_FILES}
         "${INSECTSWARM_VST3_ASSET_DIR}/InsectSwarm.vst3/Contents/Resources/presets"
      COMMENT "Staging presets into the VST3 bundle"
      VERBATIM
   )

   # VST3s do not live in ~/.clap, so this one is installed on its own rather
   # than through CMAKE_INSTALL_PREFIX.
   set(INSECTSWARM_VST3_INSTALL_DIR "$ENV{HOME}/.vst3"
      CACHE PATH "Where 'cmake --build . --target InsectSwarm-vst3-install' puts the bundle")
   add_custom_target(InsectSwarm-vst3-install
      COMMAND "${CMAKE_COMMAND}" -E make_directory "${INSECTSWARM_VST3_INSTALL_DIR}"
      COMMAND "${CMAKE_COMMAND}" -E rm -rf "${INSECTSWARM_VST3_INSTALL_DIR}/InsectSwarm.vst3"
      COMMAND "${CMAKE_COMMAND}" -E copy_directory
         "${INSECTSWARM_VST3_ASSET_DIR}/InsectSwarm.vst3"
         "${INSECTSWARM_VST3_INSTALL_DIR}/InsectSwarm.vst3"
      DEPENDS InsectSwarm-vst3
      COMMENT "Installing InsectSwarm.vst3 into ${INSECTSWARM_VST3_INSTALL_DIR}"
      VERBATIM
   )
endif()

# ---------------------------------------------------------------------- install
# The plugin looks for a "presets" directory next to its own binary, so both
# land in the same InsectSwarm/ folder.
install(TARGETS InsectSwarm LIBRARY DESTINATION "InsectSwarm")
install(FILES ${INSECTSWARM_PRESET_FILES} DESTINATION "InsectSwarm/presets")

message(STATUS "InsectSwarm: 'cmake --install' will write to ${CMAKE_INSTALL_PREFIX}/InsectSwarm")
