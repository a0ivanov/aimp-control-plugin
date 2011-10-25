// Copyright (c) 2003-2010 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef CONNECTION_H
#define CONNECTION_H

#include <boost/asio.hpp>
#include <boost/array.hpp>
#include <boost/noncopyable.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>
#include "reply.h"
#include "request.h"
#include "request_parser.h"

namespace Http {

class RequestHandler;

/// Represents a single tcp-ip connection from a client.
template <typename SocketT>
class Connection : /*public Connection, */public boost::enable_shared_from_this< Connection<SocketT> >, private boost::noncopyable
{
    template <typename T>
    friend class CometDelayedConnection;

public:

    /// Construct a connection with the given io_service.
    Connection(boost::asio::io_service& io_service,
               RequestHandler& handler);

    ~Connection();
    
    /// Start the first asynchronous operation for the connection.
    void start();

    /// Get the socket associated with the connection.
    SocketT& socket();

private:

    void read_some_to_buffer();

    void write_reply_content();

    /// Handle completion of a read operation.
    void handle_read(const boost::system::error_code& e, std::size_t bytes_transferred);

    /// Handle completion of a write operation.
    void handle_write(const boost::system::error_code& e);

    /// Strand to ensure the connection's handlers are not called concurrently.
    boost::asio::io_service::strand strand_;

    /// Socket for the connection.
    SocketT socket_;

    /// The handler used to process the incoming request.
    RequestHandler& request_handler_;

    /// Buffer for incoming data.
    boost::array<char, 8192> buffer_;

    /// The incoming request.
    Request request_;

    /// The parser for the incoming request.
    request_parser request_parser_;

    /// The reply to be sent back to the client.
    Reply reply_;
};


class DelayedResponseSender;

class ICometDelayedConnection
{
public:
    virtual ~ICometDelayedConnection() {}

    virtual void sendResponse(boost::shared_ptr<Http::DelayedResponseSender> comet_http_response_sender) = 0;
};

typedef boost::shared_ptr<ICometDelayedConnection> ICometDelayedConnection_ptr;

template<typename SocketT>
class CometDelayedConnection : public ICometDelayedConnection, public boost::enable_shared_from_this< CometDelayedConnection<SocketT> >
{
    typedef Connection<SocketT> ConnectionType;
    typedef boost::shared_ptr<ConnectionType> ConnectionType_ptr;

public:
    CometDelayedConnection(ConnectionType_ptr connection)
        :
        connection_(connection)
    {}

    virtual void sendResponse(boost::shared_ptr<Http::DelayedResponseSender> comet_http_response_sender);

private:

    void handle_write(boost::shared_ptr<Http::DelayedResponseSender> comet_http_response_sender, const boost::system::error_code& e);

    ConnectionType_ptr connection_;
};

} // namespace Http

#endif // CONNECTION_H
