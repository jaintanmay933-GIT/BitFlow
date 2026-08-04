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
        // Read credentials directly from Render Environment Variables
        const char* envUser = std::getenv("RABBITMQ_USER");
        const char* envPass = std::getenv("RABBITMQ_PASS");
        const char* envVhost = std::getenv("RABBITMQ_VHOST");

        std::string user = envUser ? envUser : "guest";
        std::string pass = envPass ? envPass : "guest";
        std::string vhost = envVhost ? envVhost : "/";

        // IMPORTANT: CreateSecure handles TLS natively on port 5671 without crashing over missing client certs
        channel_ = AmqpClient::Channel::CreateSecure(
            "",        // CA Cert Path (Empty uses the OS system default)
            host_,     // CloudAMQP Host (passed from main.cpp)
            "",        // Client Cert Path (Empty because CloudAMQP doesn't need it)
            "",        // Client Key Path
            port_,     // Port (5671 passed from main.cpp)
            user,      // Username
            pass,      // Password
            vhost      // Vhost
        );
        
        // Ensure destination queue exists (durable = true)
        channel_->DeclareQueue(queue_, false, true, false, false);
        
        // Subscribe to queue
        consumer_tag_ = channel_->BasicConsume(queue_, "", true, true, false);
        
        running_ = true;
        std::cout << "✅ [AMQP] Successfully connected securely to CloudAMQP and subscribed to: " << queue_ << std::endl;

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
        std::cerr << "❌ [AMQP SSL Error] " << e.what() << std::endl;
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
    std::cout << "🛑 [AMQP] Consumer stopped gracefully." << std::endl;
}

} // namespace BitFlow