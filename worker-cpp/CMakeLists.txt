cmake_minimum_required(VERSION 3.15)
project(BitFlowWorker CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Enable standard build warnings
if(MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# ==========================================
# 1. Locate Required Dependencies
# ==========================================
find_package(Threads REQUIRED)

# JSON library
find_package(nlohmann_json QUIET)
if(NOT nlohmann_json_FOUND)
    find_package(nlohmann_json 3.2.0 REQUIRED)
endif()

# HTTP client for API progress reporting and result uploads
find_package(CURL REQUIRED)

# SimpleAmqpClient library search
find_path(SIMPLEAMQPCLIENT_INCLUDE_DIR NAMES SimpleAmqpClient/SimpleAmqpClient.h HINTS /usr/local/include /usr/include)
find_library(SIMPLEAMQPCLIENT_LIBRARY NAMES SimpleAmqpClient simpleamqpclient HINTS /usr/local/lib /usr/lib /usr/local/lib/x86_64-linux-gnu /usr/lib/x86_64-linux-gnu)

if(NOT SIMPLEAMQPCLIENT_INCLUDE_DIR OR NOT SIMPLEAMQPCLIENT_LIBRARY)
    message(FATAL_ERROR "SimpleAmqpClient header or library not found!")
endif()

# ==========================================
# 2. Define Executable Target
# ==========================================
add_executable(video_worker 
    main.cpp 
    amqp_consumer.cpp 
    transcoder.cpp 
)

target_include_directories(video_worker PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${SIMPLEAMQPCLIENT_INCLUDE_DIR}
    ${CURL_INCLUDE_DIRS}
)

target_link_libraries(video_worker PRIVATE
    ${SIMPLEAMQPCLIENT_LIBRARY}
    ${CURL_LIBRARIES}
    nlohmann_json::nlohmann_json
    Threads::Threads
)