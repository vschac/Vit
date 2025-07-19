cmake_minimum_required(VERSION 3.13)

project(vit)

set(CMAKE_CXX_STANDARD 23) # Enable the C++23 standard

file(GLOB_RECURSE SOURCE_FILES src/*.cpp src/*.hpp)

find_package(OpenSSL REQUIRED)
find_package(ZLIB REQUIRED)

add_executable(vit ${SOURCE_FILES})

target_include_directories(vit PRIVATE ${OPENSSL_INCLUDE_DIR})
target_link_libraries(vit PRIVATE ${OPENSSL_LIBRARIES})
target_link_libraries(vit PRIVATE ZLIB::ZLIB)
