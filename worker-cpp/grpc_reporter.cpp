#include "grpc_reporter.h"
#include <iostream>

namespace BitFlow {

GrpcReporter::GrpcReporter(const std::string& targetAddress) {
    // Instantiate an insecure network channel directly to the Node.js gRPC port
    auto channel = grpc::CreateChannel(targetAddress, grpc::InsecureChannelCredentials());
    stub_ = bitflow::BitFlowCallbackService::NewStub(channel);
    std::cout << "📡 [gRPC Reporter] Client socket channel bound to target: " << targetAddress << std::endl;
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