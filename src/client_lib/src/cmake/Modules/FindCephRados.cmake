#
#  Try to find the Ceph librados library.
# 
#  This will define the following variables:: 
#
#    CEPHRADOS_FOUND       - True if the system has folly
#    CEPHRADOS_VERSION     - Ceph librados version which was found
#
#    
#  and the following imported targets::
#
#    CephRados::CephRados - The Ceph librados target


# try to get hints for the paths
find_package(PkgConfig)
pkg_check_modules(PC_rados QUIET rados)

find_path(RADOS_INCLUDE_DIR
    NAMES rados/librados.hpp 
    PATHS ${PC_rados_INCLUDE_DIRS}
    PATH_SUFFIXES include
)


find_library(RADOS_LIBRARY
    NAMES rados
    PATHS ${PC_rados_LIBRARY_DIRS}
)


set(RADOS_VERSION ${PC_rados_VERSION})


include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(rados
    FOUND_VAR RADOS_FOUND
    REQUIRED_VARS
        RADOS_LIBRARY
        RADOS_INCLUDE_DIR
    VERSION_VAR RADOS_VERSION
)


if(RADOS_FOUND)
    if(NOT TARGET CephRados::CephRados)
        add_library(CephRados::CephRados UNKNOWN IMPORTED)
    endif()

    set_target_properties(CephRados::CephRados PROPERTIES
                          INTERFACE_COMPILE_OPTIONS 
                          "${PC_rados_CFLAGS_OTHER}"
                          INTERFACE_INCLUDE_DIRECTORIES 
                          "${RADOS_INCLUDE_DIR}"
                          IMPORTED_LOCATION
                          "${RADOS_LIBRARY}")

endif()


# pass the values to the user
set(CEPHRADOS_FOUND         ${RADOS_FOUND})
set(CEPHRADOS_VERSION      "${RADOS_VERSION}")


mark_as_advanced(
    CEPHRADOS_FOUND
    CEPHRADOS_VERSION
)
                          

