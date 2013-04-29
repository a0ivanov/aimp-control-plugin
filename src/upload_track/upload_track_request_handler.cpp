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

const wchar_t * const kPlaylistTitle = L"Control plugin";

bool RequestHandler::handle_request(const Http::Request& req, Http::Reply& rep)
{
    using namespace Http;

    if ( AIMPPlayer::AIMPManager30* aimp3_manager = dynamic_cast<AIMPPlayer::AIMPManager30*>(&aimp_manager_) ) {
        try {
            for (auto field_it : req.mpfd_parser.GetFieldsMap()) {
                const MPFD::Field& field_const = *field_it.second;
                MPFD::Field& field = const_cast<MPFD::Field&>(field_const);

                const std::string filename = field.GetFileName();
                const fs::wpath path = temp_dir_ / filename;

                { // save to temp dir.
                std::ofstream out(path.native(), std::ios_base::out | std::ios_base::binary);
                out.write(field.GetFileContent(), field.GetFileContentSize());
                out.close();
                }
                
                aimp3_manager->addFileToPlaylist(path, getTargetPlaylist());

                // we should not erase file since AIMP will use it.
                //fs::remove(path);
                
            }
            rep = Reply::stock_reply(Reply::ok);
        } catch (MPFD::Exception&) {
            rep = Reply::stock_reply(Reply::bad_request);
        } catch (std::exception&) {
            rep = Reply::stock_reply(Reply::forbidden);
        }
    }
    return true;
}

bool getPlaylistByTitle(sqlite3* db, const char* title, PlaylistID* playlist_id);

int RequestHandler::getTargetPlaylist()
{
    if ( AIMPPlayer::AIMPManager30* aimp3_manager = dynamic_cast<AIMPPlayer::AIMPManager30*>(&aimp_manager_) ) {
        if (!target_playlist_id_created_) {
            if (!getPlaylistByTitle(getPlaylistsDB(aimp_manager_),
                                    StringEncoding::utf16_to_utf8(kPlaylistTitle).c_str(),
                                    &target_playlist_id_)
                )
            {
                target_playlist_id_ = aimp3_manager->createPlaylist(kPlaylistTitle);
            }
            
            target_playlist_id_created_ = true;
        }
        return target_playlist_id_;
    }

    throw std::runtime_error(__FUNCTION__": AIMP3 is supported only.");
}

bool getPlaylistByTitle(sqlite3* db, const char* title, PlaylistID* playlist_id)
{
    using namespace Utilities;

    std::ostringstream query;

    query << "SELECT id FROM Playlists WHERE title = ?";

    sqlite3_stmt* stmt = createStmt( db, query.str() );
    ON_BLOCK_EXIT(&sqlite3_finalize, stmt);

    int rc_db = sqlite3_bind_text(stmt, 1, title, strlen(title), SQLITE_STATIC);
    if (SQLITE_OK != rc_db) {
        const std::string msg = MakeString() << "sqlite3_bind_text16() error " << rc_db;
        throw std::runtime_error(msg);
    }

    for(;;) {
		rc_db = sqlite3_step(stmt);
        if (SQLITE_ROW == rc_db) {
            assert(sqlite3_column_count(stmt) == 1);
            if (playlist_id) {
                *playlist_id = sqlite3_column_int(stmt, 0);
            }
            return true;
        } else if (SQLITE_DONE == rc_db) {
            break;
        } else {
            const std::string msg = MakeString() << "sqlite3_step() error "
                                                 << rc_db << ": " << sqlite3_errmsg(db)
                                                 << ". Query: " << query.str();
            throw std::runtime_error(msg);
		}
    }

    return false;
}

} // namespace UploadTrack
