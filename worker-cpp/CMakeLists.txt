cmake_minimum_required(VERSION 3.15)
project(BitFlowWorker CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# -----------------------------------------------------------------
# 1. Locate Required Dependencies
# -----------------------------------------------------------------
find_package(Threads REQUIRED)
find_package(Protobuf REQUIRED)

# gRPC via pkg-config (Compatible with Ubuntu APT installations)
find_package(PkgConfig REQUIRED)
pkg_check_modules(gRPC REQUIRED IMPORTED_TARGET grpc++)
if(NOT TARGET gRPC::grpc++)
    add_library(gRPC::grpc++ ALIAS PkgConfig::gRPC)
endif()

# Locate nlohmann_json
find_package(nlohmann_json QUIET)
if(NOT nlohmann_json_FOUND)
    find_package(nlohmann_json 3.2.0 REQUIRED)
endif()

# Find FFmpeg Core Multimedia Components
find_program(FFMPEG_EXECUTABLE ffmpeg)
if(NOT FFMPEG_EXECUTABLE)
    message(FATAL_ERROR "FFmpeg runtime binary executable not found on system paths!")
endif()

# Locate SimpleAmqpClient
find_path(SIMPLEAMQPCLIENT_INCLUDE_DIR NAMES SimpleAmqpClient/SimpleAmqpClient.h HINTS /usr/local/include /usr/include)
find_library(SIMPLEAMQPCLIENT_LIBRARY NAMES SimpleAmqpClient simpleamqpclient HINTS /usr/local/lib /usr/lib /usr/local/lib/x86_64-linux-gnu /usr/lib/x86_64-linux-gnu)

if(NOT SIMPLEAMQPCLIENT_INCLUDE_DIR OR NOT SIMPLEAMQPCLIENT_LIBRARY)
    message(FATAL_ERROR "SimpleAmqpClient header or library not found!")
endif()

# Locate grpc_cpp_plugin binary
find_program(GRPC_CPP_PLUGIN_EXECUTABLE NAMES grpc_cpp_plugin protoc-gen-grpc HINTS /usr/bin /usr/local/bin)
if(NOT GRPC_CPP_PLUGIN_EXECUTABLE AND TARGET gRPC::grpc_cpp_plugin)
    set(GRPC_CPP_PLUGIN_EXECUTABLE $<TARGET_FILE:gRPC::grpc_cpp_plugin>)
endif()

# -----------------------------------------------------------------
# 2. Generate gRPC & Protobuf Source Bridges
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
# 3. Define Executable Target
# -----------------------------------------------------------------
add_executable(video_worker 
    main.cpp 
    amqp_consumer.cpp 
    transcoder.cpp 
    grpc_reporter.cpp 
    ${PROTO_SRCS} 
    ${GRPC_SRCS}
)

target_include_directories(video_worker PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${GEN_OUT_DIR}
    ${SIMPLEAMQPCLIENT_INCLUDE_DIR}
)

target_link_libraries(video_worker PRIVATE
    gRPC::grpc++
    protobuf::libprotobuf
    ${SIMPLEAMQPCLIENT_LIBRARY}
    nlohmann_json::nlohmann_json
    Threads::Threads
)