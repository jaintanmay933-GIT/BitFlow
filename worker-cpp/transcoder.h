#ifndef TRANSCODER_H
#define TRANSCODER_H

#include <string>

namespace BitFlow {

class Transcoder {
public:
    Transcoder();
    ~Transcoder();

    bool processVideoJob(const std::string& jobId, const std::string& inputPath, const std::string& outputPath);

private:
    void sendGrpcUpdate(const std::string& jobId, const std::string& status, int percentage, const std::string& errorMessage = "");
};

} // namespace BitFlow

#endif // TRANSCODER_H