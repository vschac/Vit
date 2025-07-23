cmake_minimum_required(VERSION 3.13)
project(ai_examples)

set(CMAKE_CXX_STANDARD 23)

# Find packages
find_package(PkgConfig REQUIRED)
pkg_check_modules(CURL REQUIRED libcurl)

# Find nlohmann_json (you might need to install via vcpkg)
find_package(nlohmann_json REQUIRED)

# OpenAI example
add_executable(openai_example openai_example.cpp)
target_link_libraries(openai_example ${CURL_LIBRARIES} nlohmann_json::nlohmann_json)
target_include_directories(openai_example PRIVATE ${CURL_INCLUDE_DIRS})

# Ollama example
add_executable(ollama_example ollama_example.cpp)
target_link_libraries(ollama_example ${CURL_LIBRARIES} nlohmann_json::nlohmann_json)
target_include_directories(ollama_example PRIVATE ${CURL_INCLUDE_DIRS})