#include <iostream>
#include <string>
#include <cstdlib>
#include <unistd.h>
#include <nlohmann/json.hpp> // Lightweight header-only C++ JSON library
#include "amqp_consumer.h"
#include "transcoder.h"
#include "grpc_reporter.h"

using json = nlohmann::json;

void processVideoPipeline(const std::string& jobId, const std::string& input, const std::string& output) {
    BitFlow::Transcoder engine;

    // 1. Fetch gRPC endpoint dynamically from environment (fallback to localhost:50051)
    const char* envGrpc = std::getenv("GRPC_HOST");
    std::string grpcHost = envGrpc ? envGrpc : "localhost:50051";

    BitFlow::GrpcReporter reporter(grpcHost);

    std::cout << "\n⚡ [Engine Core] Initializing Pipeline for Job: " << jobId << std::endl;

    // Notify Node.js backend that processing has started
    reporter.reportProgress(jobId, 0, "PROCESSING");

    // Report intermediate heartbeat ticks
    for (int i = 25; i <= 75; i += 25) {
        usleep(300000); // 0.3-second progress steps
        reporter.reportProgress(jobId, i, "PROCESSING");
    }

    // Call FFmpeg to transcode the actual uploaded video
    bool success = engine.processVideoJob(jobId, input, output);

    // Update the final DB state over gRPC based on exit codes
    if (success) {
        reporter.reportProgress(jobId, 100, "COMPLETED");
        std::cout << "🏆 [Engine Core] Pipeline execution successfully finished for Job: " << jobId << "\n" << std::endl;
    } else {
        reporter.reportProgress(jobId, 0, "FAILED", "FFmpeg native rendering failure");
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

    std::cout << "🔗 [AMQP Setup] Connecting to RabbitMQ at " << amqpHost << ":" << amqpPort << std::endl;

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

            // Execute processing on real paths
            processVideoPipeline(jobId, inputPath, outputPath);

        } catch (const std::exception& e) {
            std::cerr << "❌ [JSON Error] Failed to parse payload: " << e.what() << std::endl;
        }
    };

    // Keep daemon listening for incoming tasks
    queueConsumer.startListening(onMessageArrival);

    return 0;
}