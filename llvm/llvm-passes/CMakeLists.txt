cmake_minimum_required(VERSION 2.8)

# Set flags
set(CMAKE_C_FLAGS "-Wall -Werror")

# helper function to prepend a path to source file names
function( prepend_path DIR_PATH SOURCE_FILES )
  foreach( SOURCE_FILE ${${SOURCE_FILES}} )
    set( MODIFIED ${MODIFIED} ${DIR_PATH}/${SOURCE_FILE} )
  endforeach()
  set( ${SOURCE_FILES} ${MODIFIED} PARENT_SCOPE )
endfunction()

# add gtest header and libs
# we will not use the gtest comming with the llvm version to be independent from a certain llvm version 
# and to have tab support (not included in llvm gtest)
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/../../tools/_install/include/")
link_directories("${CMAKE_CURRENT_SOURCE_DIR}/../../tools/_install/lib/")


# LLVM 
# ----------------------------------------

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

# set directories for LLVM headers and libs
add_definitions(${LLVM_DEFINITIONS})
include_directories(${LLVM_INCLUDE_DIRS})
link_directories(${LLVM_LIBRARY_DIRS})


# CUSTOM LLVM PASSES LIBRARY
# ----------------------------------------

# set new target
add_custom_target(MehariCustomLLVMPasses)

# add include directory for custom LLVM passes
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/include")

# set external dependencies for custom LLVM passes
set(PASSES_REQUIRED_LIBS ctemplate)

# set source files for all custom LLVM passes
set(MEHARI_COMMON_SOURCES HardwareInformation.cpp)
set(MEHARI_ANALYSIS_SOURCES IRGraphPrinter.cpp InstructionDependencyAnalysis.cpp SpeedupAnalysis.cpp)
set(MEHARI_CODEGEN_SOURCES SimpleCCodeGenerator.cpp TemplateWriter.cpp)
set(MEHARI_TRANSFORMS_SOURCES 
  Partitioning.cpp 
  PartitioningGraph.cpp 
  PartitioningAlgorithms.cpp 
  AddAlwaysInlineAttributePass.cpp
  )

# put path and source file names together
prepend_path("lib" MEHARI_COMMON_SOURCES)
prepend_path("lib/Analysis" MEHARI_ANALYSIS_SOURCES)
prepend_path("lib/CodeGen" MEHARI_CODEGEN_SOURCES)
prepend_path("lib/Transforms" MEHARI_TRANSFORMS_SOURCES)

# create a shared library
add_llvm_loadable_module(SharedMehariLLVMPasses 
  ${MEHARI_COMMON_SOURCES} 
  ${MEHARI_ANALYSIS_SOURCES} 
  ${MEHARI_CODEGEN_SOURCES} 
  ${MEHARI_TRANSFORMS_SOURCES}
  )
target_link_libraries(SharedMehariLLVMPasses ${PASSES_REQUIRED_LIBS})
set_target_properties(SharedMehariLLVMPasses PROPERTIES OUTPUT_NAME "MehariLLVMPasses")

# create a static library
add_llvm_library(LibraryMehariLLVMPasses 
  ${MEHARI_COMMON_SOURCES}
  ${MEHARI_ANALYSIS_SOURCES} 
  ${MEHARI_CODEGEN_SOURCES} 
  ${MEHARI_TRANSFORMS_SOURCES})
target_link_libraries(LibraryMehariLLVMPasses ${PASSES_REQUIRED_LIBS})
set_target_properties(LibraryMehariLLVMPasses PROPERTIES OUTPUT_NAME "MehariLLVMPasses")

# install both libraries
install(TARGETS SharedMehariLLVMPasses LibraryMehariLLVMPasses
  RUNTIME DESTINATION bin
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
)


# CUSTOM LLVM PASSES UNITTESTS
# ----------------------------------------

# set new target
add_custom_target(MehariUnitTests)

# helper function for adding unit tests
function(add_mehari_llvm_unittest test_name)
  # call llvm add_unittest function
  add_unittest(MehariUnitTests ${test_name} ${ARGN})
endfunction()

# add llvm passes lib directory to link_directories to use them in the unit tests
link_directories(${CMAKE_BINARY_DIR})

# enable building llvm tests
set(LLVM_BUILD_TESTS ON)

# set source files for all custom LLVM passes unittests
set(MEHARI_ANALYSIS_TEST_SOURCES InstructionDependencyAnalysisTest.cpp)
set(MEHARI_CODEGEN_TEST_SOURCES SimpleCCodeGeneratorTest.cpp)

# put path and source file names together
prepend_path("unittests/Analysis" MEHARI_ANALYSIS_TEST_SOURCES)
prepend_path("unittests/CodeGen" MEHARI_CODEGEN_TEST_SOURCES)

# set dependencies for the unit test
set(LLVM_LINK_COMPONENTS Analysis AsmParser)
set(TESTS_REQUIRED_LIBS LLVMAnalysis LLVMAsmParser)

# build unittest
add_mehari_llvm_unittest(MehariAnalysisTests
  ${MEHARI_ANALYSIS_TEST_SOURCES}
  ${MEHARI_CODEGEN_TEST_SOURCES}
  unittests/test_main.cpp
 )
target_link_libraries(MehariAnalysisTests MehariLLVMPasses ${TESTS_REQUIRED_LIBS})

# add dependency between targets
add_dependencies(MehariAnalysisTests LibraryMehariLLVMPasses)

# install unittest
install(TARGETS MehariAnalysisTests
  RUNTIME DESTINATION unittests
)
