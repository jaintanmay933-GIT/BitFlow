#include <iostream>
#include <string>
#include <cstdlib>
#include <csignal>
#include <atomic>
#include <curl/curl.h>
#include <nlohmann/json.hpp> 
#include "amqp_consumer.h"
#include "transcoder.h"

using json = nlohmann::json;

// Atomic flag for graceful termination signal handling
std::atomic<bool> g_running{true};

void handleSignal(int signal) {
    std::cout << "\n🛑 [Worker Shutdown] Signal (" << signal << ") received. Stopping consumer gracefully..." << std::endl;
    g_running = false;
}

void processVideoPipeline(BitFlow::Transcoder& engine, const std::string& jobId, const std::string& input, const std::string& output) {
    std::cout << "\n⚡ [Engine Core] Initializing Pipeline for Job: " << jobId << std::endl;

    // Execute FFmpeg transcoding, submit telemetry, and upload final binary
    bool success = engine.processVideoJob(jobId, input, output);

    if (success) {
        std::cout << "🏆 [Engine Core] Pipeline execution successfully finished for Job: " << jobId << "\n" << std::endl;
    } else {
        std::cerr << "💥 [Engine Core] Pipeline failed for Job: " << jobId << "\n" << std::endl;
    }
}

int main() {
    // 1. Register OS termination signals (SIGINT / SIGTERM)
    std::signal(SIGINT, handleSignal);
    std::signal(SIGTERM, handleSignal);

    // 2. Global Libcurl Initialization
    curl_global_init(CURL_GLOBAL_ALL);

    std::cout << "=========================================================" << std::endl;
    std::cout << "🚀 BitFlow High-Performance Distributed C++ Worker Online" << std::endl;
    std::cout << "=========================================================" << std::endl;

    // 3. Resolve RabbitMQ connection credentials & parameters dynamically
    const char* envHost = std::getenv("RABBITMQ_HOST");
    const char* envPort = std::getenv("RABBITMQ_PORT");
    const char* envUser = std::getenv("RABBITMQ_USER");
    const char* envPass = std::getenv("RABBITMQ_PASS");

    std::string amqpHost = (envHost && std::string(envHost).length() > 0) ? envHost : "127.0.0.1";
    int amqpPort = envPort ? std::atoi(envPort) : 5672;
    std::string amqpUser = envUser ? envUser : "guest";
    std::string amqpPass = envPass ? envPass : "guest";

    std::cout << "🔗 [AMQP Setup] Connecting to RabbitMQ at " << amqpHost << ":" << amqpPort << " (User: " << amqpUser << ")" << std::endl;

    // Instantiated once across the daemon lifecycle to prevent repeatedly initializing core drivers
    BitFlow::Transcoder engine;

    try {
        BitFlow::AmqpConsumer queueConsumer(amqpHost, amqpPort, amqpUser, amqpPass, "video_jobs");

        // 4. Task Message Handler Callback
        auto onMessageArrival = [&engine](const std::string& messagePayload) {
            std::cout << "📩 [RabbitMQ] Received task payload: " << messagePayload << std::endl;

            try {
                auto data = json::parse(messagePayload);
                
                std::string jobId = data.value("jobId", "");
                std::string inputPath = data.value("inputPath", "");
                std::string outputPath = data.value("outputPath", "");

                if (jobId.empty() || inputPath.empty() || outputPath.empty()) {
                    std::cerr << "⚠️  [Payload Error] Missing essential JSON fields (jobId, inputPath, outputPath)." << std::endl;
                    return;
                }

                processVideoPipeline(engine, jobId, inputPath, outputPath);

            } catch (const json::parse_error& e) {
                std::cerr << "❌ [JSON Syntax Error] Failed to parse payload: " << e.what() << std::endl;
            } catch (const std::exception& e) {
                std::cerr << "❌ [Pipeline Error] Unexpected exception during processing: " << e.what() << std::endl;
            }
        };

        // 5. Start listening loop until SIGINT/SIGTERM or unrecoverable error
        queueConsumer.startListening(onMessageArrival, g_running);

    } catch (const std::exception& e) {
        std::cerr << "💥 [Fatal Exception] Worker daemon crashed: " << e.what() << std::endl;
    }

    // 6. Global Libcurl Cleanup
    curl_global_cleanup();
    std::cout << "👋 [Worker Shutdown] Cleaned up networking drivers. Exiting." << std::endl;

    return 0;
}