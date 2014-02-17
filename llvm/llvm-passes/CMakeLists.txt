cmake_minimum_required(VERSION 2.8)

# The target LLVM installation
set(LLVM_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../_install/llvm" CACHE PATH "Root of LLVM install.")

# A bit of a sanity check
if( NOT EXISTS ${LLVM_ROOT}/include/llvm )
message(FATAL_ERROR "LLVM_ROOT (${LLVM_ROOT}) is not a valid LLVM install")
endif()

# We incorporate the CMake features provided by LLVM:
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${LLVM_ROOT}/share/llvm/cmake")
include(LLVMConfig)

# Define add_llvm_* macro's.
include(AddLLVM)

add_definitions(${LLVM_DEFINITIONS})
include_directories(${LLVM_INCLUDE_DIRS})
link_directories(${LLVM_LIBRARY_DIRS})

# Add all directories for custom passes
add_subdirectory(Analysis)
# add_subdirectory(Transform)
