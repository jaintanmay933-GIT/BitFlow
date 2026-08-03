#include "transcoder.h"
#include "grpc_reporter.h"
#include <iostream>
#include <cstdlib>
#include <sstream>

namespace BitFlow {

Transcoder::Transcoder() {
    std::cout << "🎞️  [Transcoder Engine] Core Media Drivers Loaded Successfully" << std::endl;
}

Transcoder::~Transcoder() {}

bool Transcoder::processVideoJob(const std::string& jobId, const std::string& inputPath, const std::string& outputPath) {
    std::cout << "🎬 [Processing Started] Job ID: " << jobId << " | Target Source Asset: " << inputPath << std::endl;

    this->sendGrpcUpdate(jobId, "PROCESSING", 0);

    std::string mkdirCmd = "mkdir -p \"$(dirname \"" + outputPath + "\")\"";
    std::system(mkdirCmd.c_str());

    this->sendGrpcUpdate(jobId, "PROCESSING", 45);

    std::stringstream ffmpegCmd;
  ffmpegCmd << "ffmpeg -y -i \"" << inputPath << "\""
          << " -c:v libx264 -crf 23 -preset medium "
          << " -c:a aac -b:a 128k "
          << "\"" << outputPath << "\"";

    std::cout << "⚙️  [FFmpeg Execution] Executing binary stream processing..." << std::endl;

    int status = std::system(ffmpegCmd.str().c_str());

    if (status == 0) {
        std::cout << "✨ [Processing Completed] Transcoded Asset Saved: " << outputPath << std::endl;
        this->sendGrpcUpdate(jobId, "COMPLETED", 100);
        return true;
    } else {
        std::cerr << "❌ [FFmpeg Error] Media pipeline stream layout decoding failed!" << std::endl;
        this->sendGrpcUpdate(jobId, "FAILED", 0, "FFmpeg native rendering failure");
        return false;
    }
}

void Transcoder::sendGrpcUpdate(const std::string& jobId, const std::string& status, int percentage, const std::string& errorMessage) {
    std::cout << "📡 [gRPC Telemetry Dispatch] Job " << jobId << " -> " << status << " (" << percentage << "%)" << std::endl;
    
    try {
        GrpcReporter reporter("127.0.0.1:50051");
        reporter.reportProgress(jobId, percentage, status, errorMessage);
    } catch (const std::exception& e) {
        std::cerr << "❌ [gRPC Connection Exception] Failed to send telemetry: " << e.what() << std::endl;
    }
}

} // namespace BitFlow