#pragma once
#include <string>
#include <functional>

namespace BitFlow {

class AmqpConsumer {
public:
    AmqpConsumer(const std::string& host, int port, const std::string& queue);
    ~AmqpConsumer();

    // Explicit functional signature matching main.cpp precisely
    bool startListening(std::function<void(const std::string&)> messageCallback);
    void stop();

private:
    std::string host_;
    int port_;
    std::string queue_;
    bool running_;
    int socket_fd_;
};

} // namespace BitFlow