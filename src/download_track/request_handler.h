// Copyright (c) 2011, Alexey Ivanov

#ifndef DOWNLOAD_TRACK_REQUEST_HANDLER_H
#define DOWNLOAD_TRACK_REQUEST_HANDLER_H

namespace AIMPPlayer {
    class AIMPManager;
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

    std::wstring getTrackSourcePath(const std::string& request_uri); // throws std::exception.

private:

    AIMPPlayer::AIMPManager& aimp_manager_;
};

} // namespace DownloadTrack

#endif // #ifndef DOWNLOAD_TRACK_REQUEST_HANDLER_H
