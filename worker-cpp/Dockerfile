# --- Stage 1: Build Environment ---
FROM ubuntu:22.04 AS builder

ENV DEBIAN_FRONTEND=noninteractive

# Install C++ toolchain, CMake, FFmpeg, gRPC, Protobuf & Curl development libraries
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    g++ \
    pkg-config \
    libavcodec-dev \
    libavformat-dev \
    libavutil-dev \
    libswscale-dev \
    libgrpc++-dev \
    libgrpc-dev \
    protobuf-compiler \
    protobuf-compiler-grpc \
    libprotobuf-dev \
    libcurl4-openssl-dev \
    curl \
    git \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY . .

# Build your video_worker executable
RUN mkdir -p build && cd build && cmake .. && make -j$(nproc)

# --- Stage 2: Runtime Environment ---
FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive

# Install runtime FFmpeg, gRPC runtime libs, and Python3 (for health checks)
RUN apt-get update && apt-get install -y \
    ffmpeg \
    ca-certificates \
    libstdc++6 \
    libgrpc++1 \
    libprotobuf23 \
    libcurl4 \
    python3 \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

# Copy the exact compiled binary 'video_worker' from Stage 1
COPY --from=builder /app/build/video_worker /app/video_worker

# Expose port 8080 for Koyeb health checks
EXPOSE 8080

# Run a dummy HTTP server on port 8080 while executing your video worker binary
CMD python3 -m http.server 8080 & /app/video_worker