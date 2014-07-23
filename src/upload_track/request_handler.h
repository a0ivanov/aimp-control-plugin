// Copyright (c) 2014, Alexey Ivanov

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
    RequestHandler(AIMPPlayer::AIMPManager& aimp_manager, bool enabled)
        :
        aimp_manager_(aimp_manager),
        enabled_(enabled)
    {}

    bool handle_request(const Http::Request& req, Http::Reply& rep); // throws std::exception.

private:

    int getTargetPlaylist();

    AIMPPlayer::AIMPManager& aimp_manager_;
    bool enabled_;
};

} // namespace UploadTrack
