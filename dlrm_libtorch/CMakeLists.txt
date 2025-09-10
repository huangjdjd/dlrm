set(CMAKE_THREAD_LIBS_INIT "-lpthread")
set(CMAKE_HAVE_THREADS_LIBRARY 1)
set(CMAKE_USE_WIN32_THREADS_INIT 0)
set(CMAKE_USE_PTHREADS_INIT 1)
set(THREADS_PREFER_PTHREAD_FLAG ON)
set(CMAKE_BUILD_TYPE Debug)

#set(CMAKE_CXX_STANDARD 11)
set(CMAKE_PREFIX_PATH ${CMAKE_PREFIX_PATH} "/usr/local/lib")


cmake_minimum_required(VERSION 3.10 FATAL_ERROR)
project(dlrm)

SET(DATA ./build/data)
SET(DATA2 ./build/data2)
LINK_DIRECTORIES(
${DATA}
${DATA2}
)
find_package(OpenMP)
if (OPENMP_FOUND)
    set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${OpenMP_C_FLAGS}")
    set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")
    set (CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${OpenMP_EXE_LINKER_FLAGS}")
endif()

find_package(Torch REQUIRED)
include_directories(include)
add_executable(dlrmtest src/dlrmtest.cpp src/emb_cache.cpp src/data_transfer.cpp src/emb_init.cpp include/data.h)
#add_executable(test_wr src/emb_cache.cpp src/data_transfer.cpp src/emb_init.cpp include/data.h)
add_executable(dlrmtest2 src/dlrmtest2.cpp src/emb_cache2.cpp src/data_transfer2.cpp src/emb_init2.cpp include/data2.h)
find_library(/usr/local/lib NAMES liblightnvm.a)
add_library(lightnvm STATIC IMPORTED)
set_target_properties(lightnvm PROPERTIES IMPORTED_LOCATION /usr/local/lib/liblightnvm.a )

target_link_libraries(dlrmtest "${TORCH_LIBRARIES}")
target_link_libraries(dlrmtest lightnvm)
target_link_libraries(dlrmtest2 "${TORCH_LIBRARIES}")
target_link_libraries(dlrmtest2 lightnvm)
#target_link_libraries(test_wr lightnvm)
set_property(TARGET dlrmtest PROPERTY CXX_STANDARD 14)

set_property(TARGET dlrmtest2 PROPERTY CXX_STANDARD 14)



# cmake_minimum_required(VERSION 3.18)
# project(dlrm CXX)

# # --- 搜尋路徑（不要把 /usr/local/lib 塞進 CMAKE_PREFIX_PATH）---
# # 由命令列 -DCMAKE_PREFIX_PATH=/home/user/libtorch 傳入就好

# set(CMAKE_CXX_STANDARD 14)
# set(CMAKE_CXX_STANDARD_REQUIRED ON)

# # 建議：只有在沒有用命令列指定時才給預設
# if(NOT CMAKE_BUILD_TYPE)
#   set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
# endif()

# # 針對 Release/RelWithDebInfo 開較激進最佳化
# if(CMAKE_BUILD_TYPE MATCHES "Release|RelWithDebInfo")
#   add_compile_options(-march=native)
#   # 可選：視情況再開
#   # add_compile_options(-ffast-math)
# endif()

# # LTO（CMake 3.9+）
# include(CheckIPOSupported)
# check_ipo_supported(RESULT lto_ok OUTPUT lto_msg)
# if(lto_ok)
#   set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)
# endif()

# # Threads & OpenMP
# find_package(Threads REQUIRED)
# find_package(OpenMP)
# if(OpenMP_CXX_FOUND)
#   message(STATUS "OpenMP found: ${OpenMP_CXX_VERSION}")
# endif()

# # Torch
# find_package(Torch REQUIRED)

# # include / link 目錄
# include_directories(include)
# link_directories(${CMAKE_BINARY_DIR}/data ${CMAKE_BINARY_DIR}/data2)

# # imported 靜態庫：liblightnvm.a
# add_library(lightnvm STATIC IMPORTED)
# set_target_properties(lightnvm PROPERTIES
#   IMPORTED_LOCATION /usr/local/lib/liblightnvm.a
#   INTERFACE_INCLUDE_DIRECTORIES /usr/local/include
# )

# # 可選：若需要全域性 OpenMP
# # target_link_libraries(lightnvm INTERFACE OpenMP::OpenMP_CXX)

# # targets
# add_executable(dlrmtest
#   src/dlrmtest.cpp src/emb_cache.cpp src/data_transfer.cpp src/emb_init.cpp
#   include/data.h
# )
# add_executable(dlrmtest2
#   src/dlrmtest2.cpp src/emb_cache2.cpp src/data_transfer2.cpp src/emb_init2.cpp
#   include/data2.h
# )

# # 針對 target 正確連結（順序：Torch -> OpenMP/pthread -> lightnvm）
# foreach(tgt IN ITEMS dlrmtest dlrmtest2)
#   target_link_libraries(${tgt}
#     PRIVATE
#       ${TORCH_LIBRARIES}
#       Threads::Threads
#       lightnvm
#   )
#   if(OpenMP_CXX_FOUND)
#     target_link_libraries(${tgt} PRIVATE OpenMP::OpenMP_CXX)
#   else()
#     # 若 find_package(OpenMP) 失敗，但 liblightnvm.a 需要 libgomp：
#     target_compile_options(${tgt} PRIVATE -fopenmp)
#     target_link_libraries(${tgt} PRIVATE -fopenmp)
#   endif()
# endforeach()
