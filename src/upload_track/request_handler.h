// Copyright (c) 2013, Alexey Ivanov

#pragma once

namespace AIMPPlayer { class AIMPManager; }
namespace Http {
    struct Request; 
    struct Reply;
}

namespace UploadTrack
{

class RequestHandler : boost::noncopyable
{
public:
    RequestHandler(AIMPPlayer::AIMPManager& aimp_manager)
        :
        aimp_manager_(aimp_manager)
    {}

    bool handle_request(const Http::Request& req, Http::Reply& rep); // throws std::exception.

private:

    AIMPPlayer::AIMPManager& aimp_manager_;
};

} // namespace UploadTrack
