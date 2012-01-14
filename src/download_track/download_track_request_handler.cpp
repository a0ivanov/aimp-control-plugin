// Copyright (c) 2011, Alexey Ivanov

#include "stdafx.h"
#include "request_handler.h"
#include "../aimp/manager.h"

namespace DownloadTrack
{

using namespace AIMPPlayer;
using namespace std;

const std::string playlist_id_tag("/playlist_id/");
const std::string track_id_tag("/track_id/");

PlaylistID getPlaylistID(const std::string& request_uri)
{    
    size_t start_index = request_uri.find(playlist_id_tag);
    if (start_index == string::npos) {
        throw std::runtime_error("can't find playlist_id tag in uri");
    }
    start_index += playlist_id_tag.length();

    const size_t end_index = request_uri.find('/', start_index);
    if (   end_index == string::npos
        || end_index - start_index == 0) 
    {
        throw std::runtime_error("can't extract playlist_id");
    }

    const string id(request_uri.c_str(), start_index, end_index - start_index);
    const PlaylistID playlist_id = boost::lexical_cast<std::size_t>(id);
    return playlist_id;
}

PlaylistEntryID getTrackID(const std::string& request_uri)
{
    size_t start_index = request_uri.find(track_id_tag);
    if (start_index == string::npos) {
        throw std::runtime_error("can't find track_id_tag tag in uri");
    }
    start_index += track_id_tag.length();
    const string id(request_uri.c_str(), start_index, request_uri.length() - start_index);
    const PlaylistID track_id = boost::lexical_cast<std::size_t>(id);
    return track_id;
}

bool fileExists(const std::wstring& filepath)
{
    namespace fs = boost::filesystem;
    const fs::wpath path(filepath);
    return fs::exists(path);
}

std::wstring RequestHandler::getTrackSourcePath(const std::string& request_uri)
{
    const TrackDescription track_desc(getPlaylistID(request_uri), getTrackID(request_uri));
    const PlaylistEntry& entry = aimp_manager_.getEntry(track_desc);
    if (   entry.filename().empty() 
        || !fileExists( entry.filename() ) 
        )
    {
        throw std::runtime_error("Track source does not exist.");
    }
    return entry.filename();
}

} // namespace DownloadTrack
