# Define the submodule
cmake_minimum_required(VERSION 3.5 FATAL_ERROR)
project(remoteproj VERSION 0.1 LANGUAGES C CXX)



# Find extra boost coponents
find_package(Boost 1.66 REQUIRED COMPONENTS
             system context)


# required by GCC
find_package(Threads REQUIRED)

# Define the library for linking with others

add_library(remotelib
    src/SecuritySocket.cpp 
    src/Message.cpp
    src/Security.cpp
    src/Sender.cpp
    src/ServerChannel.cpp
    src/Receiver.cpp
    src/SecurityModule.cpp)

add_library(singstorage::remotelib ALIAS remotelib)

# Define include directories

# Need to change this?
#include_directories("${CMAKE_SOURCE_DIR}")


target_include_directories(remotelib INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
    PRIVATE src
    include
    "${CMAKE_SOURCE_DIR}"
    )



target_compile_features(remotelib
    PUBLIC 
      cxx_nullptr
      cxx_rvalue_references
      cxx_strong_enums
    PRIVATE
      cxx_decltype
      cxx_trailing_return_types
      cxx_decltype_auto
   )



# Define a dependency on the librados and folly libraries
target_link_libraries(remotelib
    PUBLIC
      singstorage::jsonlib
      FacebookFolly::FacebookFolly
      Boost::boost
      glog::glog
      ${Boost_SYSTEM_LIBRARY}
      ${Boost_CONTEXT_LIBRARY}
      ${CMAKE_THREAD_LIBS_INIT}
      ${CMAKE_DL_LIBS}
      ${FOLLY_REQS}
    )
