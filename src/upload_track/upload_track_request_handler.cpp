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

                aimp3_manager->addFileToPlaylist( path,
                                                  aimp3_manager->getPlayingPlaylist() );

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

} // namespace UploadTrack
