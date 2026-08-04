# --- Stage 1: Build Environment ---
FROM ubuntu:22.04 AS builder

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    g++ \
    pkg-config \
    libssl-dev \
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

RUN git clone --depth 1 https://github.com/alanxz/SimpleAmqpClient.git /tmp/SimpleAmqpClient && \
    cd /tmp/SimpleAmqpClient && \
    mkdir build && cd build && \
    cmake .. -DCMAKE_INSTALL_PREFIX=/usr/local -DBUILD_SHARED_LIBS=ON -DBUILD_TESTS=OFF -DENABLE_SSL_SUPPORT=ON && \
    make -j$(nproc) && \
    make install && \
    rm -rf /tmp/SimpleAmqpClient

WORKDIR /app
COPY . .

WORKDIR /app/worker-cpp
RUN rm -rf build && mkdir -p build && cd build && cmake .. && make -j$(nproc)

# --- Stage 2: Runtime Environment with Local TLS Proxy ---
FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y \
    ffmpeg \
    ca-certificates \
    stunnel4 \
    libssl3 \
    libstdc++6 \
    libgrpc++1 \
    libprotobuf23 \
    libcurl4 \
    librabbitmq4 \
    libboost-system1.74.0 \
    libboost-chrono1.74.0 \
    python3 \
    && rm -rf /var/lib/apt/lists/*

# Create stunnel configuration with SNI and stdout logging enabled
RUN mkdir -p /etc/stunnel /var/run/stunnel && \
    echo "pid = /tmp/stunnel.pid" > /etc/stunnel/stunnel.conf && \
    echo "output = /dev/stdout" >> /etc/stunnel/stunnel.conf && \
    echo "debug = 7" >> /etc/stunnel/stunnel.conf && \
    echo "[rabbitmq]" >> /etc/stunnel/stunnel.conf && \
    echo "client = yes" >> /etc/stunnel/stunnel.conf && \
    echo "accept = 127.0.0.1:5672" >> /etc/stunnel/stunnel.conf && \
    echo "connect = campbell.lmq.cloudamqp.com:5671" >> /etc/stunnel/stunnel.conf && \
    echo "sni = campbell.lmq.cloudamqp.com" >> /etc/stunnel/stunnel.conf && \
    echo "verifyChain = no" >> /etc/stunnel/stunnel.conf

WORKDIR /app

COPY --from=builder /usr/local/lib /usr/local/lib
COPY --from=builder /app/worker-cpp/build/video_worker /app/video_worker

RUN ldconfig

EXPOSE 8080

# Launch stunnel in background, wait 2 seconds for binding, then start python HTTP server and C++ worker
CMD stunnel /etc/stunnel/stunnel.conf & sleep 2 && python3 -m http.server 8080 & /app/video_worker