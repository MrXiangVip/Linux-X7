# Find FIT
# -------
#
# Finds the FIT library
#
# This will define the following variables:
#
#   FIT_FOUND             -- True if the system has the FIT library
#   FIT_INCLUDE_DIRS      -- The include directories for FIT
#   FIT_LIBRARIES         -- Libraries to link against
#   FIT_LIBRARIES_ARCHIVE -- Libraries type is archive
#   FIT_CXX_FLAGS         -- Additional (required) compiler flags
#
# and the following imported targets:
#
#   fit

include(FindPackageHandleStandardArgs)

if (DEFINED ENV{FIT_INSTALL_PREFIX})
  set(FIT_INSTALL_PREFIX $ENV{FIT_INSTALL_PREFIX})
else()
  # Assume we are in <install-prefix>/share/cmake/FITConfig.cmake
  get_filename_component(CMAKE_CURRENT_LIST_DIR "${CMAKE_CURRENT_LIST_FILE}" PATH)
  get_filename_component(FIT_INSTALL_PREFIX "${CMAKE_CURRENT_LIST_DIR}/../../" ABSOLUTE)
endif()

# Include directories.
if (EXISTS "${FIT_INSTALL_PREFIX}/lib/include")
  set(FIT_INCLUDE_DIRS
    ${FIT_INSTALL_PREFIX}/lib/include)
else()
  set(FIT_INCLUDE_DIRS
    ${FIT_INSTALL_PREFIX}/include)
endif()

# Library dependencies.
if (1)
    if(1)
      find_library(FIT_LIBRARY fit NO_DEFAULT_PATH PATHS "${FIT_INSTALL_PREFIX}/lib")
    else()
      if(MSVC)
        find_library(FIT_LIBRARY fit NO_DEFAULT_PATH PATHS "${FIT_INSTALL_PREFIX}/lib")
      endif()
      set(FIT_LIBRARY_TYPE "static")
      set(FIT_LIBRARIES_ARCHIVE ON)
    endif()
endif()
add_library(fit UNKNOWN IMPORTED)
set(FIT_LIBRARIES fit)

# Set required cxx flags
if ("")
  set(FIT_CXX_FLAGS "")
endif()

# Set required link flags
if ("")
  set(FIT_CXX_FLAGS "")
endif()

set_target_properties(fit PROPERTIES
    IMPORTED_LOCATION "${FIT_LIBRARY}"
    INTERFACE_INCLUDE_DIRECTORIES "${FIT_INCLUDE_DIRS}"
    CXX_STANDARD 11
)
if (FIT_CXX_FLAGS)
  set_property(TARGET fit PROPERTY INTERFACE_COMPILE_OPTIONS "${FIT_CXX_FLAGS}")
endif()

if (FIT_LINK_FLAGS)
  set_property(TARGET fit PROPERTY INTERFACE_LINK_OPTIONS "${FIT_LINK_FLAGS}")
endif()

find_package_handle_standard_args(fit DEFAULT_MSG FIT_LIBRARY FIT_INCLUDE_DIRS)
