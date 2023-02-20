cmake_minimum_required(VERSION 3.22)
project(CCOrange_WebServer)

set(CMAKE_CXX_STANDARD 11)

file(COPY staticResources DESTINATION ${CMAKE_BINARY_DIR})
add_subdirectory(code)