#pragma once
#include <string>

namespace BitFlow {

class Transcoder {
public:
    Transcoder();
    ~Transcoder();

    // High-performance media extraction & encoding pipeline
    bool processVideo(const std::string& inputPath, const std::string& outputPath);
};

} // namespace BitFlow