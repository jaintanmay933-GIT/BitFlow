# --- Stage 1: Build Environment ---
FROM ubuntu:22.04 AS builder

ENV DEBIAN_FRONTEND=noninteractive

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
    libsimpleamqpclient-dev \
    librabbitmq-dev \
    curl \
    git \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY . .

# Force clean build directory and build video_worker
RUN rm -rf build && mkdir -p build && cd build && cmake .. && make -j$(nproc)

# --- Stage 2: Runtime Environment ---
FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y \
    ffmpeg \
    ca-certificates \
    libstdc++6 \
    libgrpc++1 \
    libprotobuf23 \
    libcurl4 \
    libsimpleamqpclient2 \
    librabbitmq4 \
    python3 \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

COPY --from=builder /app/build/video_worker /app/video_worker

EXPOSE 8080

CMD python3 -m http.server 8080 & /app/video_worker