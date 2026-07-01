#include "transcoder.h"
#include <iostream>
#include <cstdlib>
#include <sstream>

namespace BitFlow {

Transcoder::Transcoder() {
    std::cout << "🎞️  [Transcoder Engine] Core Media Drivers Loaded Successfully" << std::endl;
}

Transcoder::~Transcoder() {}

bool Transcoder::processVideo(const std::string& inputPath, const std::string& outputPath) {
    std::cout << "🎬 [Processing Started] Target Source Asset: " << inputPath << std::endl;

    // Build automated output folder creation shell command
    std::string mkdirCmd = "mkdir -p $(dirname " + outputPath + ")";
    std::system(mkdirCmd.c_str());

    // Construct high-performance FFmpeg acceleration command.
    // This tells FFmpeg to transcode to H.264 video encoding at an optimized 2Mbps bitrate.
    std::stringstream ffmpegCmd;
    ffmpegCmd << "ffmpeg -y -i " << inputPath 
              << " -vcodec libx264 -b:v 2000k -acodec aac -b:a 128k " 
              << outputPath << " 2>/dev/null";

    std::cout << "⚙️  [FFmpeg Execution] Compiling stream vectors..." << std::endl;
    
    // Execute transcode loop
    int status = std::system(ffmpegCmd.str().c_str());

    if (status == 0) {
        std::cout << "✨ [Processing Completed] Transcoded Asset Saved: " << outputPath << std::endl;
        return true;
    } else {
        std::cerr << "❌ [FFmpeg Error] Media pipeline stream layout decoding failed!" << std::endl;
        return false;
    }
}

} // namespace BitFlow