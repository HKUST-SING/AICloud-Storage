# Define the submodule
cmake_minimum_required(VERSION 3.5 FATAL_ERROR)
project(remoteproj VERSION 0.1 LANGUAGES C CXX)



# Define the library for linking with others

add_library(remotelib 
    src/Message.cpp
    src/Sender.cpp
    src/ServerChannel.cpp
    src/Receiver.cpp)


# Define include directories
target_include_directories(remotelib PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR/include}>
    $<INSTALL_INTERFACE:include>
    PRIVATE src
    include
    ${PROJECT_SOURCE_DIR})



target_compile_features(remotelib
    PUBLIC 
      cxx_nullptr
      cxx_rvalue_references
      cxx_strong_enums
    PRIVATE
      cxx_decltype
   )



# Define a dependency on the librados and folly libraries
target_link_libraries(remotelib
    FacebookFolly::FacebookFolly
    boost::boost
    )
