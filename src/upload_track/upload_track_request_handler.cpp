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

namespace UploadTrack
{

using namespace AIMPPlayer;
using namespace std;

namespace fs = boost::filesystem;

void fill_reply_disabled(Http::Reply& rep);
PlaylistID getPlaylistID(const std::string& uri);

const std::string kPlaylistIDTag("/playlist_id/");

bool RequestHandler::handle_request(const Http::Request& req, Http::Reply& rep)
{
    using namespace Http;

    if (!enabled_) {
        fill_reply_disabled(rep);
        return true;
    }

    try {
        const PlaylistID playlist_id = getPlaylistID(req.uri);

        for (auto field_it : req.mpfd_parser.GetFieldsMap()) {
            const MPFD::Field& field_const = *field_it.second;
            MPFD::Field& field = const_cast<MPFD::Field&>(field_const);

            switch (field.GetType()) {
            case MPFD::Field::FileType:
                {
                const std::string filename = field.GetFileName();
                const fs::wpath path = temp_dir_ / filename;

                { // save to temp dir.
                std::ofstream out(path.native(), std::ios_base::out | std::ios_base::binary);
                out.write(field.GetFileContent(), field.GetFileContentSize());
                out.close();
                }
            
                aimp_manager_.addFileToPlaylist(path, playlist_id);
                // we should not erase file since AIMP will use it.
                //fs::remove(path);
                break;
                }
            case MPFD::Field::TextType:
                {
                aimp_manager_.addURLToPlaylist(field.GetTextTypeContent(), playlist_id);
                break;
                }
            default:
                assert(!"unexpected type");
                break;
            }
        }
        rep = Reply::stock_reply(Reply::ok);
    } catch (MPFD::Exception&) {
        rep = Reply::stock_reply(Reply::bad_request);
    } catch (std::exception&) {
        rep = Reply::stock_reply(Reply::forbidden);
    }
    return true;
}

void fill_reply_disabled(Http::Reply& rep)
{
    rep.status = Http::Reply::forbidden;
    rep.content = "403 Forbidden<br/> Track upload is disabled in Control plugin settings.";
    rep.headers.resize(2);
    rep.headers[0].name = "Content-Length";
    rep.headers[0].value = boost::lexical_cast<std::string>(rep.content.size());
    rep.headers[1].name = "Content-Type";
    rep.headers[1].value = "text/html";
}

PlaylistID getPlaylistID(const std::string& uri)
{    
    size_t start_index = uri.find(kPlaylistIDTag);
    if (start_index == string::npos) {
        throw std::runtime_error("can't find playlist id tag in uri");
    }
    start_index += kPlaylistIDTag.length();
    const string id(uri.c_str(), start_index, uri.length() - start_index);
    const PlaylistID playlist_id = boost::lexical_cast<PlaylistID>(id);
    return playlist_id;
}

} // namespace UploadTrack
