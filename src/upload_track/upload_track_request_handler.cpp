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

bool RequestHandler::handle_request(const Http::Request& /*req*/, Http::Reply& rep)
{
    using namespace Http;

    try {
        rep = Reply::stock_reply(Reply::ok);
    } catch (std::exception&) {
        rep.filename.clear();
        rep = Reply::stock_reply(Reply::not_found);
    }
    return true;
}

} // namespace UploadTrack
