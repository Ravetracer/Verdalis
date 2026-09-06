# The code every Verdalis plugin is built on.
#
# Consumed by a plugin with
#
#    add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../shared" verdalis-shared)
#    target_link_libraries(<target> PRIVATE verdalis::shared)
#
# which also brings in the include directory, so shared headers are reached as
# <verdalis/params.h>, <verdalis/dsp/filters.h> and so on.
#
# A static library rather than an interface one: params.cpp is real code, and
# building it once per plugin is cheaper than compiling it into every target
# that needs it.

cmake_minimum_required(VERSION 3.16)

add_library(verdalis_shared STATIC
   src/params.cpp
   src/preset.cpp
   src/preset_provider.cpp
)
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

target_compile_features(verdalis_shared PUBLIC cxx_std_17)

# Linked into a plugin MODULE, so it has to be position independent, and it
# must not export anything of its own: the plugins hand out one symbol.
set_target_properties(verdalis_shared PROPERTIES
   POSITION_INDEPENDENT_CODE ON
   CXX_VISIBILITY_PRESET hidden
   VISIBILITY_INLINES_HIDDEN ON
)

target_compile_options(verdalis_shared PRIVATE -Wall -Wextra)
