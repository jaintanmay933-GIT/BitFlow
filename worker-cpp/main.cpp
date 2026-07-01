#include <iostream>
#include <string>
#include <unistd.h>
#include "amqp_consumer.h"
#include "transcoder.h"
#include "grpc_reporter.h"

// Simulate pulling structural details from an incoming queue payload
void simulateProcessingPipeline(const std::string& jobId, const std::string& input, const std::string& output) {
    BitFlow::Transcoder engine;
    BitFlow::GrpcReporter reporter("localhost:50051");

    std::cout << "\n⚡ [Engine Core] Initializing Pipeline for Job: " << jobId << std::endl;

    // 1. Fire up a processing update to move status to 'PROCESSING'
    reporter.reportProgress(jobId, 0, "PROCESSING");

    // 2. Run simulation ticks to mimic frame extraction progress reporting
    std::cout << "📈 [Engine Core] Encoding multimedia stream vectors..." << std::endl;
    for (int i = 25; i <= 75; i += 25) {
        usleep(500000); // 0.5-second processing steps
        reporter.reportProgress(jobId, i, "PROCESSING");
    }

    // 3. Execute the actual underlying multimedia transcode operation via FFmpeg
    bool success = engine.processVideo(input, output);

    // 4. Update the final state based on hardware exit codes
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

    // Instantiate our custom underlying TCP message block consumer 
    BitFlow::AmqpConsumer queueConsumer("127.0.0.1", 5672, "video_jobs");

    // Provide a lambda event listener that handles execution threads when incoming jobs land
    auto onMessageArrival = [](const std::string& dummyPayload) {
        // In full pipeline state, this parses incoming JSON targets string from RabbitMQ
        // For our test simulation, we run an automated internal profile target asset:
        simulateProcessingPipeline("demo-job-id-123", "test_input.mp4", "output/processed_demo.mp4");
    };

    // Keep our system daemon block listening for data arrivals
    queueConsumer.startListening(onMessageArrival);

    return 0;
}