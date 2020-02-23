cmake_minimum_required(VERSION 3.12)

# Installation will try use a standard file directory structure.
# The included module provides functions for that.
include(GNUInstallDirs)
# Installation will create packages.
# The included module makes it easier to create them.
include(CMakePackageConfigHelpers)

project(pcaudiolib)

# Original Autoconf provided some options.
# They are replicated here under different names.
# They are not available in Windows though.
if("${CMAKE_SYSTEM_NAME}" STREQUAL "Windows")
  set(${PROJECT_NAME}_WITH_ALSA FALSE)
  set(${PROJECT_NAME}_WITH_PULSEAUDIO FALSE)
  set(${PROJECT_NAME}_WITH_OSS FALSE)
  set(${PROJECT_NAME}_WITH_QSA FALSE)
  set(${PROJECT_NAME}_WITH_COREAUDIO FALSE)

  # Window specific option.
  # XAudio2 is only available in MSVC.
  # There is a replacement called FAudio.
  set(${PROJECT_NAME}_WITH_FAUDIO_DEFAULT TRUE)
  if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
    set(${PROJECT_NAME}_WITH_FAUDIO_DEFAULT FALSE)
  endif()
  set(
    ${PROJECT_NAME}_WITH_FAUDIO
    "${${PROJECT_NAME}_WITH_FAUDIO_DEFAULT}"
    CACHE BOOL "Replace XAudio2 dependency with FAudio."
  )
else()
  # There are some detection rules in the originlal Autoconf,
  # but I have no way of testing them.
  # They are left here to remind myself they are there
  # in case they are needed.
  set(
    ${PROJECT_NAME}_WITH_ALSA
    FALSE
    CACHE BOOL "Enable support for ALSA audio output. (Not used)."
  )
  set(
    ${PROJECT_NAME}_WITH_PULSEAUDIO
    FALSE
    CACHE BOOL "Enable support for PulseAudio output. (Not used)."
  )
  set(
    ${PROJECT_NAME}_WITH_OSS
    FALSE
    CACHE BOOL "Enable support for OSS audio output. (Not used)."
  )
  set(
    ${PROJECT_NAME}_WITH_QSA
    FALSE
    CACHE BOOL "Enable support for QSA audio output. (Not used)."
  )
  set(
    ${PROJECT_NAME}_WITH_COREAUDIO
    FALSE
    CACHE BOOL "Enable support for coreaudio output. (Not used)."
  )
  set(${PROJECT_NAME}_WITH_FAUDIO FALSE)
endif()

#
# ### CMake compatibility.
#
# Autoconf creates a `config.h`, and source code expects to find it.
# The header is not necessary with CMake,
# though not all functionalities are ironed out yet.
set(FAKE_CONFIG_H_DIR "${CMAKE_CURRENT_BINARY_DIR}/fake_include")
set(FAKE_CONFIG_H "${FAKE_CONFIG_H_DIR}/config.h")
add_custom_command(
  OUTPUT "${FAKE_CONFIG_H}"
  COMMAND
    "${CMAKE_COMMAND}" ARGS "-E" "make_directory" "${FAKE_CONFIG_H_DIR}"
  COMMAND "${CMAKE_COMMAND}" ARGS "-E" "touch" "${FAKE_CONFIG_H}"
)
add_custom_target(create_fake_config_h DEPENDS "${FAKE_CONFIG_H}")

#
#
#
# ## The pcaudiolib

set(TARGET_NAME ${PROJECT_NAME})
if("${CMAKE_SYSTEM_NAME}" STREQUAL "Windows")
  add_library(
    ${TARGET_NAME} SHARED
    "${CMAKE_CURRENT_SOURCE_DIR}/src/audio.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/windows.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/xaudio2.cpp"
  )
else()
  # These are the files iin Automake.
  # Not tested.
  add_library(
    ${TARGET_NAME} SHARED
    "${CMAKE_CURRENT_SOURCE_DIR}/src/alsa.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/qsa.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/oss.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/pulseaudio.c"
  )
endif()

# The output library name is usually `libpcaudio.so`.
set_target_properties(${TARGET_NAME} PROPERTIES OUTPUT_NAME "pcaudio")

# Tell CMake that the library built should be installed when asked to.
install(TARGETS ${TARGET_NAME} EXPORT ${TARGET_NAME})

# Let other CMake users find this library after it is installed.
install(
  EXPORT ${TARGET_NAME}
  DESTINATION "${CMAKE_INSTALL_DATADIR}/${PROJECT_NAME}/cmake"
  NAMESPACE "${PROJECT_NAME}::"
  FILE "${TARGET_NAME}-target.cmake"
)

#
# ### Headers

# For `config.h`.
add_dependencies(${TARGET_NAME} create_fake_config_h)
target_include_directories(${TARGET_NAME} PRIVATE "${FAKE_CONFIG_H_DIR}")

