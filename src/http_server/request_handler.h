// Copyright (c) 2003-2010 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

// Copyright (c) 2014, Alexey Ivanov

#ifndef HTTP_REQUEST_HANDLER_H
#define HTTP_REQUEST_HANDLER_H

#include <string>
#include <map>
#include <boost/noncopyable.hpp>
#include "connection.h"

// headers for DelayedResponseSender class
#include <boost/enable_shared_from_this.hpp>
#include "http_server/auth_manager.h"

namespace Rpc           { class RequestHandler; }
namespace DownloadTrack { class RequestHandler; }
namespace UploadTrack   { class RequestHandler; }

namespace Http
{

struct Reply;
struct Request;

/// The common handler for all incoming requests.
class RequestHandler : private boost::noncopyable
{
    friend class DelayedResponseSender;

public:

    /// Construct with document root directory and Rpc handler.
    explicit RequestHandler(const std::string& document_root,
                            Rpc::RequestHandler& rpc_request_handler,
                            DownloadTrack::RequestHandler& download_track_request_handler,
                            UploadTrack::RequestHandler& upload_track_request_handler)
        :
        document_root_(document_root),
        rpc_request_handler_(rpc_request_handler),
        download_track_request_handler_(download_track_request_handler),
        upload_track_request_handler_(upload_track_request_handler)
    {}

    /*
        Handle a request and produce a reply.
        Return true if reply should be sent to client immediately, false if response sending must be delayed.
    */
    bool handle_request(const Request& req, Reply& rep, ICometDelayedConnection_ptr connection);

private:

    void handle_file_request(const Request& req, Reply& rep);

    // Perform URL-decoding on a string. \return false if the encoding was invalid.
    static bool url_decode(const std::string& in, std::string& out);

    /*
        Fill headers of reply with content.
        Note: content should be assinged before call this method.
    */
    static void fillReplyWithContent(const std::string& content_type, Reply& rep);

    void fillAuthFailReply(Reply& rep);

    void trySendInitCookies(const Request& req, Reply& rep);

    // The directory containing the files to be served.
    std::string document_root_;

    Authentication::AuthManager auth_manager_;

    Rpc::RequestHandler& rpc_request_handler_;
    DownloadTrack::RequestHandler& download_track_request_handler_;
    UploadTrack::RequestHandler& upload_track_request_handler_;
};


class ClientDescriptor;

class DelayedResponseSender : public boost::enable_shared_from_this<DelayedResponseSender>, private boost::noncopyable
{
public:
    DelayedResponseSender(ICometDelayedConnection_ptr comet_connection,
                          RequestHandler& http_request_handler)
        :
        comet_connection_(comet_connection),
        http_request_handler_(http_request_handler)
    {}

    void send(const std::string& response, const std::string& response_content_type);

    const Reply& get_reply() const;

private:

    ICometDelayedConnection_ptr comet_connection_;
    RequestHandler& http_request_handler_;
    Reply reply_; // Reply object is member since it should exist till connection send it to client.
};

typedef boost::shared_ptr<DelayedResponseSender> DelayedResponseSender_ptr;

//! descriptor of HTTP request caller.
class ClientDescriptor
{
public:
    explicit ClientDescriptor(const std::string& ip_addr)
        :
        ip_addr_(ip_addr)
    {}

    const std::string& getIpAddress() const
        { return ip_addr_; }

private:

    std::string ip_addr_; //!< client IP address.
};

} // namespace Http

#endif // HTTP_REQUEST_HANDLER_H
