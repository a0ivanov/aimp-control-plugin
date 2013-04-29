// Copyright (c) 2013, Alexey Ivanov

#pragma once

#include <boost/filesystem.hpp>

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
    RequestHandler(AIMPPlayer::AIMPManager& aimp_manager, boost::filesystem::wpath temp_dir, bool enabled)
        :
        aimp_manager_(aimp_manager),
        temp_dir_(temp_dir),
        enabled_(enabled),
        target_playlist_id_(0),
        target_playlist_id_created_(false)
    {}

    bool handle_request(const Http::Request& req, Http::Reply& rep); // throws std::exception.

private:

    int getTargetPlaylist();

    AIMPPlayer::AIMPManager& aimp_manager_;
    boost::filesystem::wpath temp_dir_;
    bool enabled_;
    int target_playlist_id_;
    bool target_playlist_id_created_;
};

} // namespace UploadTrack
