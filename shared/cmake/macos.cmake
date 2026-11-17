# macOS support for a Verdalis plugin's CMakeLists.txt.
#
# Included by every plugin right after VERDALIS_SHARED_DIR is known:
#
#    include("${VERDALIS_SHARED_DIR}/cmake/macos.cmake")
#
# On any other platform it defines the helpers below and does nothing else, so
# Linux and Windows builds are untouched by it.
#
# macOS is built natively on GitHub's macOS runners
# (.github/workflows/macos.yml), never on the development machine: the macOS
# SDK may only be used on Apple hardware. Everything here is what that build
# needs on top of the Linux one -- the Cocoa window, bundles in place of plain
# shared objects, and the Audio Unit -- and is kept in one place so that the
# eleven plugin CMakeLists.txt files only call it.
#
# The deployment target cannot be set here: it has to be set before project(),
# which each plugin does itself (std::filesystem needs 10.15).

if(APPLE)
   # The window's Cocoa view and the file panels (shared/src/gui/*.mm).
   # enable_language() must run at the top level of the project, which an
   # include() from the plugin's own CMakeLists.txt is.
   enable_language(OBJCXX)
endif()

# A static, universal Cairo with its Quartz backend (fonts) and pixman, from
# shared/cmake/build-macos-cairo.sh. Nothing from Homebrew: its libraries are
# dylibs for one architecture and for that machine's macOS version.
get_filename_component(_verdalis_root "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
set(VERDALIS_MAC_CAIRO "${_verdalis_root}/macbuild/cairo-macos"
   CACHE PATH "Static Cairo for macOS (shared/cmake/build-macos-cairo.sh)")

# verdalis_macos_gui(<PREFIX>)
#
# Sets <PREFIX>_GUI_DEPS_INCLUDE_DIRS, <PREFIX>_GUI_DEPS_LIBRARIES,
# <PREFIX>_GUI_SOURCES and <PREFIX>_GUI_FOUND_MAC in the caller's scope, as the
# Windows and X11 branches of a plugin's CMakeLists.txt do for theirs.
#
# A missing Cairo is a hard failure, unlike on Windows: no release path builds
# a Mac plugin without a window, and one that silently lost it would still ship.
macro(verdalis_macos_gui prefix)
   if(NOT EXISTS "${VERDALIS_MAC_CAIRO}/lib/libcairo.a")
      message(FATAL_ERROR
         "${PROJECT_NAME}: no macOS Cairo at VERDALIS_MAC_CAIRO=${VERDALIS_MAC_CAIRO}; "
         "run shared/cmake/build-macos-cairo.sh, or configure with -D${prefix}_BUILD_GUI=OFF")
   endif()
   set(${prefix}_GUI_DEPS_INCLUDE_DIRS
      "${VERDALIS_MAC_CAIRO}/include" "${VERDALIS_MAC_CAIRO}/include/cairo")
   set(${prefix}_GUI_DEPS_LIBRARIES
      "${VERDALIS_MAC_CAIRO}/lib/libcairo.a" "${VERDALIS_MAC_CAIRO}/lib/libpixman-1.a"
      "-framework Cocoa" "-framework CoreGraphics" "-framework CoreText"
      "-framework CoreFoundation" "-framework ApplicationServices")
   set(${prefix}_GUI_SOURCES src/gui/gui.cpp)
   set(${prefix}_GUI_FOUND_MAC TRUE)
   message(STATUS "${PROJECT_NAME}: building the plugin window (Cocoa + Cairo)")
endmacro()

# verdalis_stage_presets(<target> <preset files...>)
#
# Copies the factory presets into a macOS bundle's Contents/Resources/presets,
# which is where factoryPresetDir() looks from Contents/MacOS. The window
# reads the library embedded in the binary; this copy is for the CLAP preset
# discovery and for the user to read, exactly as in the Linux VST3 bundle.
function(verdalis_stage_presets target)
   add_custom_command(TARGET ${target} POST_BUILD
      COMMAND "${CMAKE_COMMAND}" -E make_directory
         "$<TARGET_BUNDLE_CONTENT_DIR:${target}>/Resources/presets"
      COMMAND "${CMAKE_COMMAND}" -E copy_if_different ${ARGN}
         "$<TARGET_BUNDLE_CONTENT_DIR:${target}>/Resources/presets"
      COMMENT "Staging presets into the ${target} bundle"
      VERBATIM
   )
endfunction()

# verdalis_macos_clap_bundle(<target> <Name> <plugin id> <preset files...>)
#
# Turns the CLAP module into the bundle hosts expect on macOS,
# <Name>.clap/Contents/MacOS/<Name>, exporting clap_entry and nothing else.
# Called after the plugin's own set_target_properties(), whose ".clap" suffix
# it takes back: inside a bundle the binary is named like the bundle.
function(verdalis_macos_clap_bundle target name id)
   set_target_properties(${target} PROPERTIES
      BUNDLE TRUE
      BUNDLE_EXTENSION clap
      PREFIX ""
      SUFFIX ""
      OUTPUT_NAME "${name}"
      LIBRARY_OUTPUT_NAME "${name}"
      MACOSX_BUNDLE_GUI_IDENTIFIER "${id}"
      MACOSX_BUNDLE_BUNDLE_NAME "${name}"
      MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
      MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
   )
   # Undefined symbols are an error by default with Apple's linker, so there is
   # no --no-undefined to add, and no version script: ld64 takes the list here.
   target_link_options(${target} PRIVATE "-Wl,-exported_symbol,_clap_entry")
   verdalis_stage_presets(${target} ${ARGN})
endfunction()

# verdalis_add_auv2(<Name> <plugin id> <subtype code> <preset files...>)
#
# The same plugin as an Audio Unit instrument ("aumu"), built through the
# clap-wrapper exactly as the VST3 is: <Name>-auv2 links <Name>-impl and its
# own copy of src/entry.cpp. The wrapper finds clap_entry in its own bundle.
#
# The manufacturer and subtype codes are the plugin's for good: Logic and
# every other AU host keep projects by them, exactly as CLAP hosts keep them by
# the plugin id. "Rvtr" is shared with the plugins in Ravetracer/audio-plugins,
# so a subtype must not repeat one used there (Rsid, Saek).
#
# Writes <build>/auv2-id.txt ("aumu <subtype> Rvtr"), which is what the
# workflow hands to auval.
function(verdalis_add_auv2 name id subtype)
   add_library(${name}-auv2 MODULE src/entry.cpp)
   target_link_libraries(${name}-auv2 PRIVATE ${name}-impl)
   target_compile_options(${name}-auv2 PRIVATE -Wall -Wextra)
   target_add_auv2_wrapper(
      TARGET ${name}-auv2
      OUTPUT_NAME "${name}"
      BUNDLE_IDENTIFIER "${id}.auv2"
      BUNDLE_VERSION "${PROJECT_VERSION}"
      MANUFACTURER_NAME "Ravetracer"
      MANUFACTURER_CODE "Rvtr"
      SUBTYPE_CODE "${subtype}"
      INSTRUMENT_TYPE "aumu"
   )
   verdalis_stage_presets(${name}-auv2 ${ARGN})
   file(WRITE "${CMAKE_BINARY_DIR}/auv2-id.txt" "aumu ${subtype} Rvtr\n")
endfunction()
