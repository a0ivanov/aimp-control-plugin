// Copyright (c) 2013, Alexey Ivanov

#ifndef DOWNLOAD_TRACK_REQUEST_HANDLER_H
#define DOWNLOAD_TRACK_REQUEST_HANDLER_H

namespace AIMPPlayer { class AIMPManager; }
namespace Http {
    struct Request; 
    struct Reply;
}

namespace DownloadTrack
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

    std::wstring getTrackSourcePath(const std::string& request_uri); // throws std::exception.


    AIMPPlayer::AIMPManager& aimp_manager_;
};

} // namespace DownloadTrack

#endif // #ifndef DOWNLOAD_TRACK_REQUEST_HANDLER_H
