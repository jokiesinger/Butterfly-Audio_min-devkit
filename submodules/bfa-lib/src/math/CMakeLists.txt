cmake_minimum_required(VERSION 3.13)


project(math-lib VERSION 1.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

if(PROJECT_IS_TOP_LEVEL)
	option(UNIT_TESTING "Add and build unit tests" OFF)
	set_property(GLOBAL PROPERTY USE_FOLDERS ON)
endif()

set(target math)
add_library(${target} INTERFACE
	src/constants.h
	src/matrix.h
	src/solvers.h
	src/fft.h
	src/interpolation.h
	src/signal_generators.h
)
target_include_directories(${target} INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/src")
#set_target_properties(${target} PROPERTIES LINKER_LANGUAGE CXX)


if(UNIT_TESTING)
	if(PROJECT_IS_TOP_LEVEL)
		find_package(Catch2 3 REQUIRED)
	endif()
	set(test_target "${target}_unit_tests")
	add_executable(${test_target} 
		tests/fft_tests.cpp
		tests/matrix_tests.cpp
		tests/solvers_tests.cpp
		tests/interpolation_tests.cpp
		tests/signal_generators_tests.cpp
	)
	target_link_libraries(${test_target} PRIVATE Catch2::Catch2WithMain)
	target_link_libraries(${test_target} PRIVATE math)
	set_target_properties(${test_target} PROPERTIES FOLDER "Unit Tests")
endif()
