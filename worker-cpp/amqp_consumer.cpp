#include "amqp_consumer.h"
#include <iostream>
#include <cstdlib>

namespace BitFlow {

AmqpConsumer::AmqpConsumer(const std::string& host, int port, const std::string& queue)
    : host_(host), port_(port), queue_(queue), running_(false) {}

AmqpConsumer::~AmqpConsumer() {
    stop();
}

bool AmqpConsumer::startListening(std::function<void(const std::string&)> messageCallback) {
    try {
        AmqpClient::Channel::OpenOpts opts;

        // 1. Check if full RABBITMQ_URL environment variable is provided
        const char* envUrl = std::getenv("RABBITMQ_URL");
        if (!envUrl) {
            envUrl = std::getenv("AMQP_URL");
        }

        if (envUrl && std::string(envUrl).length() > 0) {
            // Parse full URL (e.g., amqp://user:pass@127.0.0.1:5672/vhost)
            opts = AmqpClient::Channel::OpenOpts::FromUri(envUrl);
        } else {
            // 2. Read individual environment variables or fall back to defaults
            const char* envUser = std::getenv("RABBITMQ_USER");
            const char* envPass = std::getenv("RABBITMQ_PASS");
            const char* envVhost = std::getenv("RABBITMQ_VHOST");

            std::string user = envUser ? envUser : "guest";
            std::string pass = envPass ? envPass : "guest";
            std::string vhost = envVhost ? envVhost : "/";

            opts.host = host_;
            opts.port = port_;
            opts.vhost = vhost;
            opts.auth = AmqpClient::Channel::OpenOpts::BasicAuth(user, pass);
        }

        channel_ = AmqpClient::Channel::Open(opts);
        
        // Ensure destination queue exists (durable = true)
        channel_->DeclareQueue(queue_, false, true, false, false);
        
        // Subscribe to queue
        consumer_tag_ = channel_->BasicConsume(queue_, "", true, true, false);
        
        running_ = true;
        std::cout << "[AMQP] Successfully subscribed to queue: " << queue_ << std::endl;

        while (running_) {
            AmqpClient::Envelope::ptr_t envelope;
            // Wait up to 1000ms for a message before looping
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