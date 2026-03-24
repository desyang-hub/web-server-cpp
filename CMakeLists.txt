cmake_minimum_required(VERSION 3.10)
project(web-server-cpp)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

# Source files for the library
set(WEB_SOURCES
    src/web/http_server.cc
    src/web/connect_context.cpp
)

# Create library
add_library(web_server_lib STATIC ${WEB_SOURCES})

# Link libraries for the library
target_link_libraries(web_server_lib
    pthread
)

# Create main executable
add_executable(web-server-cpp src/main.cpp)
target_link_libraries(web-server-cpp web_server_lib)

# Create examples
add_subdirectory(examples)

# # Create tests
# enable_testing()
# add_subdirectory(tests)
