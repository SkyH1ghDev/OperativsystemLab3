cmake_minimum_required(VERSION 3.12)

project(FileSystem CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O0 -g")

file(GLOB SOURCE_FILES
     main.cpp
     fs.cpp
     disk.cpp
)

file(GLOB HEADER_FILES
     fs.h
     disk.h
)

add_executable(filesystem ${SOURCE_FILES} shell.cpp)

add_executable(test1 ${SOURCE_FILES} test_script1.cpp)
add_executable(test2 ${SOURCE_FILES} test_script2.cpp)
add_executable(test3 ${SOURCE_FILES} test_script3.cpp)
add_executable(test4 ${SOURCE_FILES} test_script4.cpp)
add_executable(test5 ${SOURCE_FILES} test_script5.cpp)

add_custom_target(cleanBuild
     COMMAND rm filesystem test1 test2 test3 test4 test5 main.o shell.o fs.o disk.o test_script*.o diskfile.bin
)

