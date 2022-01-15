project(all_include)

file(GLOB_RECURSE _HDRFILES ${CMAKE_CURRENT_SOURCE_DIR}/*.h)
source_group("Header Files" FILES ${_HDRFILES})
file(GLOB_RECURSE _SRCFILES ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)
source_group("Source Files" FILES ${_SRCFILES})

add_library(${PROJECT_NAME} STATIC ${_HDRFILES} ${_SRCFILES})
