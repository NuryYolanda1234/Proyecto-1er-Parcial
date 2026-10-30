cmake_minimum_required(VERSION 3.22)
project(CorredorSFML LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(SFML 3 REQUIRED COMPONENTS Graphics Window System Audio)

add_executable(corredor_sfml main.cpp)
target_link_libraries(corredor_sfml PRIVATE SFML::Graphics SFML::Audio)
