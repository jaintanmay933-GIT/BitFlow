# --- Stage 1: Build Environment ---
FROM ubuntu:22.04 AS builder

ENV DEBIAN_FRONTEND=noninteractive

# 1. Install standard system dependencies
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
    nlohmann-json3-dev \
    librabbitmq-dev \
    libboost-system-dev \
    libboost-chrono-dev \
    curl \
    git \
    && rm -rf /var/lib/apt/lists/*

# 2. Build SimpleAmqpClient from source
RUN git clone --depth 1 https://github.com/alanxz/SimpleAmqpClient.git /tmp/SimpleAmqpClient && \
    cd /tmp/SimpleAmqpClient && \
    mkdir build && cd build && \
    cmake .. -DCMAKE_INSTALL_PREFIX=/usr/local -DBUILD_SHARED_LIBS=ON -DBUILD_TESTS=OFF && \
    make -j$(nproc) && \
    make install && \
    rm -rf /tmp/SimpleAmqpClient

WORKDIR /app
COPY . .

# 3. Build video_worker binary
RUN rm -rf build && mkdir -p build && cd build && cmake .. && make -j$(nproc)

# --- Stage 2: Runtime Environment ---
FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive

# Install runtime libraries
RUN apt-get update && apt-get install -y \
    ffmpeg \
    ca-certificates \
    libstdc++6 \
    libgrpc++1 \
    libprotobuf23 \
    libcurl4 \
    librabbitmq4 \
    libboost-system1.74.0 \
    libboost-chrono1.74.0 \
    python3 \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

# Copy compiled shared libraries and binary from builder stage
COPY --from=builder /usr/local/lib /usr/local/lib
COPY --from=builder /app/build/video_worker /app/video_worker

# Link the dynamic library
RUN ldconfig

EXPOSE 8080

CMD python3 -m http.server 8080 & /app/video_worker