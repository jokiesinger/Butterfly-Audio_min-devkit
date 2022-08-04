
# Butterfly Audio Library

This library contains the core of the Butterfly Audio DSP processing code base. 




## Setup

The project uses CMake and can be built without any dependencies or integration into other projects. 
However, it can also be integrated as a module into a CMake-based project. 
This will prevent this libraries `CMakeLists.txt` file to search for Catch2 (see below), to enable `USE_FOLDERS` and to remove the option for unit testing which will then need to be given by the top-level project. 


## Coding style

This repository used clang-format to format the source code in a uniform manner. 

Naming conventions are:
- Class and namespace names: PascalCase
- Function and variable names: camelCase
- Use apprehensive and common names

In general, follow the [C++ Core Guidelines](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines). 
As this is an audio library which is used under conditions with hard realtime constraints, the code needs to be as optimized as possibly. 
However, it is not advised to delve into premature optimizations and micro-optimizations in general need to be well justified. 

Additionally, code should be written in a reusable, general way. For example, algorithms which work on containers should be agnostic to the specific type of the container. 
To achieve this, an iterator-based approach is usually sensible.  


## Unit testing
This library features unit tests with [Catch2](https://github.com/catchorg/Catch2). 
The unit tests can be built by setting `UNIT_TESTING=ON` when running CMake or running CMake with the flag `-DUNIT_TESTING=ON`. 
Ensure that you have Catch2 installed and that it can be found in CMake by `find_package(Catch2 3 REQUIRED)`. 
If folders in the IDE are enabled, the tests are put into a folder named `Unit Tests`. 

When writing own unit tests,
- write one `.cpp` test file per library header file
- whose name is based on the header file and ends on `_tests`,
- pay attention to test every function and feature and
- put the test files into a directory named `tests` next to the libraries `src` directory.

In general, each source file should have a test file (except where similar files can be tested more easily in one unit).