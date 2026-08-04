#include <iostream>
#include <string>
#include <cstdlib>
#include <nlohmann/json.hpp> 
#include "amqp_consumer.h"
#include "transcoder.h"

using json = nlohmann::json;

void processVideoPipeline(const std::string& jobId, const std::string& input, const std::string& output) {
    BitFlow::Transcoder engine;

    std::cout << "\n⚡ [Engine Core] Initializing Pipeline for Job: " << jobId << std::endl;

    // Call FFmpeg to transcode video and trigger HTTP progress updates / file upload
    bool success = engine.processVideoJob(jobId, input, output);

    if (success) {
        std::cout << "🏆 [Engine Core] Pipeline execution successfully finished for Job: " << jobId << "\n" << std::endl;
    } else {
        std::cerr << "💥 [Engine Core] Pipeline crashed for Job: " << jobId << "\n" << std::endl;
    }
}

int main() {
    std::cout << "=========================================================" << std::endl;
    std::cout << "🚀 BitFlow High-Performance Distributed C++ Worker Online" << std::endl;
    std::cout << "=========================================================" << std::endl;

    // Read dynamic RabbitMQ host/port from environment variables
    const char* envHost = std::getenv("RABBITMQ_HOST");
    const char* envPort = std::getenv("RABBITMQ_PORT");

    std::string amqpHost = envHost ? envHost : "127.0.0.1";
    int amqpPort = envPort ? std::atoi(envPort) : 5672;

    std::cout << "🔗 [AMQP Setup] Connecting securely to RabbitMQ at " << amqpHost << ":" << amqpPort << std::endl;

    BitFlow::AmqpConsumer queueConsumer(amqpHost, amqpPort, "video_jobs");

    // Lambda callback triggered whenever RabbitMQ receives a job
    auto onMessageArrival = [](const std::string& messagePayload) {
        std::cout << "📩 [RabbitMQ] Received task payload: " << messagePayload << std::endl;

        try {
            // Parse incoming JSON payload from Node.js gateway
            auto data = json::parse(messagePayload);
            
            std::string jobId = data.value("jobId", "");
            std::string inputPath = data.value("inputPath", "");
            std::string outputPath = data.value("outputPath", "");

            if (jobId.empty() || inputPath.empty() || outputPath.empty()) {
                std::cerr << "⚠️  [Payload Error] Missing essential JSON fields in message." << std::endl;
                return;
            }

            // Execute processing pipeline
            processVideoPipeline(jobId, inputPath, outputPath);

        } catch (const std::exception& e) {
            std::cerr << "❌ [JSON Error] Failed to parse payload: " << e.what() << std::endl;
        }
    };

    // Keep daemon listening for incoming tasks
    queueConsumer.startListening(onMessageArrival);

    return 0;
}