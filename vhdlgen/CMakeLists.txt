cmake_minimum_required(VERSION 2.8)

INCLUDE_DIRECTORIES("${CMAKE_CURRENT_SOURCE_DIR}/../tools/_install/include")
LINK_DIRECTORIES("${CMAKE_CURRENT_SOURCE_DIR}/../tools/_install/lib")

# SWIG doesn't generate files that work with -Werror, so we only add
# these for our targets
set(cxx_debug "-g")
set(cxx_strict "-Wall -Werror")

# This syntax only works with CMake 2.8.8 or newer - I have 2.8.7 :-(
#add_library(pprint_objs OBJECT ${SOURCES})
#add_library(pprint STATIC $<TARGET_OBJECTS:pprint_objs>)
#add_library(pprint SHARED $<TARGET_OBJECTS:pprint_objs>)

SET(PPRINT_SOURCES pprint_builder.cpp pprint.cpp pprint_empty.cpp
	pprint_hcat.cpp pprint_indent.cpp pprint_text.cpp pprint_vcat.cpp
	pprint_vcat_overlapping.cpp)
SET(VHDLGEN_SOURCES vhdlgen.cpp)
SET(TEST_PPRINT_SOURCES test_pprint_builder.cpp  test_pprint_empty.cpp
    test_pprint_indent.cpp  test_pprint_vcat.cpp test_pprint.cpp
    test_pprint_helpers.cpp  test_pprint_text.cpp    test_pprint_vcat_overlapping.cpp)
SET(TEST_VHDLGEN_SOURCES test_vhdlgen.cpp)

add_library(pprint  SHARED ${PPRINT_SOURCES})
add_library(vhdlgen SHARED ${VHDLGEN_SOURCES})
target_link_libraries(vhdlgen pprint)
set_property(TARGET pprint  PROPERTY COMPILE_FLAGS "${cxx_debug} ${cxx_strict}")
set_property(TARGET vhdlgen PROPERTY COMPILE_FLAGS "${cxx_debug} ${cxx_strict}")

add_executable(test test_main.cpp ${TEST_PPRINT_SOURCES} ${TEST_VHDLGEN_SOURCES})
target_link_libraries(test pprint vhdlgen gtest gmock pthread)
set_property(TARGET test PROPERTY COMPILE_FLAGS "${cxx_debug} ${cxx_strict}")


FIND_PACKAGE(SWIG REQUIRED 2.0)
INCLUDE(${SWIG_USE_FILE})

SET(CMAKE_SWIG_FLAGS "")

FIND_PACKAGE(PythonLibs)
INCLUDE_DIRECTORIES(${PYTHON_INCLUDE_PATH})

SET_SOURCE_FILES_PROPERTIES(pprint.i PROPERTIES CPLUSPLUS ON)
SWIG_ADD_MODULE(pprint python pprint.i dummy.cpp)
SWIG_LINK_LIBRARIES(pprint ${PYTHON_LIBRARIES} pprint)
set_property(TARGET _pprint PROPERTY COMPILE_FLAGS "${cxx_debug}")

SET_SOURCE_FILES_PROPERTIES(vhdlgen.i PROPERTIES CPLUSPLUS ON)
SWIG_ADD_MODULE(vhdlgen python vhdlgen.i dummy.cpp)
SWIG_LINK_LIBRARIES(vhdlgen ${PYTHON_LIBRARIES} pprint vhdlgen)
set_property(TARGET _vhdlgen PROPERTY COMPILE_FLAGS "${cxx_debug}")
