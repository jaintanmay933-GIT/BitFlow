#include "amqp_consumer.h"
#include <iostream>

namespace BitFlow {

AmqpConsumer::AmqpConsumer(const std::string& host, int port, const std::string& queue)
    : host_(host), port_(port), queue_(queue), running_(false) {}

AmqpConsumer::~AmqpConsumer() {
    stop();
}

bool AmqpConsumer::startListening(std::function<void(const std::string&)> messageCallback) {
    try {
       AmqpClient::Channel::OpenOpts opts;
opts.host = host_;
opts.port = port_;

// Add your RabbitMQ username and password here
opts.auth = AmqpClient::Channel::OpenOpts::BasicAuth("guest", "guest"); 

channel_ = AmqpClient::Channel::Open(opts);
        
        // Ensure destination queue exists (durable = true)
        channel_->DeclareQueue(queue_, false, true, false, false);
        
        // Subscribe to queue (auto_ack = true for simplicity)
        consumer_tag_ = channel_->BasicConsume(queue_, "", true, true, false);
        
        running_ = true;
        std::cout << "[AMQP] Successfully subscribed to queue: " << queue_ << std::endl;

        while (running_) {
            AmqpClient::Envelope::ptr_t envelope;
            // Wait up to 1000ms for a message before looping (prevents blocking indefinitely on exit)
            if (channel_->BasicConsumeMessage(consumer_tag_, envelope, 1000)) {
                std::string payload = envelope->Message()->Body();
                
                if (messageCallback) {
                    messageCallback(payload);
                }
            }
        }
        return true;
    } catch (const std::exception& e) {
        std::cerr << "[AMQP Error] " << e.what() << std::endl;
        return false;
    }
}

void AmqpConsumer::stop() {
    if (!running_) return;
    
    running_ = false;
    try {
        if (channel_ && !consumer_tag_.empty()) {
            channel_->BasicCancel(consumer_tag_);
        }
    } catch (...) {
        // Suppress cleanup exceptions during shutdown
    }
    std::cout << "[AMQP] Consumer stopped gracefully." << std::endl;
}

} // namespace BitFlow