#include "transcoder.h"
#include <iostream>
#include <cstdlib>
#include <sstream>
#include <string>

namespace BitFlow {

// Helper function to resolve the backend API URL dynamically from environment
static std::string getBackendUrl() {
    const char* envUrl = std::getenv("BACKEND_URL");
    if (envUrl && std::string(envUrl).length() > 0) {
        std::string url = std::string(envUrl);
        if (url.back() == '/') url.pop_back(); // Remove trailing slash if present
        return url;
    }
    return "https://bitflow-backend-047r.onrender.com";
}

Transcoder::Transcoder() {
    std::cout << "🎞️  [Transcoder Engine] Core Media Drivers Loaded Successfully" << std::endl;
}

Transcoder::~Transcoder() {}

bool Transcoder::processVideoJob(const std::string& jobId, const std::string& inputPath, const std::string& outputPath) {
    std::cout << "🎬 [Processing Started] Job ID: " << jobId << " | Target Source Asset: " << inputPath << std::endl;

    // 1. Telemetry: Job Started
    this->sendHttpUpdate(jobId, "PROCESSING", 10);

    // Ensure local output directory structure exists
    std::string mkdirCmd = "mkdir -p \"$(dirname \"" + outputPath + "\")\"";
    std::system(mkdirCmd.c_str());

    // 2. Telemetry: Transcoding In Progress
    this->sendHttpUpdate(jobId, "PROCESSING", 45);

    // Construct FFmpeg command (Input URL or local file -> Output MP4)
    std::stringstream ffmpegCmd;
    ffmpegCmd << "ffmpeg -y -i \"" << inputPath << "\""
              << " -c:v libx264 -crf 23 -preset medium "
              << " -c:a aac -b:a 128k "
              << "\"" << outputPath << "\"";

    std::cout << "⚙️  [FFmpeg Execution] Executing binary stream processing..." << std::endl;

    int status = std::system(ffmpegCmd.str().c_str());

    if (status == 0) {
        std::cout << "✨ [FFmpeg Completed] Transcoded Asset Saved locally: " << outputPath << std::endl;

        // 3. Upload completed video file back to backend gateway storage
        bool uploadSuccess = this->uploadOutputVideo(jobId, outputPath);

        if (uploadSuccess) {
            // 4. Telemetry: Transcoding & Upload Completed
            this->sendHttpUpdate(jobId, "COMPLETED", 100);
            return true;
        } else {
            std::cerr << "❌ [Upload Error] Transcoded video upload to gateway failed!" << std::endl;
            this->sendHttpUpdate(jobId, "FAILED", 0, "Transcoded output video sync failed");
            return false;
        }
    } else {
        std::cerr << "❌ [FFmpeg Error] Media pipeline stream layout decoding failed!" << std::endl;
        this->sendHttpUpdate(jobId, "FAILED", 0, "FFmpeg native rendering failure");
        return false;
    }
}

// Sends progress telemetry updates to the backend via HTTP REST POST
void Transcoder::sendHttpUpdate(const std::string& jobId, const std::string& status, int percentage, const std::string& errorMessage) {
    std::cout << "📡 [HTTP Telemetry Dispatch] Job " << jobId << " -> " << status << " (" << percentage << "%)" << std::endl;

    std::string backendUrl = getBackendUrl();
    std::string endpoint = backendUrl + "/api/progress";

    std::stringstream jsonPayload;
    jsonPayload << "{"
                << "\"jobId\":\"" << jobId << "\","
                << "\"percentage\":" << percentage << ","
                << "\"status\":\"" << status << "\","
                << "\"errorMessage\":\"" << errorMessage << "\""
                << "}";

    std::stringstream curlCmd;
    curlCmd << "curl -s -X POST \"" << endpoint << "\""
            << " -H \"Content-Type: application/json\""
            << " -d '" << jsonPayload.str() << "' > /dev/null";

    int ret = std::system(curlCmd.str().c_str());
    if (ret != 0) {
        std::cerr << "⚠️  [HTTP Telemetry Failed] Failed to send status update for Job: " << jobId << std::endl;
    }
}

// Uploads the generated MP4 file back to backend disk storage over HTTP
bool Transcoder::uploadOutputVideo(const std::string& jobId, const std::string& outputPath) {
    std::cout << "📤 [Output Sync] Transmitting completed video to gateway..." << std::endl;

    std::string backendUrl = getBackendUrl();
    std::string endpoint = backendUrl + "/api/upload-output/" + jobId;

    std::stringstream curlCmd;
    curlCmd << "curl -s -X POST \"" << endpoint << "\""
            << " -F \"video=@" << outputPath << "\""
            << " > /dev/null";

    int ret = std::system(curlCmd.str().c_str());
    return (ret == 0);
}

} // namespace BitFlow