#pragma once

#ifndef TRANSCODER_H
#define TRANSCODER_H

#include <string>

namespace BitFlow {

class Transcoder {
public:
    Transcoder();
    ~Transcoder();

    // Prevent copying to avoid duplicate network or handle state across worker threads
    Transcoder(const Transcoder&) = delete;
    Transcoder& operator=(const Transcoder&) = delete;

    // Allow move semantics
    Transcoder(Transcoder&&) noexcept = default;
    Transcoder& operator=(Transcoder&&) noexcept = default;

    /**
     * @brief Executes the FFmpeg transcoding job, dispatches progress telemetry, and uploads output.
     * @return true if transcoding and upload succeeded; false otherwise.
     */
    [[nodiscard]] bool processVideoJob(const std::string& jobId, 
                                       const std::string& inputPath, 
                                       const std::string& outputPath);

private:
    /**
     * @brief Dispatches progress and status updates to the backend API via HTTP POST.
     */
    void sendHttpUpdate(const std::string& jobId, 
                        const std::string& status, 
                        int percentage, 
                        const std::string& errorMessage = "");

    /**
     * @brief Uploads the completed output MP4 file back to backend storage.
     * @return true if the backend server responded with HTTP 200 OK.
     */
    [[nodiscard]] bool uploadOutputVideo(const std::string& jobId, 
                                          const std::string& outputPath);
};

} // namespace BitFlow

#endif // TRANSCODER_H