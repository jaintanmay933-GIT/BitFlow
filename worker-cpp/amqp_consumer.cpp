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
        const char* envCaPath = std::getenv("RABBITMQ_CA_PATH");

        std::string user = envUser ? envUser : "guest";
        std::string pass = envPass ? envPass : "guest";
        std::string vhost = envVhost ? envVhost : "/";
        
        // Point OpenSSL / librabbitmq to Linux system CA bundle
        std::string caPath = envCaPath ? envCaPath : "/etc/ssl/certs/ca-certificates.crt";

        // Pass caPath as parameter 1 so OpenSSL can verify CloudAMQP's TLS cert
        channel_ = AmqpClient::Channel::CreateSecure(
            caPath,    // CA Cert Path (points to Linux system trusted root bundle)
            host_,     // CloudAMQP Host
            "",        // Client Key Path
            "",        // Client Cert Path
            port_,     // Port (5671)
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