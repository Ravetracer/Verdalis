# The code every Verdalis plugin is built on.
#
# Consumed by a plugin with
#
#    add_subdirectory("${VERDALIS_SHARED_DIR}" verdalis-shared)
#    target_link_libraries(<target> PRIVATE verdalis::shared)
#
# which also brings in the include directory, so shared headers are reached as
# <verdalis/params.h>, <verdalis/dsp/filters.h> and so on.
#
# A static library rather than an interface one: this is real code, and building
# it once per plugin is cheaper than compiling it into every target that needs
# it. Being static also means its file-scope state belongs to one plugin binary,
# which is what makes the shared preset provider safe.
#
# Before including this, a plugin sets:
#
#   CLAP_INCLUDE_DIR            required, the CLAP headers
#   VERDALIS_WITH_GUI           ON to build the plugin window
#   VERDALIS_GUI_INCLUDE_DIRS   X11/Cairo, or a cross-built Cairo
#   VERDALIS_GUI_LIBRARY_DIRS
#   VERDALIS_GUI_LIBRARIES
#   VERDALIS_GUI_DEFINES
#
# The GUI dependencies are discovered by the plugin, not here: a cross build
# must not consult this machine's pkg-config.

cmake_minimum_required(VERSION 3.16)

set(VERDALIS_SOURCES
   src/params.cpp
   src/preset.cpp
   src/preset_provider.cpp
)

if(VERDALIS_WITH_GUI)
   list(APPEND VERDALIS_SOURCES src/gui/window.cpp)
endif()

add_library(verdalis_shared STATIC ${VERDALIS_SOURCES})
add_library(verdalis::shared ALIAS verdalis_shared)

target_include_directories(verdalis_shared PUBLIC
   "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

# The preset-discovery provider is written against the CLAP API. The plugin that
# pulls this in has already located the headers.
if(NOT CLAP_INCLUDE_DIR)
   message(FATAL_ERROR "verdalis_shared: CLAP_INCLUDE_DIR must be set before add_subdirectory()")
endif()
target_include_directories(verdalis_shared PUBLIC "${CLAP_INCLUDE_DIR}")

if(VERDALIS_WITH_GUI)
   target_compile_definitions(verdalis_shared PUBLIC VERDALIS_WITH_GUI ${VERDALIS_GUI_DEFINES})
   target_include_directories(verdalis_shared PUBLIC ${VERDALIS_GUI_INCLUDE_DIRS})
   target_link_directories(verdalis_shared PUBLIC ${VERDALIS_GUI_LIBRARY_DIRS})
   target_link_libraries(verdalis_shared PUBLIC ${VERDALIS_GUI_LIBRARIES})
   find_package(Threads REQUIRED)
   target_link_libraries(verdalis_shared PUBLIC Threads::Threads)
endif()

target_compile_features(verdalis_shared PUBLIC cxx_std_17)

# Linked into a plugin MODULE, so it has to be position independent, and it
# must not export anything of its own: the plugins hand out one symbol.
set_target_properties(verdalis_shared PROPERTIES
   POSITION_INDEPENDENT_CODE ON
   CXX_VISIBILITY_PRESET hidden
   VISIBILITY_INLINES_HIDDEN ON
)

target_compile_options(verdalis_shared PRIVATE -Wall -Wextra)
