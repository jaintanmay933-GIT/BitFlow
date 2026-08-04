#include "grpc_reporter.h"
#include <iostream>
#include <cstdlib>
#include <chrono>
#include <memory>

namespace BitFlow {

GrpcReporter::GrpcReporter(const std::string& targetAddress) {
    std::string address = targetAddress;

    // Fallback to GRPC_HOST environment variable if targetAddress is empty or hardcoded localhost
    if (address.empty() || address == "127.0.0.1:50051" || address == "localhost:50051") {
        const char* envHost = std::getenv("GRPC_HOST");
        if (envHost && std::string(envHost).length() > 0) {
            address = envHost;
        } else {
            address = "127.0.0.1:50051"; // Default fallback for local testing
        }
    }

    // Determine credentials based on port or GRPC_USE_SSL environment variable
    std::shared_ptr<grpc::ChannelCredentials> creds;
    const char* envSsl = std::getenv("GRPC_USE_SSL");
    bool forceSsl = (envSsl && (std::string(envSsl) == "1" || std::string(envSsl) == "true"));

    // Use SSL/TLS when targeting port 443 (Render HTTPS) or when GRPC_USE_SSL=true
    if (forceSsl || address.find(":443") != std::string::npos) {
        creds = grpc::SslCredentials(grpc::SslCredentialsOptions());
        std::cout << "🔒 [gRPC Reporter] Using SSL/TLS Secure Channel Credentials." << std::endl;
    } else {
        creds = grpc::InsecureChannelCredentials();
        std::cout << "🔓 [gRPC Reporter] Using Insecure Channel Credentials." << std::endl;
    }

    // Instantiate network channel to Node.js gRPC server
    auto channel = grpc::CreateChannel(address, creds);
    stub_ = bitflow::BitFlowCallbackService::NewStub(channel);
    std::cout << "📡 [gRPC Reporter] Client socket channel bound to target: " << address << std::endl;
}

GrpcReporter::~GrpcReporter() {}

bool GrpcReporter::reportProgress(const std::string& jobId, int percentage, const std::string& status, const std::string& errorMessage) {
    bitflow::ProgressRequest request;
    request.set_job_id(jobId);
    request.set_percentage(percentage);
    request.set_status(status);
    
    if (!errorMessage.empty()) {
        request.set_error_message(errorMessage);
    }

    bitflow::ProgressResponse response;
    grpc::ClientContext context;

    // Set a 5-second RPC deadline so requests fail gracefully if the backend is unreachable
    std::chrono::system_clock::time_point deadline = std::chrono::system_clock::now() + std::chrono::seconds(5);
    context.set_deadline(deadline);

    // Execute the RPC remote procedure call over the wire
    grpc::Status grpcStatus = stub_->UpdateProgress(&context, request, &response);

    if (grpcStatus.ok() && response.success()) {
        return true;
    } else {
        std::cerr << "⚠️  [gRPC Report Failed] Code: " << grpcStatus.error_code() 
                  << " | Msg: " << grpcStatus.error_message() << std::endl;
        return false;
    }
}

} // namespace BitFlow