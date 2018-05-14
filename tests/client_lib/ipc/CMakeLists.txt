# Define the submodule
cmake_minimum_required(VERSION 3.5 FATAL_ERROR)
project(ipcproj VERSION 0.1 LANGUAGES C CXX)



# Find extra boost coponents
find_package(Boost 1.66 REQUIRED COMPONENTS
             system context)


# Need to link to OpenSSL
find_package(OpenSSL REQUIRED)


# Define the library for linking with others

add_library(ipclib 
    src/IPCServer.cpp
    src/lib/callback/ServerAcceptCallback.cpp
    src/lib/callback/ServerReadCallback.cpp
    src/lib/callback/ServerWriteCallback.cpp
    src/lib/utils/BFCAllocator.cpp)


add_library(singstorage::ipclib ALIAS ipclib)

# Define include directories
target_include_directories(ipclib INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
    PRIVATE src
    "${CMAKE_SOURCE_DIR}"
    include
  )



target_compile_features(ipclib
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
target_link_libraries(ipclib
    PUBLIC
      singstorage::clusterlib
      FacebookFolly::FacebookFolly
      glog::glog
      OpenSSL::Crypto
      ${Boost_SYSTEM_LIBRARY}
      ${Boost_CONTEXT_LIBRARY}
      ${CMAKE_DL_LIBS}
      ${FOLLY_REQS}
    )
    
