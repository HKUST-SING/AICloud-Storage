# Use a new version of cmake
cmake_minimum_required(VERSION 3.5 FATAL_ERROR)

project(clientstorage VERSION 0.1 LANGUAGES C CXX)

# add find modules
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake/Modules/")


# Found it is useful to use GNUInstallDirs to install
# libraries on all platforms

include(GNUInstallDirs)

# Include Facebok folly as an imported target
find_package(FacebookFolly REQUIRED)


#Ceph librados has to be included as well
find_package(CephRados REQUIRED)

# Google logging library
find_package(Glog REQUIRED)
add_library(glog::glog UNKNOWN IMPORTED)
set_target_properties(glog::glog PROPERTIES
             INTERFACE_INCLUDE_DIRECTORIES 
             ${GLOG_INCLUDE_DIRS}
             IMPORTED_LOCATION
             ${GLOG_LIBRARIES})

# Boost library required
find_package(Boost 1.66 REQUIRED)
add_library(Boost::boost INTERFACE IMPORTED)
set_property(TARGET Boost::boost PROPERTY
             INTERFACE_INCLUDE_DIRECTORIES ${Boost_INCLUDE_DIR})

# Build our targets
add_subdirectory(include)
add_subdirectory(cluster)
add_subdirectory(remote)
add_subdirectory(ipc)


# Add executable
# Enable logging by default
OPTION(SING_ENABLE_LOG "Compile source code with logging." ON)
add_executable(rados_cli rados_client.cpp)

if(SING_ENABLE_LOG)
  target_compile_definitions(rados_cli PRIVATE SING_ENABLE_LOG=1)
endif(SING_ENABLE_LOG)

# Link to all dependencies
target_link_libraries(rados_cli PRIVATE 
                          singstorage::clusterlib 
                          singstorage::remotelib
                          singstorage::ipclib)
