#include "transcoder.h"
#include <iostream>
#include <cstdlib>
#include <sstream>
#include <string>
#include <curl/curl.h>
#include <nlohmann/json.hpp>

namespace BitFlow {

// Helper: Dynamically resolve backend base URL
static std::string getBackendUrl() {
    const char* envUrl = std::getenv("BACKEND_URL");
    if (envUrl && std::string(envUrl).length() > 0) {
        std::string url = std::string(envUrl);
        if (url.back() == '/') url.pop_back(); // Remove trailing slash if present
        return url;
    }
    return "https://bitflow-backend-047r.onrender.com";
}

// Helper: Escape double quotes for shell execution safety
static std::string escapeShellArg(const std::string& arg) {
    std::string escaped = arg;
    size_t pos = 0;
    while ((pos = escaped.find('"', pos)) != std::string::npos) {
        escaped.replace(pos, 1, "\\\"");
        pos += 2;
    }
    return escaped;
}

Transcoder::Transcoder() {
    curl_global_init(CURL_GLOBAL_ALL);
    std::cout << "🎞️  [Transcoder Engine] Core Media Drivers & libcurl Loaded Successfully" << std::endl;
}

Transcoder::~Transcoder() {
    curl_global_cleanup();
}

bool Transcoder::processVideoJob(const std::string& jobId, const std::string& inputPath, const std::string& outputPath) {
    std::cout << "🎬 [Processing Started] Job ID: " << jobId << " | Target Source Asset: " << inputPath << std::endl;

    // 1. Telemetry: Job Started
    this->sendHttpUpdate(jobId, "PROCESSING", 10);

    // Ensure local output directory structure exists
    std::string mkdirCmd = "mkdir -p \"$(dirname \"" + escapeShellArg(outputPath) + "\")\"";
    std::system(mkdirCmd.c_str());

    // 2. Telemetry: Transcoding In Progress
    this->sendHttpUpdate(jobId, "PROCESSING", 45);

    // Construct FFmpeg command (Input URL/File -> Output MP4)
    std::stringstream ffmpegCmd;
    ffmpegCmd << "ffmpeg -y -i \"" << escapeShellArg(inputPath) << "\""
              << " -c:v libx264 -crf 23 -preset medium "
              << " -c:a aac -b:a 128k "
              << "\"" << escapeShellArg(outputPath) << "\"";

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

// Native libcurl HTTP POST for telemetry updates
void Transcoder::sendHttpUpdate(const std::string& jobId, const std::string& status, int percentage, const std::string& errorMessage) {
    std::cout << "📡 [HTTP Telemetry Dispatch] Job " << jobId << " -> " << status << " (" << percentage << "%)" << std::endl;

    std::string endpoint = getBackendUrl() + "/api/progress";

    nlohmann::json jsonPayload = {
        {"jobId", jobId},
        {"percentage", percentage},
        {"status", status},
        {"errorMessage", errorMessage}
    };

    std::string payloadStr = jsonPayload.dump();

    CURL* curl = curl_easy_init();
    if (!curl) {
        std::cerr << "⚠️  [HTTP Telemetry Failed] Could not initialize libcurl handle" << std::endl;
        return;
    }

    struct curl_slist* headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");

    curl_easy_setopt(curl, CURLOPT_URL, endpoint.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payloadStr.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        std::cerr << "⚠️  [HTTP Telemetry Failed] libcurl error: " << curl_easy_strerror(res) << std::endl;
    }

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
}

// Native libcurl multipart/form-data upload for completed output MP4
bool Transcoder::uploadOutputVideo(const std::string& jobId, const std::string& outputPath) {
    std::cout << "📤 [Output Sync] Transmitting completed video to gateway..." << std::endl;

    std::string endpoint = getBackendUrl() + "/api/upload-output/" + jobId;

    CURL* curl = curl_easy_init();
    if (!curl) {
        std::cerr << "❌ [Upload Failed] Unable to initialize libcurl" << std::endl;
        return false;
    }

    curl_mime* mime = curl_mime_init(curl);
    curl_mimepart* part = curl_mime_addpart(mime);

    curl_mime_name(part, "video");
    curl_mime_filedata(part, outputPath.c_str());

    curl_easy_setopt(curl, CURLOPT_URL, endpoint.c_str());
    curl_easy_setopt(curl, CURLOPT_MIMEPOST, mime);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 300L); // 5-minute upload timeout for large videos

    CURLcode res = curl_easy_perform(curl);
    long responseCode = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &responseCode);

    bool success = (res == CURLE_OK && responseCode >= 200 && responseCode < 300);

    if (!success) {
        std::cerr << "❌ [Upload Failed] libcurl status: " << curl_easy_strerror(res) 
                  << " | HTTP Code: " << responseCode << std::endl;
    }

    curl_mime_free(mime);
    curl_easy_cleanup(curl);

    return success;
}

} // namespace BitFlow