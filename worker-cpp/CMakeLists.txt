cmake_minimum_required(VERSION 3.15)
project(BitFlowWorker CXX)

# Force modern C++17 standards
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# -----------------------------------------------------------------
# 1. Locate Required Dependencies
# -----------------------------------------------------------------
find_package(Threads REQUIRED)
find_package(Protobuf REQUIRED)
find_package(gRPC CONFIG REQUIRED)
find_package(nlohmann_json 3.2.0 REQUIRED)

# Find FFmpeg Core Multimedia Components
find_program(FFMPEG_EXECUTABLE ffmpeg)
if(NOT FFMPEG_EXECUTABLE)
    message(FATAL_ERROR "FFmpeg runtime binary executable not found on system paths!")
endif()

# Locate SimpleAmqpClient installed manually in /usr/local
find_path(SIMPLEAMQPCLIENT_INCLUDE_DIR NAMES SimpleAmqpClient/SimpleAmqpClient.h HINTS /usr/local/include /usr/include)
find_library(SIMPLEAMQPCLIENT_LIBRARY NAMES SimpleAmqpClient simpleamqpclient HINTS /usr/local/lib /usr/lib /usr/local/lib/x86_64-linux-gnu /usr/lib/x86_64-linux-gnu)

if(NOT SIMPLEAMQPCLIENT_INCLUDE_DIR OR NOT SIMPLEAMQPCLIENT_LIBRARY)
    message(FATAL_ERROR "SimpleAmqpClient header or library not found in /usr/local! Ensure step 2 installation completed.")
endif()

# Dynamically locate protoc-gen-grpc plugin binary
find_program(GRPC_CPP_PLUGIN_EXECUTABLE protoc-gen-grpc)
if(NOT GRPC_CPP_PLUGIN_EXECUTABLE)
    set(GRPC_CPP_PLUGIN_EXECUTABLE $<TARGET_FILE:gRPC::grpc_cpp_plugin>)
endif()

# -----------------------------------------------------------------
# 2. Generate gRPC & Protobuf Source Bridges from Contract
# -----------------------------------------------------------------
set(PROTO_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../pb")
set(GEN_OUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
file(MAKE_DIRECTORY ${GEN_OUT_DIR})

set(PROTO_SRCS "${GEN_OUT_DIR}/video_service.pb.cc")
set(PROTO_HDRS "${GEN_OUT_DIR}/video_service.pb.h")
set(GRPC_SRCS "${GEN_OUT_DIR}/video_service.grpc.pb.cc")
set(GRPC_HDRS "${GEN_OUT_DIR}/video_service.grpc.pb.h")

add_custom_command(
    OUTPUT "${PROTO_SRCS}" "${PROTO_HDRS}" "${GRPC_SRCS}" "${GRPC_HDRS}"
    COMMAND ${PROTOBUF_PROTOC_EXECUTABLE}
    ARGS --cpp_out=${GEN_OUT_DIR} 
         --grpc_out=${GEN_OUT_DIR}
         --plugin=protoc-gen-grpc=${GRPC_CPP_PLUGIN_EXECUTABLE}
         -I ${PROTO_SRC_DIR} ${PROTO_SRC_DIR}/video_service.proto
    DEPENDS "${PROTO_SRC_DIR}/video_service.proto"
    COMMENT "Running C++ Protocol Buffer Compiler (protoc)..."
)

# -----------------------------------------------------------------
# 3. Define the Executable Target
# -----------------------------------------------------------------
add_executable(video_worker 
    main.cpp 
    amqp_consumer.cpp 
    transcoder.cpp 
    grpc_reporter.cpp 
    ${PROTO_SRCS} 
    ${GRPC_SRCS}
)

# Target include directories
target_include_directories(video_worker PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${GEN_OUT_DIR}
    ${SIMPLEAMQPCLIENT_INCLUDE_DIR}
)

# Link essential runtime libraries
target_link_libraries(video_worker PRIVATE
    gRPC::grpc++
    protobuf::libprotobuf
    ${SIMPLEAMQPCLIENT_LIBRARY}
    nlohmann_json::nlohmann_json
    Threads::Threads
)