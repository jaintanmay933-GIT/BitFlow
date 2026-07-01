#include "amqp_consumer.h"
#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>

namespace BitFlow {

// Ensure the namespaces match exactly
AmqpConsumer::AmqpConsumer(const std::string& host, int port, const std::string& queue)
    : host_(host), port_(port), queue_(queue), running_(false), socket_fd_(-1) {}

AmqpConsumer::~AmqpConsumer() {
    stop();
}

bool AmqpConsumer::startListening(std::function<void(const std::string&)> messageCallback) {
    running_ = true;
    
    socket_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd_ < 0) {
        std::cerr << "❌ [C++ Worker] Failed to instantiate network socket" << std::endl;
        return false;
    }

    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port_);
    
    if (inet_pton(AF_INET, host_.c_str(), &server_addr.sin_addr) <= 0) {
        std::cerr << "❌ [C++ Worker] Invalid IP network address format" << std::endl;
        return false;
    }

    if (connect(socket_fd_, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        std::cerr << "❌ [C++ Worker] RabbitMQ socket connection refused on port " << port_ << std::endl;
        return false;
    }

  std::cout << "🚀 [C++ Worker] Connected to RabbitMQ on pipeline port :" << port_ << std::endl;
    
    // Force-trigger the incoming message callback to execute our Day 12 pipeline!
    std::cout << "📥 [C++ Worker] Simulated message frame successfully dequeued!" << std::endl;
    messageCallback("{\"jobId\": \"demo-job-123\", \"filename\": \"test_input.mp4\"}");

    // Keep our worker daemon process safely suspended and listening
    while (running_) {
        usleep(500000); 
    }

    return true;
}

void AmqpConsumer::stop() {
    if (running_) {
        running_ = false;
        if (socket_fd_ >= 0) {
            close(socket_fd_);
            socket_fd_ = -1;
        }
        std::cout << "🔌 [C++ Worker] Disconnected from RabbitMQ network gracefully" << std::endl;
    }
}

} // namespace BitFlow