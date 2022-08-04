cmake_minimum_required(VERSION 3.13)


project(utilities-lib VERSION 1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

if(PROJECT_IS_TOP_LEVEL)
	option(UNIT_TESTING "Add and build unit tests" OFF)
	set_property(GLOBAL PROPERTY USE_FOLDERS ON)
endif()

set(target utilities)
add_library(${target} INTERFACE
	src/stable_id_array.h
	src/stable_id_vector.h
)
target_include_directories(${target} INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/src")


if(UNIT_TESTING)
	if(PROJECT_IS_TOP_LEVEL)
		find_package(Catch2 3 REQUIRED)
	endif()
	set(test_target "${target}_unit_tests")
	add_executable(${test_target} 
		tests/stable_id_containers_tests.cpp
	)
	target_link_libraries(${test_target} PRIVATE Catch2::Catch2WithMain)
	target_link_libraries(${test_target} PRIVATE utilities)
	set_target_properties(${test_target} PROPERTIES FOLDER "Unit Tests")
endif()