# Tell compiler where to find headers.
target_include_directories(
  ${TARGET_NAME}
  PUBLIC
    # This tells compiler where to find the library headers
    # when compiling the library.
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/include>
    # When the library is installed,
    # a CMake module is also installed
    # to tell other CMake users where to find this library.
    # This tells other users of the CMake module how to find the headers.
    #
    # This needs to be specified because the in-source hierarchy
    # is incompatible with the hierarchy for installation
    #
    # The actual installation of those headers are below.
    $<INSTALL_INTERFACE:include>
)

# Install headers of this library along with the library.
install(
  DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/src/include/pcaudiolib"
  TYPE "INCLUDE"
)

# ### Link dependencies

if("${CMAKE_SYSTEM_NAME}" STREQUAL "Windows")
  if("${${PROJECT_NAME}_WITH_FAUDIO}")
    find_package(FAudio)
    target_link_libraries(${TARGET_NAME} FAudio::FAudio)
  else()
    target_link_libraries(${TARGET_NAME} "XAudio2")
  endif()
endif()

#
# ### Extra features based on user configuration

# Most of these options in Autoconf seem to use pkg_check to fetch flags.
# I am not sure how to replicate them.
# These are just placeholder code for parts that I did understand.
if("${${PROJECT_NAME}_WITH_ALSA}")
  target_compile_definitions(
    ${TARGET_NAME} PRIVATE "HAVE_ALSA_ASOUNDLIB_H"
  )
endif()
if("${${PROJECT_NAME}_WITH_OSS}")

endif()
if("${${PROJECT_NAME}_WITH_PULSEAUDIO}")
  target_compile_definitions(
    ${TARGET_NAME} PRIVATE "HAVE_PULSE_SIMPLE_H"
  )
endif()
if("${${PROJECT_NAME}_WITH_QSA}")
  target_link_libraries(${TARGET_NAME} asound)
endif()
if("${${PROJECT_NAME}_WITH_COREAUDIO}")
  target_sources(
    ${TARGET_NAME}
    PRIVATE
      "src/coreaudio.c"
      "src/TPCircularBuffer/TPCircularBuffer.c"
      "src/TPCircularBuffer/TPCircularBuffer+AudioBufferList.c"
  )
  target_compile_options(
    ${TARGET_NAME} PRIVATE "-dynamiclib" "-fvisibility=default"
  )
  target_compile_definitions(${TARGET_NAME} PRIVATE "HAVE_COREAUDIO")

  # Need to `find_library` first.
  # But I do not know the coventions for these.
  target_link_libraries(${TARGET_NAME} CoreAudio AudioToolbox AudioUnit)
endif()

# A Windows-only option.
if("${${PROJECT_NAME}_WITH_FAUDIO}")
  target_compile_definitions(${TARGET_NAME} PRIVATE "HAVE_FAUDIO_H")
endif()

#
# ### Enable warnings and silence known warnings

if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
  target_compile_definitions(
    ${TARGET_NAME}
    PRIVATE
      # Disable warnings.
      "_CRT_SECURE_NO_WARNINGS"
      "_CRT_NONSTDC_NO_DEPRECATE"
  )
endif()

#
# ### Create a CMake package that imports all targets in this project.

# Generate the template needed to for creating the package file.
# This is done at configuration time.
set(TEMPLATE_DIR "${CMAKE_CURRENT_BINARY_DIR}/cmake_modules")
set(CONFIG_TEMPLATE "${TEMPLATE_DIR}/package_config_template.cmake.in")
set(CONFIG_INSTALL_DIR "${CMAKE_INSTALL_DATADIR}/${PROJECT_NAME}/cmake")
set(INSTALLED_BUILDSYSTEM_TARGETS "pcaudiolib")
file(
  WRITE "${CONFIG_TEMPLATE}"
  [=[
@PACKAGE_INIT@

include(CMakeFindDependencyMacro)

foreach(TARGET_TO_INCLUDE @INSTALLED_BUILDSYSTEM_TARGETS@)
  include("${CMAKE_CURRENT_LIST_DIR}/${TARGET_TO_INCLUDE}-target.cmake")
endforeach()

if(@pcaudiolib_WITH_FAUDIO@)
  find_dependency(FAudio)
endif()

check_required_components(@INSTALLED_BUILDSYSTEM_TARGETS@)
]=]
)

# Tell CMake to generate the package.
set(CONFIG_FILENAME "${PROJECT_NAME}-config.cmake")
configure_package_config_file(
  # Tell CMake to use the template generated above.
  "${CONFIG_TEMPLATE}"
  # The file to contain the generated package file at build time.
  "${TEMPLATE_DIR}/${CONFIG_FILENAME}"
  # Directory to copy the package to at install time.
  # Note that it does not actually install the file created.
  INSTALL_DESTINATION
  "${CONFIG_INSTALL_DIR}"
)
# So install the file manually.
install(
  FILES "${TEMPLATE_DIR}/${CONFIG_FILENAME}"
  DESTINATION "${CONFIG_INSTALL_DIR}"
)
