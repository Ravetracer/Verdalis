cmake_minimum_required(VERSION 3.16)
project(VerdaliScene VERSION 0.4.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
   set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

option(VERDALISCENE_BUILD_TOOLS "Build the offline render/verification host" ON)
option(VERDALISCENE_BUILD_GUI "Build the plugin window (needs X11 and Cairo)" ON)
option(VERDALISCENE_BUILD_VST3 "Also build a VST3 via the clap-wrapper" OFF)

# CLAP plugins live in ~/.clap on Linux, so default there rather than /usr/local.
if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
   set(CMAKE_INSTALL_PREFIX "$ENV{HOME}/.clap" CACHE PATH "Install prefix" FORCE)
endif()

# The suite's shared code, and the suite root the layers are built from.
get_filename_component(VERDALIS_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)
set(VERDALIS_SHARED_DIR "${VERDALIS_ROOT}/shared")

# The instruments a scene layers. Each is compiled from its own folder -- its
# engine, its parameter table, its preset bindings, its embedded factory
# presets and, with a window, its panel layout -- so VerdaliScene carries no
# copy of any of them. WhooshPact is not here on purpose: it makes production
# sounds, not a place.
set(VERDALISCENE_LAYERS
   rainyday thunderclap shorebreak skyhowl chirpparade riverflow crackleblaze insectswarm nightlife)

# ---------------------------------------------------------------- CLAP headers
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
message(STATUS "VerdaliScene: using CLAP headers from ${CLAP_INCLUDE_DIR}")

# ------------------------------------------------------- embedded preset tables
#
# The scenes, and every layer's own factory library: a layer's preset browser
# offers its plugin's factory presets from this binary, exactly as the plugin
# does from its own.
function(verdaliscene_embed_presets ns ext dir out_var)
   file(GLOB _files "${dir}/*.${ext}")
   set(_header "${CMAKE_CURRENT_BINARY_DIR}/generated/${ns}/presets_generated.h")
   add_custom_command(
      OUTPUT "${_header}"
      COMMAND "${CMAKE_COMMAND}"
         -DPRESET_DIR=${dir}
         -DOUTPUT=${_header}
         -DNAMESPACE=${ns}
         -DPRESET_EXT=${ext}
         -P "${VERDALIS_SHARED_DIR}/cmake/embed_presets.cmake"
      DEPENDS ${_files} "${VERDALIS_SHARED_DIR}/cmake/embed_presets.cmake"
      COMMENT "Embedding ${dir} into ${ns}/presets_generated.h"
      VERBATIM
   )
   set(${out_var} "${_header}" PARENT_SCOPE)
endfunction()

set(VERDALISCENE_PRESET_DIR "${CMAKE_CURRENT_SOURCE_DIR}/presets")
file(GLOB VERDALISCENE_PRESET_FILES "${VERDALISCENE_PRESET_DIR}/*.verdaliscene")
verdaliscene_embed_presets(verdaliscene verdaliscene "${VERDALISCENE_PRESET_DIR}"
   VERDALISCENE_GENERATED_HEADER)

# ------------------------------------------------------------------ gui deps
set(VERDALISCENE_GUI_SOURCES "")
if(VERDALISCENE_BUILD_GUI AND CMAKE_SYSTEM_NAME STREQUAL "Windows")
   if(NOT VERDALISCENE_WIN_CAIRO OR NOT EXISTS "${VERDALISCENE_WIN_CAIRO}/lib/libcairo.a")
      set(VERDALISCENE_BUILD_GUI OFF)
      message(WARNING "VerdaliScene: no Windows Cairo at VERDALISCENE_WIN_CAIRO, building without a window")
   else()
      set(VERDALISCENE_GUI_DEPS_INCLUDE_DIRS "${VERDALISCENE_WIN_CAIRO}/include" "${VERDALISCENE_WIN_CAIRO}/include/cairo")
      set(VERDALISCENE_GUI_DEPS_LIBRARY_DIRS "${VERDALISCENE_WIN_CAIRO}/lib")
      set(VERDALISCENE_GUI_DEPS_LIBRARIES cairo pixman-1 gdi32 msimg32 user32 comdlg32 shell32)
      set(VERDALISCENE_GUI_SOURCES src/gui/gui.cpp)
      set(VERDALISCENE_GUI_DEFINES CAIRO_WIN32_STATIC_BUILD)
      set(VERDALISCENE_GUI_FOUND_WIN TRUE)
      message(STATUS "VerdaliScene: building the plugin window (Win32 + Cairo)")
   endif()
endif()

if(VERDALISCENE_BUILD_GUI AND NOT VERDALISCENE_GUI_FOUND_WIN)
   find_package(PkgConfig QUIET)
   if(PkgConfig_FOUND)
      pkg_check_modules(VERDALISCENE_GUI_DEPS QUIET x11 cairo)
   endif()
   if(VERDALISCENE_GUI_DEPS_FOUND)
      set(VERDALISCENE_GUI_SOURCES src/gui/gui.cpp)
      message(STATUS "VerdaliScene: building the plugin window (X11 + Cairo)")
   else()
      set(VERDALISCENE_BUILD_GUI OFF)
      message(WARNING "VerdaliScene: x11/cairo not found, building without a GUI")
   endif()
endif()

# ------------------------------------------------------------------ shared code
if(VERDALISCENE_BUILD_GUI)
   set(VERDALIS_WITH_GUI ON)
   set(VERDALIS_GUI_INCLUDE_DIRS ${VERDALISCENE_GUI_DEPS_INCLUDE_DIRS})
   set(VERDALIS_GUI_LIBRARY_DIRS ${VERDALISCENE_GUI_DEPS_LIBRARY_DIRS})
   set(VERDALIS_GUI_LIBRARIES ${VERDALISCENE_GUI_DEPS_LIBRARIES})
   set(VERDALIS_GUI_DEFINES ${VERDALISCENE_GUI_DEFINES})
endif()

add_subdirectory("${VERDALIS_SHARED_DIR}" verdalis-shared)

set(VERDALISCENE_COMPILE_OPTIONS
   -Wall -Wextra
   -fvisibility=hidden
   # Deliberately NOT -ffast-math: on x86 GCC that links crtfastmath.o, which
   # flips FTZ/DAZ for the whole host process. A plugin has no business doing
   # that to its host.
   -fno-math-errno
)

# ------------------------------------------------------------------- the layers
#
# One object library per instrument, compiled with that plugin's own src/ as
# its include directory and nothing of VerdaliScene's: the adapter in
# src/layers/ reaches VerdaliScene's registry by relative path, so a plugin's
# "params.h" and VerdaliScene's never meet. Every plugin keeps its code in its
# own namespace, which is what makes linking nine of them into one binary safe.
set(VERDALISCENE_LAYER_OBJECTS "")
foreach(layer ${VERDALISCENE_LAYERS})
   set(_dir "${VERDALIS_ROOT}/${layer}")
   verdaliscene_embed_presets(${layer} ${layer} "${_dir}/presets" _header)
   file(GLOB _engine "${_dir}/src/dsp/*.cpp")
   set(_sources
      src/layers/${layer}.cpp
      "${_dir}/src/params.cpp"
      "${_dir}/src/engine_params.cpp"
      "${_dir}/src/preset.cpp"
      ${_engine}
      "${_header}"
   )
   if(VERDALISCENE_BUILD_GUI)
      list(APPEND _sources "${_dir}/src/gui/gui.cpp")
   endif()
   add_library(vs_layer_${layer} OBJECT ${_sources})
   target_include_directories(vs_layer_${layer} PRIVATE
      "${_dir}/src"
      "${CMAKE_CURRENT_BINARY_DIR}/generated/${layer}"
   )
   target_link_libraries(vs_layer_${layer} PRIVATE verdalis::shared)
   target_compile_options(vs_layer_${layer} PRIVATE ${VERDALISCENE_COMPILE_OPTIONS})
   # The wind layer is SkyHowl's engine without its one hidden function and the
   # recordings that function plays: VerdaliScene cannot reach it, and stays
   # synthesis only.
   target_compile_definitions(vs_layer_${layer} PRIVATE SKYHOWL_WITHOUT_VENTS)
   if(VERDALISCENE_BUILD_GUI)
      target_compile_definitions(vs_layer_${layer} PRIVATE VERDALISCENE_WITH_GUI
         ${VERDALISCENE_GUI_DEFINES})
   endif()
   list(APPEND VERDALISCENE_LAYER_OBJECTS $<TARGET_OBJECTS:vs_layer_${layer}>)
endforeach()

# --------------------------------------------------------- the implementation
add_library(VerdaliScene-impl STATIC
   src/plugin.cpp
   src/preset_provider.cpp
   src/preset.cpp
   src/params.cpp
   src/table.cpp
   src/layers.cpp
   src/scene_preset.cpp
   ${VERDALISCENE_GUI_SOURCES}
   ${VERDALISCENE_LAYER_OBJECTS}
   "${VERDALISCENE_GENERATED_HEADER}"
)

target_link_libraries(VerdaliScene-impl PUBLIC verdalis::shared)

target_include_directories(VerdaliScene-impl PUBLIC
   "${CLAP_INCLUDE_DIR}"
   "${CMAKE_CURRENT_SOURCE_DIR}/src"
   "${CMAKE_CURRENT_BINARY_DIR}/generated/verdaliscene"
)

target_compile_options(VerdaliScene-impl PRIVATE ${VERDALISCENE_COMPILE_OPTIONS})

target_link_libraries(VerdaliScene-impl PUBLIC ${CMAKE_DL_LIBS})

target_compile_definitions(VerdaliScene-impl PRIVATE
   VERDALISCENE_CMAKE_VERSION="${PROJECT_VERSION}")

if(VERDALISCENE_BUILD_GUI)
   target_compile_definitions(VerdaliScene-impl PRIVATE VERDALISCENE_WITH_GUI
      ${VERDALISCENE_GUI_DEFINES})
   target_include_directories(VerdaliScene-impl PUBLIC ${VERDALISCENE_GUI_DEPS_INCLUDE_DIRS})
   target_link_libraries(VerdaliScene-impl PUBLIC ${VERDALISCENE_GUI_DEPS_LIBRARIES})
   target_link_directories(VerdaliScene-impl PUBLIC ${VERDALISCENE_GUI_DEPS_LIBRARY_DIRS})
   find_package(Threads REQUIRED)
   target_link_libraries(VerdaliScene-impl PUBLIC Threads::Threads)
endif()

# --------------------------------------------------------------- the CLAP module
add_library(VerdaliScene MODULE src/entry.cpp)
target_link_libraries(VerdaliScene PRIVATE VerdaliScene-impl)
target_compile_options(VerdaliScene PRIVATE -Wall -Wextra -fvisibility=hidden)

if(WIN32)
   target_link_options(VerdaliScene PRIVATE
      "-Wl,--exclude-all-symbols"
      "-Wl,--no-undefined"
      "-static-libgcc" "-static-libstdc++"
      "-Wl,-Bstatic,--whole-archive" "-lwinpthread" "-Wl,--no-whole-archive,-Bdynamic"
   )
else()
   target_link_options(VerdaliScene PRIVATE
      "-Wl,--version-script=${VERDALIS_SHARED_DIR}/cmake/clap_entry.version"
      "-Wl,--no-undefined"
   )
endif()

set_target_properties(VerdaliScene PROPERTIES
   PREFIX ""
   OUTPUT_NAME "VerdaliScene"
   SUFFIX ".clap"
)

# ------------------------------------------------------------- offline test host
if(VERDALISCENE_BUILD_TOOLS)
   # The scene format and the layers are linked in as well as loaded through
   # the plugin, so the self-test can check every preset of every plugin
   # against the code that reads and writes them.
   add_executable(verdaliscene-render tools/render.cpp)
   target_link_libraries(verdaliscene-render PRIVATE VerdaliScene-impl)
   target_compile_options(verdaliscene-render PRIVATE -Wall -Wextra)
   target_link_libraries(verdaliscene-render PRIVATE ${CMAKE_DL_LIBS})
   if(WIN32)
      target_link_options(verdaliscene-render PRIVATE "-static-libgcc" "-static-libstdc++"
         "-Wl,-Bstatic,--whole-archive" "-lwinpthread" "-Wl,--no-whole-archive,-Bdynamic")
   endif()

   if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
      if(VERDALISCENE_BUILD_GUI)
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
      add_executable(verdaliscene-guihost tools/guihost.cpp)
      target_include_directories(verdaliscene-guihost PRIVATE "${CLAP_INCLUDE_DIR}" ${GUIHOST_X11_INCLUDE_DIRS})
      target_compile_options(verdaliscene-guihost PRIVATE -Wall -Wextra)
      target_link_libraries(verdaliscene-guihost PRIVATE ${CMAKE_DL_LIBS} ${GUIHOST_X11_LIBRARIES})
      if(WIN32)
         target_link_options(verdaliscene-guihost PRIVATE "-static-libgcc" "-static-libstdc++"
            "-Wl,-Bstatic,--whole-archive" "-lwinpthread" "-Wl,--no-whole-archive,-Bdynamic")
      endif()
   endif()

   if(NOT CMAKE_SYSTEM_NAME STREQUAL "Windows")
      add_executable(verdaliscene-fithost "${VERDALIS_SHARED_DIR}/tools/fithost.cpp")
      target_include_directories(verdaliscene-fithost PRIVATE "${CLAP_INCLUDE_DIR}")
      target_compile_options(verdaliscene-fithost PRIVATE -Wall -Wextra)
      target_link_libraries(verdaliscene-fithost PRIVATE ${CMAKE_DL_LIBS})
   endif()
endif()

# ------------------------------------------------------------------------ VST3
#
# The VST3 is the same plugin, not a port: the clap-wrapper implements a CLAP
# host against the VST3 API and loads VerdaliScene-impl through it.
if(VERDALISCENE_BUILD_VST3)
   get_filename_component(VERDALISCENE_CLAP_DIR "${CLAP_INCLUDE_DIR}/../.." ABSOLUTE)
   set(VERDALISCENE_WRAPPER_DIR "${VERDALISCENE_CLAP_DIR}/clap-wrapper"
      CACHE PATH "Directory containing a clap-wrapper checkout")
   set(VERDALISCENE_VST3_SDK_DIR "${VERDALISCENE_CLAP_DIR}/vst3sdk"
      CACHE PATH "Directory containing a VST3 SDK checkout")

   if(NOT EXISTS "${VERDALISCENE_WRAPPER_DIR}/cmake/wrap_vst3.cmake")
      message(FATAL_ERROR "VerdaliScene: no clap-wrapper at ${VERDALISCENE_WRAPPER_DIR}")
   endif()
   if(NOT EXISTS "${VERDALISCENE_VST3_SDK_DIR}/public.sdk/source/main/pluginfactory.cpp")
      message(FATAL_ERROR "VerdaliScene: no VST3 SDK at ${VERDALISCENE_VST3_SDK_DIR}")
   endif()

   set(CLAP_SDK_ROOT "${VERDALISCENE_CLAP_DIR}/clap" CACHE PATH "" FORCE)
   set(VST3_SDK_ROOT "${VERDALISCENE_VST3_SDK_DIR}" CACHE PATH "" FORCE)
   set(CLAP_WRAPPER_DOWNLOAD_DEPENDENCIES FALSE CACHE BOOL "" FORCE)
   set(CLAP_WRAPPER_OUTPUT_NAME "VerdaliScene" CACHE STRING "" FORCE)
   set(CLAP_WRAPPER_BUILD_TESTS FALSE CACHE BOOL "" FORCE)

   add_subdirectory("${VERDALISCENE_WRAPPER_DIR}" clap-wrapper EXCLUDE_FROM_ALL)

   set(VERDALISCENE_VST3_ASSET_DIR "${CMAKE_BINARY_DIR}/vst3")

   add_library(VerdaliScene-vst3 MODULE src/entry.cpp)
   target_link_libraries(VerdaliScene-vst3 PRIVATE VerdaliScene-impl)
   target_compile_options(VerdaliScene-vst3 PRIVATE -Wall -Wextra)

   target_add_vst3_wrapper(
      TARGET VerdaliScene-vst3
      OUTPUT_NAME "VerdaliScene"
      BUNDLE_IDENTIFIER "de.ravetracer.verdaliscene.vst3"
      BUNDLE_VERSION "${PROJECT_VERSION}"
      ASSET_OUTPUT_DIRECTORY "${VERDALISCENE_VST3_ASSET_DIR}"
   )

   # The wrapper finds the CLAP it wraps either inside this binary or as a .clap
   # installed somewhere else. On Linux its look inside the binary never works
   # -- it dlsym()s a handle it has not opened yet, and the version script below
   # keeps clap_entry unexported anyway -- so without this the .vst3 silently
   # loaded whatever .clap of the same name it found in ~/.clap, and nothing at
   # all where there was none. Linked statically, the entry is taken by address.
   target_compile_definitions(clap-wrapper-shared-detail PRIVATE STATICALLY_LINKED_CLAP_ENTRY=1)

   if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
      set_target_properties(VerdaliScene-vst3 PROPERTIES PREFIX "")
      target_link_options(VerdaliScene-vst3 PRIVATE
         "-Wl,--no-undefined"
         "-static-libgcc" "-static-libstdc++"
         "-Wl,-Bstatic,--whole-archive" "-lwinpthread" "-Wl,--no-whole-archive,-Bdynamic"
      )
   else()
      target_link_options(VerdaliScene-vst3 PRIVATE
         "-Wl,--version-script=${VERDALIS_SHARED_DIR}/cmake/vst3_entry.version"
      )
   endif()

   add_custom_command(TARGET VerdaliScene-vst3 POST_BUILD
      COMMAND "${CMAKE_COMMAND}" -E make_directory
         "${VERDALISCENE_VST3_ASSET_DIR}/VerdaliScene.vst3/Contents/Resources/presets"
      COMMAND "${CMAKE_COMMAND}" -E copy_if_different ${VERDALISCENE_PRESET_FILES}
         "${VERDALISCENE_VST3_ASSET_DIR}/VerdaliScene.vst3/Contents/Resources/presets"
      COMMENT "Staging presets into the VST3 bundle"
      VERBATIM
   )

   set(VERDALISCENE_VST3_INSTALL_DIR "$ENV{HOME}/.vst3"
      CACHE PATH "Where 'cmake --build . --target VerdaliScene-vst3-install' puts the bundle")
   add_custom_target(VerdaliScene-vst3-install
      COMMAND "${CMAKE_COMMAND}" -E make_directory "${VERDALISCENE_VST3_INSTALL_DIR}"
      COMMAND "${CMAKE_COMMAND}" -E rm -rf "${VERDALISCENE_VST3_INSTALL_DIR}/VerdaliScene.vst3"
      COMMAND "${CMAKE_COMMAND}" -E copy_directory
         "${VERDALISCENE_VST3_ASSET_DIR}/VerdaliScene.vst3"
         "${VERDALISCENE_VST3_INSTALL_DIR}/VerdaliScene.vst3"
      DEPENDS VerdaliScene-vst3
      COMMENT "Installing VerdaliScene.vst3 into ${VERDALISCENE_VST3_INSTALL_DIR}"
      VERBATIM
   )
endif()

# ---------------------------------------------------------------------- install
install(TARGETS VerdaliScene LIBRARY DESTINATION "VerdaliScene")
install(FILES ${VERDALISCENE_PRESET_FILES} DESTINATION "VerdaliScene/presets")

message(STATUS "VerdaliScene: 'cmake --install' will write to ${CMAKE_INSTALL_PREFIX}/VerdaliScene")
