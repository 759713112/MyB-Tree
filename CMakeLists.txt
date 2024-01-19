cmake_minimum_required(VERSION 2.8.5 FATAL_ERROR)
project(Sherman)

# disable boost warning
add_definitions(-DBOOST_COROUTINES_NO_DEPRECATION_WARNING)

#Compiler options
set(CMAKE_C_FLAGS "-Wall -Wno-deprecated-declarations -Wsign-compare -O3 -g")
set(CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS} -std=c++11") 
# Link Options
set(LINKS_FLAGS "-lcityhash -lboost_system -lboost_coroutine -lpthread -libverbs -lmemcached -lmlx5")

find_package(PkgConfig REQUIRED)
pkg_check_modules(DOCA IMPORTED_TARGET doca-libs)
#Source file define
set(COMMON_SRC ${PROJECT_SOURCE_DIR}/src)
file(GLOB_RECURSE COMMON_FILE ${COMMON_SRC}/*.c*)
set(INCLUDE_BASE ${PROJECT_SOURCE_DIR}/include)
include_directories(${INCLUDE_BASE} ${DOCA_INCLUDE_DIRS})
add_compile_definitions(USE_CORO)
if (DOCA_FOUND)



    if(${CMAKE_SYSTEM_PROCESSOR} STREQUAL "aarch64")
        message("Code for aarch64 architecture.")
        add_compile_definitions(AARCH64)
    endif()

    add_library(sherman STATIC ${COMMON_FILE})
    link_libraries(sherman)

    file(GLOB TEST_SRC ${PROJECT_SOURCE_DIR}/test/*.cpp)
    foreach (TEST ${TEST_SRC})
        get_filename_component(TEST_NAME ${TEST} NAME_WE)
        add_executable(${TEST_NAME} ${TEST})
        target_link_libraries(${TEST_NAME} ${LINKS_FLAGS} ${DOCA_LINK_LIBRARIES})
    endforeach()

else()
    message("Just Compile client")
    file(GLOB_RECURSE REMOVE_SOURCE ${COMMON_SRC}/*.c ${COMMON_SRC}/DmaDpu.cpp 
                ${COMMON_SRC}/DpuPro*.cpp  ${COMMON_SRC}/DSMemory*.cpp)
    list(REMOVE_ITEM COMMON_FILE ${REMOVE_SOURCE})

    add_library(sherman STATIC ${COMMON_FILE})
    link_libraries(sherman)

    file(GLOB TEST_SRC ${PROJECT_SOURCE_DIR}/test/benchmark.cpp)
    foreach (TEST ${TEST_SRC})
        get_filename_component(TEST_NAME ${TEST} NAME_WE)
        add_executable(${TEST_NAME} ${TEST})
        target_link_libraries(${TEST_NAME} ${LINKS_FLAGS})
    endforeach()
endif()
