// Copyright (c) 2003-2010 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

// Copyright (c) 2011, Alexey Ivanov

#include "stdafx.h"
#include "http_server/request_handler.h"
#include "download_track/request_handler.h"
#include <fstream>
#include <sstream>
#include <string>
#include <boost/lexical_cast.hpp>
#include "reply.h"
#include "request.h"
#include "mime_types.h"
#include "rpc/request_handler.h"

#include "utils/string_encoding.h"
#include "utils/util.h"

namespace Http {

bool RequestHandler::handle_request(const Request& req, Reply& rep, ICometDelayedConnection_ptr connection)
{
    if ( Rpc::Frontend* frontend = rpc_request_handler_.getFrontEnd(req.uri) ) { // handle RPC call.
        std::string response_content_type;
        DelayedResponseSender_ptr comet_delayed_response_sender( new DelayedResponseSender(connection, *this) );

        boost::tribool result = rpc_request_handler_.handleRequest(req.uri,
                                                                   req.content,
                                                                   comet_delayed_response_sender,
                                                                   *frontend,
                                                                   &rep.content,
                                                                   &response_content_type
                                                                   );
        if (result || !result) {
            fillReplyWithContent(response_content_type, rep);
            return true; // response will be sent immediately.
        }

        return false; // response sending will be delayed.
    } else if (req.uri.find("/downloadTrack/") == 0) { // handle special download track request.
        return handleDownloadTrackRequest(req, rep);
    } else {
        handle_file_request(req, rep);
    }

    return true;
}

bool RequestHandler::handleDownloadTrackRequest(const Request& req, Reply& rep)
{
    try {
        namespace fs = boost::filesystem;
        const std::wstring track_source_path( download_track_request_handler_.getTrackSourcePath(req.uri) );

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
        fillReplyWithContent(mime_types::extension_to_type( path.extension().c_str() ), rep);

        { // add Content-Disposition header.
            Http::header header;
            header.name = "Content-Disposition";
            header.value= Utilities::MakeString() << "attachment; filename=\"" << path.filename() << "\"";
            rep.headers.push_back(header);
        }
    } catch (std::exception&) {
        rep = Reply::stock_reply(Reply::not_found);
    }
    return true;
}

void RequestHandler::handle_file_request(const Request& req, Reply& rep)
{
    // Decode url to path.
    std::string request_path;
    if ( !url_decode(req.uri, request_path) ) {
        rep = Reply::stock_reply(Reply::bad_request);
        return;
    }

    // Request path must be absolute and not contain "..".
    if (request_path.empty() || request_path[0] != '/'
        || request_path.find("..") != std::string::npos)
    {
        rep = Reply::stock_reply(Reply::bad_request);
        return;
    }

    // If path ends in slash (i.e. is a directory) then add "index.htm".
    if (request_path[request_path.size() - 1] == '/') {
        request_path += "index.htm";
    }

    // Determine the file extension.
    std::size_t last_slash_pos = request_path.find_last_of("/");
    std::size_t last_dot_pos = request_path.find_last_of(".");
    std::string extension;
    if (last_dot_pos != std::string::npos && last_dot_pos > last_slash_pos) {
        extension = request_path.substr(last_dot_pos);
    }

    // Open the file to send back.
    std::string full_path = document_root_ + request_path;
    std::ifstream is(full_path.c_str(), std::ios::in | std::ios::binary);
    if (!is) {
        rep = Reply::stock_reply(Reply::not_found);
        return;
    }

    // Fill out the reply to be sent to the client.
    char buf[512];
    while (is.read( buf, sizeof(buf) ).gcount() > 0) {
        rep.content.append( buf, static_cast<std::size_t>( is.gcount() ) ); // cast from int64 to uint32 is safe here since we have maximum 512 bytes.
    }

    fillReplyWithContent(mime_types::extension_to_type(extension), rep);
}

void RequestHandler::fillReplyWithContent(const std::string& content_type, Reply& rep)
{
    // Fill out the reply to be sent to the client.
    rep.status = Reply::ok;
    rep.headers.resize(2);
    rep.headers[0].name = "Content-Length";
    rep.headers[0].value = boost::lexical_cast<std::string>( rep.content.size() );
    rep.headers[1].name = "Content-Type";
    rep.headers[1].value = content_type;
}

bool RequestHandler::url_decode(const std::string& in, std::string& out)
{
    out.clear();
    out.reserve( in.size() );
    for (std::size_t i = 0; i < in.size(); ++i) {
        if (in[i] == '%') {
            if ( i + 3 <= in.size() ) {
                int value = 0;
                std::istringstream is(in.substr(i + 1, 2));
                if (is >> std::hex >> value) {
                    out += static_cast<char>(value);
                    i += 2;
                } else {
                    return false;
                }
            } else {
                return false;
            }
        }
        else if (in[i] == '+') {
            out += ' ';
        } else {
            out += in[i];
        }
    }

    return true;
}

const Reply& DelayedResponseSender::get_reply() const
{ 
    return reply_;
}

void DelayedResponseSender::send(const std::string& response, const std::string& response_content_type)
{
    reply_.content = response;
    http_request_handler_.fillReplyWithContent(response_content_type, reply_);
    comet_connection_->sendResponse( shared_from_this() );
}

} // namespace Http
