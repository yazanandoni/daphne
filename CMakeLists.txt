# Copyright 2021 The DAPHNE Consortium
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

cmake_minimum_required(VERSION 3.25.2)

# Build release version by default (override with -DCMAKE_BUILD_TYPE=Debug in your initial cmake invocation)
# This needs to be set *before* the project() command
set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose the type of build.")

find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
    message(STATUS "Using ccache")
    set(CMAKE_CXX_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
    set(CMAKE_CUDA_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
endif()

project(daphne-prototype LANGUAGES CXX C)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_BUILD_WITH_INSTALL_NAME_DIR ON)
set(CMAKE_CXX_STANDARD 20 CACHE STRING "C++ standard to conform to")
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_OPTIMIZE_DEPENDENCIES 1)

set(CMAKE_CXX_FLAGS_DEBUG="${CMAKE_CXX_FLAGS_DEBUG} -g -fno-omit-frame-pointer")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3")
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} -g -O3 -fno-omit-frame-pointer")

# silence a warning about DEPFILE path transformations (used in LLVM)
cmake_policy(SET CMP0116 OLD)

# *****************************************************************************
# Related to MLIR/LLVM
# *****************************************************************************

find_package(MLIR REQUIRED CONFIG)

message(STATUS "Using MLIRConfig.cmake in: ${MLIR_DIR}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

set(LLVM_RUNTIME_OUTPUT_INTDIR ${CMAKE_BINARY_DIR}/bin)
set(LLVM_LIBRARY_OUTPUT_INTDIR ${CMAKE_BINARY_DIR}/lib)
set(MLIR_BINARY_DIR ${CMAKE_BINARY_DIR})

list(APPEND CMAKE_MODULE_PATH "${MLIR_CMAKE_DIR}")
list(APPEND CMAKE_MODULE_PATH "${LLVM_CMAKE_DIR}")
include(TableGen)
include(AddLLVM)
include(AddMLIR)

# the next command needs to be guarded because it messes with compiler flags
if (CMAKE_BUILD_TYPE STREQUAL "Release")
    set(LLVM_ENABLE_ASSERTIONS OFF)
endif()

include(HandleLLVMOptions)

include_directories(${LLVM_INCLUDE_DIRS})
include_directories(${MLIR_INCLUDE_DIRS})

link_directories(${LLVM_BUILD_LIBRARY_DIR})
add_definitions(${LLVM_DEFINITIONS})

# Enable exception handling and run-time type information.
set(LLVM_ENABLE_EH ON)
set(LLVM_ENABLE_RTTI ON)

# *****************************************************************************
# Related to external libraries (OpenBLAS, CUDA, etc)
# *****************************************************************************

SET(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

set(BLA_VENDOR OpenBLAS)
set(BLA_STATIC OFF)
find_package(BLAS REQUIRED)

# specify multiple paths in CMAKE_PREFIX_PATH separated by semicolon to add multiple include dirs
# to make compile/exec in container plus finding includes in local IDE work, specify both, local third party sources
# prefix and /usr/local e.g.: -DCMAKE_PREFIX_PATH="/usr/local;${PROJECT_SOURCE_DIR}/thirdparty/installed"
foreach(path ${CMAKE_PREFIX_PATH})
    message(STATUS "CMAKE_PREFIX_PATH:  ${path}")
    include_directories(${path}/include)
endforeach(path)

# check <package>_ROOT env var
cmake_policy(SET CMP0074 NEW)

###### MPI
if(USE_MPI)
    find_package(MPI REQUIRED)
    add_definitions(-DUSE_MPI)
    include_directories(${MPI_INCLUDE_PATH})
endif()    
##########

###### PAPI
if(USE_PAPI)
    add_definitions(-DUSE_PAPI)
endif()
##########

find_package(fmt)
add_definitions(-DSPDLOG_FMT_EXTERNAL)

option(USE_CUDA "Whether to activate compilation of CUDA features" OFF)
include(CheckLanguage)
check_language(CUDA)
if(USE_CUDA AND CMAKE_CUDA_COMPILER)
    enable_language(CUDA)
    find_package(CUDAToolkit REQUIRED)
    set(CMAKE_CUDA_STANDARD 20)
    set(CMAKE_CUDA_STANDARD_REQUIRED ON)

    if(${CMAKE_COMPILER_IS_GNUCXX})
        set(GCC_EXPECTED_VERSION 11.3.0)
        set(CUDA_EXPECTED_VERSION "11.7.99")
        if(CMAKE_CXX_COMPILER_VERSION VERSION_GREATER GCC_EXPECTED_VERSION AND NOT DEFINED ENV{CUDAHOSTCXX})
            if(CMAKE_CUDA_COMPILER_VERSION VERSION_LESS ${CUDA_EXPECTED_VERSION})
                message(WARNING "CUDA ${CMAKE_CUDA_COMPILER_VERSION} requires version ${GCC_EXPECTED_VERSION} to build \
                        but found ${CMAKE_CXX_COMPILER_VERSION} You should set the env var CUDAHOSTCXX to something \
                        like g++-11 (if installed)")
                execute_process(COMMAND sleep 7)
            endif()
        endif()
    endif()

    set(CMAKE_CUDA_ARCHITECTURES  OFF)
    cmake_policy(SET CMP0104 NEW)
    add_definitions(-DUSE_CUDA)
    message(STATUS "Note: disabled CUSPARSE_DEPRECATED in main CMakeLists.txt")
    add_definitions(-DDISABLE_CUSPARSE_DEPRECATED)
    set(CMAKE_CUDA_STANDARD 20)
    set(CMAKE_CUDA_STANDARD_REQUIRED ON)
    message(STATUS  "CUDA enabled (version ${CMAKE_CUDA_COMPILER_VERSION})")
    if(DEFINED ENV{CUDAHOSTCXX})
        message(STATUS "CUDAHOSTCXX value: $ENV{CUDAHOSTCXX}")
    endif()
endif()

find_program(LLD_PROGRAM lld)
if(LLD_PROGRAM)
    message(STATUS "Using lld")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fuse-ld=lld")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fuse-ld=lld")
endif()

find_package(Arrow REQUIRED)
find_package(Parquet REQUIRED)

# support for Eigen library
find_package (Eigen3 REQUIRED)

option(USE_FPGAOPENCL "Whether to activate compilation of FPGA OpenCL features" OFF)
if(USE_FPGAOPENCL)
	if(NOT DEFINED ENV{QUARTUSDIR})
		message(SEND_ERROR "Intel(R) Quartus installation directory should be defined by QUARTUSDIR varaiable (e.g. /opt/intel/intelFPGA_pro/21.4/)")
		execute_process(COMMAND sleep 10)
	endif()
	include_directories($ENV{QUARTUSDIR}/hld/examples_aoc/common/inc/ $ENV{QUARTUSDIR}/hld/host/include/)
	message(STATUS "cmake: using FPGA")
	add_definitions(-DUSE_FPGAOPENCL)
endif()


# HDFS library
# look through provided CMAKE_PREFIX_PATHs
option(USE_HDFS "Whether to activate compilation of HDSF support" OFF)
if(USE_HDFS)
    foreach(path ${CMAKE_PREFIX_PATH})
        if(NOT DEFINED HDFS_LIB_FOUND)
            set(HDFS_LIB_LOCATION ${path}/hdfs)
            if(EXISTS ${HDFS_LIB_LOCATION})
                set(HDFS_LIB_FOUND TRUE)
            else()
                unset(HDFS_LIB_LOCATION)
                unset(HDFS_LIB_FOUND)
            endif()
        endif()
    endforeach(path)

    # fallback if not using CMAKE_PREFIX_PATH (e.g., system/container install)
    if(NOT DEFINED CMAKE_PREFIX_PATH OR NOT DEFINED HDFS_LIB_FOUND)
        set(HDFS_LIB_LOCATION /usr/local/include/hdfs)
        if(EXISTS ${HDFS_LIB_LOCATION})
            set(HDFS_LIB_FOUND TRUE)
        else()
            unset(HDFS_LIB_LOCATION)
        endif()
    endif()

    include_directories(${PROJECT_SOURCE_DIR} ${HDFS_LIB_LOCATION})
    find_library(hdfs3 NAMES libhdfs3.so HINTS ${PROJECT_BINARY_DIR}/installed/lib REQUIRED)
    message(STATUS "HDFS_LIB_FOUND: ${HDFS_LIB_FOUND}")
    if(DEFINED HDFS_LIB_FOUND)
        add_definitions(-DUSE_HDFS)
    endif()
endif()


set(CMAKE_VERBOSE_MAKEFILE ON)


# *****************************************************************************
# DuckDB Configuration (pre-built)
# *****************************************************************************

include_directories(${PROJECT_SOURCE_DIR}/thirdparty/installed/duckdb/include)

find_library(DUCKDB_LIBRARY
        NAMES duckdb
        HINTS ${PROJECT_SOURCE_DIR}/thirdparty/installed/duckdb/lib
        REQUIRED
)
# *****************************************************************************
# ODBC Configuration
# *****************************************************************************


find_package(ODBC REQUIRED)


find_library(ODBC_LIBRARY
        NAMES odbc
        HINTS /usr/lib/x86_64-linux-gnu /usr/local/lib
        REQUIRED
)

find_library(ODBCINST_LIBRARY
        NAMES odbcinst
        HINTS /usr/lib/x86_64-linux-gnu /usr/local/lib
        REQUIRED
)

#find_library(DUCKDB_ODBC_LIBRARY
#        NAMES duckdb_odbc
#        HINTS ${PROJECT_SOURCE_DIR}/thirdparty/sources/duckdb-odbc/build/release
#        REQUIRED
#)

# *****************************************************************************
# sqlite Configuration (pre-built)
# *****************************************************************************

# Ensure system SQLite is not used
set(SQLITE3_FOUND OFF)

# Set the path to SQLite headers and source files
set(SQLITE3_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/thirdparty/installed/sqlite-amalgamation-3490000)

# Add SQLite as a static library
add_library(sqlite3 STATIC
        ${SQLITE3_INCLUDE_DIR}/sqlite3.c
)

# Enable additional SQLite features
target_compile_definitions(sqlite3 PUBLIC
        SQLITE_ENABLE_MATH_FUNCTIONS  # Enables SQRT, LOG, EXP, etc.
        SQLITE_ENABLE_JSON1           # Enables JSON functions (JSON_OBJECT, JSON_ARRAY, etc.)
        SQLITE_ENABLE_FTS5            # Enables Full-Text Search v5 (for text indexing)
        SQLITE_ENABLE_RTREE           # Enables R-Tree spatial indexes
        SQLITE_ENABLE_GEOPOLY         # Enables geospatial functions
        SQLITE_ENABLE_UPDATE_DELETE_LIMIT  # Enables LIMIT in UPDATE and DELETE statements
        SQLITE_ENABLE_COLUMN_METADATA  # Allows querying metadata of columns
        SQLITE_ENABLE_STAT4            # Improves SQLite query planner statistics
)

# Make SQLite headers available to all components
target_include_directories(sqlite3 PUBLIC ${SQLITE3_INCLUDE_DIR})

# *****************************************************************************
# Project-specific include directories
# *****************************************************************************

include_directories(${PROJECT_SOURCE_DIR}/src)
include_directories(${PROJECT_BINARY_DIR}/src)

# *****************************************************************************
# Descend to subdirectories
# *****************************************************************************

add_subdirectory(src/ir/daphneir)
add_subdirectory(src/api/cli)
add_subdirectory(src/api/daphnelib)
add_subdirectory(src/api/internal)
add_subdirectory(src/compiler/execution)
add_subdirectory(src/compiler/explanation)
add_subdirectory(src/compiler/inference)
add_subdirectory(src/compiler/lowering)
add_subdirectory(src/compiler/utils)
add_subdirectory(src/parser)
add_subdirectory(src/parser/catalog)
add_subdirectory(src/parser/config)
add_subdirectory(src/parser/metadata)
add_subdirectory(src/runtime/distributed/proto)
add_subdirectory(src/runtime/distributed/worker)
add_subdirectory(src/runtime/local/datastructures)
add_subdirectory(src/runtime/local/io)
add_subdirectory(src/runtime/local/kernels)
if(USE_FPGAOPENCL)
    add_subdirectory(src/runtime/local/kernels/FPGAOPENCL)
endif() 
add_subdirectory(src/util)

add_dependencies(CompilerUtils MLIRDaphneTransformsIncGen)

add_subdirectory(daphne-opt)
add_subdirectory(test)
