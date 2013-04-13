// Copyright (c) 2013, Alexey Ivanov

#include "stdafx.h"
#include "request_handler.h"
#include "../aimp/manager.h"
#include "../aimp/manager_impl_common.h"
#include "../http_server/reply.h"
#include "../http_server/request.h"
#include "../http_server/mime_types.h"

#include "utils/string_encoding.h"
#include "utils/util.h"

struct sqlite3;

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
    const std::wstring& entry_filename = getEntryField<std::wstring>(AIMPPlayer::getPlaylistsDB(aimp_manager_),
                                                                     "filename",
                                                                     aimp_manager_.getAbsoluteTrackDesc(track_desc)
                                                                     ); 
    if (   entry_filename.empty() 
        || !fileExists(entry_filename) 
        )
    {
        throw std::runtime_error("Track source does not exist.");
    }
    return entry_filename;
}

bool RequestHandler::handle_request(const Http::Request& req, Http::Reply& rep)
{
    using namespace Http;

    try {
        namespace fs = boost::filesystem;
        rep.filename = getTrackSourcePath(req.uri);
        const fs::wpath path(rep.filename);
        
        const uintmax_t file_size = fs::file_size(path);

        // fill http headers.
        rep.status = Reply::ok;
        rep.headers.resize(3);
        rep.headers[0].name = "Content-Length";
        rep.headers[0].value = boost::lexical_cast<std::string>(file_size);
        rep.headers[1].name = "Content-Type";
        rep.headers[1].value = mime_types::extension_to_type( path.extension().string().c_str() );
        rep.headers[2].name = "Content-Disposition";
        rep.headers[2].value = Utilities::MakeString() << "attachment; filename=\"" << StringEncoding::utf16_to_utf8( path.filename().native() ) << "\"";
    } catch (std::exception&) {
        rep.filename.clear();
        rep = Reply::stock_reply(Reply::not_found);
    }
    return true;
}

} // namespace DownloadTrack
