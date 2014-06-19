cmake_minimum_required(VERSION 2.8)

# Set flags
set(CMAKE_C_FLAGS "-Wall -Werror")
set(CMAKE_CXX_FLAGS "-Wall -Werror")

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

include_directories("${CMAKE_CURRENT_SOURCE_DIR}/../llvm-passes/include")
link_directories("${CMAKE_BINARY_DIR}/../llvm-passes")

#TODO
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/../../tools/pivpav/api/include")
link_directories("${CMAKE_CURRENT_SOURCE_DIR}/../../tools/pivpav/api/wrapper/build")

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

# The LLVM build scripts automatically add the required libraries, but unfortunately
# they are too early in the dependency list, so we have to add them a second time.
set(LLVM_LIBS LLVMAnalysis LLVMAsmParser LLVMSupport LLVMTarget LLVMCore LLVMMC LLVMObject)


# CUSTOM LLVM PASSES LIBRARY
# ----------------------------------------

# add our include directory
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/include")

# set our external dependencies
set(VHDL_REQUIRED_LIBS pivpav meharipasses ctemplate ${LLVM_LIBS})

# set source files for all custom LLVM passes
set(MEHARI_VHDL_SOURCES "ReadySignals.cpp" "Channel.cpp" "ValueStorage.cpp" "GenerateVHDL.cpp"
  "MyOperator.cpp" "ReconOSOperator.cpp")

# put path and source file names together
prepend_path("lib/vhdl" MEHARI_VHDL_SOURCES)

# create a shared library
add_llvm_loadable_module(libMehariVHDLPasses 
  ${MEHARI_VHDL_SOURCES})
target_link_libraries(libMehariVHDLPasses ${VHDL_REQUIRED_LIBS})

# install both libraries
install(TARGETS libMehariVHDLPasses
  RUNTIME DESTINATION bin
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
)


# CUSTOM LLVM PASSES UNITTESTS
# ----------------------------------------

# set new target
add_custom_target(MehariVHDLUnitTests)

# helper function for adding unit tests
function(add_mehari_vhdl_unittest test_name)
  # call llvm add_unittest function
  add_unittest(MehariVHDLUnitTests ${test_name} ${ARGN})
endfunction()

# add llvm passes lib directory to link_directories to use them in the unit tests
link_directories(${CMAKE_BINARY_DIR})

# enable building llvm tests
set(LLVM_BUILD_TESTS ON)

# set dependencies for the unit test
set(LLVM_LINK_COMPONENTS Analysis AsmParser)
set(TESTS_REQUIRED_LIBS ${LLVM_LIBS})

# build unittest
add_mehari_vhdl_unittest(MehariVHDLTests
  unittests/CodeGen/SimpleVHDLGeneratorTest.cpp
  unittests/test_main.cpp
 )
target_link_libraries(MehariVHDLTests mehariunittesthelpers libMehariVHDLPasses ${VHDL_REQUIRED_LIBS} ${TESTS_REQUIRED_LIBS})

# create symlink to test data
add_custom_command(TARGET MehariVHDLTests POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E create_symlink
      "${CMAKE_CURRENT_SOURCE_DIR}/unittests/CodeGen/data"
      "${CMAKE_BINARY_DIR}/CodeGen_data"
)

# create symlink to clang
add_custom_command(TARGET MehariVHDLTests POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E create_symlink
      "${CMAKE_BINARY_DIR}/../llvm/bin/clang"
      "${CMAKE_BINARY_DIR}/clang"
)

# install unittest
install(TARGETS MehariVHDLTests
  RUNTIME DESTINATION unittests
)

# run tests
add_custom_target(test
  ./MehariVHDLTests
  COMMAND RECONOS=${CMAKE_CURRENT_SOURCE_DIR}/../../reconos/reconos "${CMAKE_CURRENT_SOURCE_DIR}/test.sh" run-by-cmake
  COMMAND RECONOS=${CMAKE_CURRENT_SOURCE_DIR}/../../reconos/reconos "${CMAKE_CURRENT_SOURCE_DIR}/test2.sh" run-by-cmake
  DEPENDS MehariVHDLTests)
