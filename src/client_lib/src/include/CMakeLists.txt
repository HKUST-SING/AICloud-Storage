# Define the submodule
cmake_minimum_required(VERSION 3.5 FATAL_ERROR)
project(includeproj VERSION 0.1 LANGUAGES C CXX)



# Define the library for linking with others

add_library(jsonlib 
    JSONHandler.cpp)


add_library(singstorage::jsonlib ALIAS jsonlib)

# Define include directories
target_include_directories(jsonlib INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
    PRIVATE src
    "${CMAKE_CURRENT_SOURCE_DIR}"
  )



target_compile_features(jsonlib
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
target_link_libraries(jsonlib
    PUBLIC
      Boost::boost
    )
    
