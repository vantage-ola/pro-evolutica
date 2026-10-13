cmake_minimum_required(VERSION 3.22)
project(pro_evolutica LANGUAGES CXX)

find_package(SDL3 REQUIRED CONFIG)
add_library(main SHARED app/main.cpp rendering/triangle.cpp)
target_compile_features(main PRIVATE cxx_std_17)
target_compile_options(main PRIVATE -Wall -Wextra -Wpedantic -Werror)
target_link_libraries(main PRIVATE SDL3::SDL3 GLESv3)
