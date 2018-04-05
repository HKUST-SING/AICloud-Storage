#
#  Try to find the Facebook Folly library.
# 
#  This will define the following variables:: 
#
#    FACEBOOKFOOLY_FOUND       - True if the system has folly
#    FACEBOOKFOLLY_VERSION     - Facebook folly version which was found
#
#  and the following imported targets::
#
#    FacebookFolly::FacebookFolly  - The Facebook Folly target


# try to get hints for the paths
find_package(PkgConfig)
pkg_check_modules(PC_folly QUIET folly)

find_path(FOLLY_INCLUDE_DIR
    NAMES folly/ 
    PATHS ${PC_folly_INCLUDE_DIRS}
    PATH_SUFFIXES include
)


find_library(FOLLY_LIBRARY
    NAMES folly
    PATHS ${PC_folly_LIBRARY_DIRS}
)


set(FOLLY_VERSION ${PC_folly_VERSION})


include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(folly
    FOUND_VAR FOLLY_FOUND
    REQUIRED_VARS
        FOLLY_LIBRARY
        FOLLY_INCLUDE_DIR
    VERSION_VAR FOLLY_VERSION
)


if(FOLLY_FOUND)
    if(NOT TARGET FacebookFolly::FacebookFolly)
        add_library(FacebookFolly::FacebookFolly UNKNOWN IMPORTED)
    endif()

    set_target_properties(FacebookFolly::FacebookFolly PROPERTIES
                          INTERFACE_COMPILE_OPTIONS 
                          "${PC_folly_CFLAGS_OTHER}"
                          INTERFACE_INCLUDE_DIRECTORIES 
                          "${FOLLY_INCLUDE_DIR}"
                          IMPORTED_LOCATION 
                          "${FOLLY_LIBRARY}")
endif()


# pass the values to the user
set(FACEBOOKFOLLY_FOUND        ${FOLLY_FOUND})
set(FACEBOOKFOLLY_VERSION     "${FOLLY_VERSION}")


mark_as_advanced(
    FACEBOOKFOLLY_FOUND
    FACEBOOKFOLLY_VERSION
)
                          

