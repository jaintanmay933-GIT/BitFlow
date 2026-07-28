#pragma once

#include <string>
#include <functional>
#include <memory>
#include <SimpleAmqpClient/SimpleAmqpClient.h>

namespace BitFlow {

class AmqpConsumer {
public:
    AmqpConsumer(const std::string& host, int port, const std::string& queue);
    ~AmqpConsumer();

    // Begins the blocking consume loop, executing messageCallback for each JSON payload
    bool startListening(std::function<void(const std::string&)> messageCallback);
    
    // Safely halts the worker loop
    void stop();

private:
    std::string host_;
    int port_;
    std::string queue_;
    bool running_;
    
    // Managed RabbitMQ channel pointer
    AmqpClient::Channel::ptr_t channel_;
    std::string consumer_tag_;
};

} // namespace BitFlow