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
find_package(gRPC REQUIRED)

# Find FFmpeg Core Multimedia Components
find_program(FFMPEG_EXECUTABLE ffmpeg)
if(NOT FFMPEG_EXECUTABLE)
    message(FATAL_ERROR "FFmpeg runtime binary executable not found on system paths!")
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
    ARGS --cpp_out=${GEN_OUT_DIR} --grpc_out=${GEN_OUT_DIR}
         --plugin=protoc-gen-grpc=/usr/bin/grpc_cpp_plugin
         -I ${PROTO_SRC_DIR} ${PROTO_SRC_DIR}/video_service.proto
    DEPENDS "${PROTO_SRC_DIR}/video_service.proto"
    COMMENT "Running C++ Protocol Buffer Compiler (protoc)..."
)

# -----------------------------------------------------------------
# 3. Define the Executable Target
# -----------------------------------------------------------------
# For now, we will link an empty main file that we will write tomorrow
# Add your new source file to the compilation pool here:
# Make sure ALL four .cpp files are included right here:
add_executable(video_worker 
    main.cpp 
    amqp_consumer.cpp 
    transcoder.cpp 
    grpc_reporter.cpp 
    ${PROTO_SRCS} 
    ${GRPC_SRCS}
)
# Target include directories (where to look for headers)
target_include_directories(video_worker PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${GEN_OUT_DIR}
)

# Link essential runtime libraries straight into our engine binary
target_link_libraries(video_worker PRIVATE
    gRPC::grpc++
    protobuf::libprotobuf
    Threads::Threads
)