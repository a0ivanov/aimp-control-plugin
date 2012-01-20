// Copyright (c) 2011, Alexey Ivanov

#include "stdafx.h"
#include "request_handler.h"
#include "../aimp/manager.h"

#include "../http_server/reply.h"
#include "../http_server/request.h"
#include "../http_server/mime_types.h"

#include "utils/string_encoding.h"
#include "utils/util.h"

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

bool RequestHandler::handle_request(const Http::Request& req, Http::Reply& rep)
{
    using namespace Http;

    try {
        namespace fs = boost::filesystem;
        const std::wstring track_source_path( getTrackSourcePath(req.uri) );

        // Open the file to send back.
        const std::string full_path = StringEncoding::utf16_to_system_ansi_encoding(track_source_path);
        std::ifstream is(full_path.c_str(), std::ios::in | std::ios::binary);
        if (!is) {
            rep = Reply::stock_reply(Reply::not_found);
            return true;
        }

        // Fill out the reply to be sent to the client.
        char buf[512];
        while (is.read( buf, sizeof(buf) ).gcount() > 0) {
            rep.content.append( buf, static_cast<std::size_t>( is.gcount() ) ); // cast from int64 to uint32 is safe here since we have maximum 512 bytes.
        }

        const fs::path path(full_path);

        // fill http headers.
        rep.status = Reply::ok;
        rep.headers.resize(3);
        rep.headers[0].name = "Content-Length";
        rep.headers[0].value = boost::lexical_cast<std::string>( rep.content.size() );
        rep.headers[1].name = "Content-Type";
        rep.headers[1].value = mime_types::extension_to_type( path.extension().string().c_str() );
        rep.headers[2].name = "Content-Disposition";
        rep.headers[2].value = Utilities::MakeString() << "attachment; filename=\"" << path.filename() << "\"";
    } catch (std::exception&) {
        rep = Reply::stock_reply(Reply::not_found);
    }
    return true;
}

} // namespace DownloadTrack
