# Define the submodule
cmake_minimum_required(VERSION 3.5 FATAL_ERROR)
project(clusterproj VERSION 0.1 LANGUAGES C CXX)



# Need to link to Threads
find_package(Threads REQUIRED)

# Need to link to OpenSSL
find_package(OpenSSL REQUIRED)


# Define the library for linking with others

add_library(clusterlib 
    src/CephContext.cpp
    src/StoreObj.cpp
    src/Worker.cpp
    src/StoreWorker.cpp
    src/RemoteProtocol.cpp
    src/JSONRemoteProtocol.cpp)


add_library(singstorage::clusterlib ALIAS clusterlib)

# Define include directories
target_include_directories(clusterlib INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
    PRIVATE src
    "${CMAKE_SOURCE_DIR}"
    "${CMAKE_SOURCE_DIR}/remote/include"
    include
  )



target_compile_features(clusterlib
    PUBLIC 
      cxx_nullptr
      cxx_rvalue_references
      cxx_decltype_auto
      cxx_trailing_return_types
      cxx_strong_enums
    PRIVATE
      cxx_decltype
   )



# Define a dependency on the librados and folly libraries
target_link_libraries(clusterlib
    PUBLIC
      singstorage::jsonlib
      singstorage::remotelib
      FacebookFolly::FacebookFolly
      CephRados::CephRados
      glog::glog
      OpenSSL::Crypto
      ${CMAKE_THREAD_LIBS_INIT}
    )
    
