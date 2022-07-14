project(RdsGatewayRepM3)

cmake_minimum_required(VERSION 3.0)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

option(test "Build all tests." OFF) # Makes boolean 'test' available.

#set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
#set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
#set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

if(NOT CMAKE_BUILD_TOOL MATCHES "(msdev|devenv|nmake|MSBuild)")
  include(CheckCXXCompilerFlag)
  CHECK_CXX_COMPILER_FLAG("-std=c++1y" COMPILER_SUPPORTS_CXX1Y)
  if(COMPILER_SUPPORTS_CXX1Y)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC -std=c++1y -pthread")
  else()
    message(STATUS "The compiler ${CMAKE_CXX_COMPILER} has no C++14 support. Please use a different C++ compiler.")
  endif()
endif()

set_directory_properties(PROPERTIES COMPILE_DEFINITIONS $<$<CONFIG:Debug>:_DEBUG>)

FIND_PACKAGE(shape REQUIRED)
include(${CMAKE_MODULE_PATH}/ShapeComponentDeclaration.cmake)

FIND_PACKAGE(shapeware REQUIRED)
FIND_PACKAGE(oegw REQUIRED)

set(PROJECT_INSTALL_PREFIX ${PROJECT_NAME})
set(CMAKE_INSTALL_PREFIX ${shape_DEPLOY})

add_definitions(-DRAPIDJSON_HAS_STDSTRING)

include_directories(${CMAKE_SOURCE_DIR}/include)
#include_directories(${CMAKE_SOURCE_DIR}/3rdparty/rapidjson/include)

if (test)
  include(CTest)
  enable_testing()
endif()

add_subdirectory(include)
add_subdirectory(RepM3-test)
add_subdirectory(RepM3-example)
add_subdirectory(RepM3Edge)
add_subdirectory(repm3gw)

set(DEFAULT_CHSGW_VERSION v1.0.0-develop)
string(TIMESTAMP DEFAULT_BUILD_TIMESTAMP)

if(DEFINED CHSGW_VERSION)
  message(STATUS "CHSGW_VERSION = " ${CHSGW_VERSION})
else()
  message(STATUS "Set to default CHSGW_VERSION = " ${DEFAULT_CHSGW_VERSION})
  set(APP_VERSION ${DEFAULT_CHSGW_VERSION})
endif()

if(DEFINED BUILD_TIMESTAMP)
  #message(STATUS "BUILD_TIMESTAMP = " ${BUILD_TIMESTAMP})
else()
  message(STATUS "Set to default BUILD_TIMESTAMP = " ${DEFAULT_BUILD_TIMESTAMP})
  set(APP_BUILD_TIMESTAMP ${DEFAULT_BUILD_TIMESTAMP})
endif()

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/VersionInfo.h.in ${CMAKE_BINARY_DIR}/VersionInfo.h @ONLY)
