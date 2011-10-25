// Copyright (c) 2003-2010 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include "stdafx.h"
#include <vector>
#include <boost/bind.hpp>
#include "connection.h"
#include "http_server/request_handler.h"
#include "plugin/logger.h"

//namespace {
//using namespace AIMPControlPlugin::PluginLogger;
//ModuleLoggerType& logger()
//    { return getLogManager().getModuleLogger<Http::Server>(); }
//}

namespace Http {

template <typename SocketT>
Connection<SocketT>::Connection(boost::asio::io_service& io_service, RequestHandler& handler)
    :
    strand_(io_service),
    socket_(io_service),
    request_handler_(handler)
{
    try {
        BOOST_LOG_SEV(logger(), info) << "Creating connection to host " << socket().remote_endpoint();
    } catch (boost::system::system_error&) {
        BOOST_LOG_SEV(logger(), info) << "Creating connection.";
    }
}

template <typename SocketT>
Connection<SocketT>::~Connection()
{
    try {
        BOOST_LOG_SEV(logger(), info) << "Destroying connection to host " << socket().remote_endpoint();
    } catch (boost::system::system_error&) {
        BOOST_LOG_SEV(logger(), info) << "Destroying connection.";
    }
}

template <typename SocketT>
SocketT& Connection<SocketT>::socket()
{
    return socket_;
}

template <typename SocketT>
void Connection<SocketT>::start()
{
    read_some_to_buffer();
}

template <typename SocketT>
void Connection<SocketT>::read_some_to_buffer()
{
    socket_.async_read_some(boost::asio::buffer(buffer_),
                            strand_.wrap(boost::bind(&Connection<SocketT>::handle_read,
                                                     shared_from_this(),
                                                     boost::asio::placeholders::error,
                                                     boost::asio::placeholders::bytes_transferred
                                                     )
                                         )
                            );
}

template <typename SocketT>
void Connection<SocketT>::write_reply_content()
{
    boost::asio::async_write(socket_,
                             reply_.to_buffers(),
                             strand_.wrap(boost::bind(&Connection<SocketT>::handle_write,
                                                      shared_from_this(),
                                                      boost::asio::placeholders::error
                                                      )
                                          )
                             );
}

template <typename SocketT>
void Connection<SocketT>::handle_read(const boost::system::error_code& e,
                             std::size_t bytes_transferred)
{
    if (!e) {
        boost::tribool result;
        boost::tie(result, boost::tuples::ignore) = request_parser_.parse(request_,
                                                                          buffer_.data(),
                                                                          buffer_.data() + bytes_transferred
                                                                          );
        if (result) {
            ICometDelayedConnection_ptr comet_connection( new CometDelayedConnection<SocketT>( shared_from_this() ) );
            bool reply_immediately = request_handler_.handle_request(request_, reply_, comet_connection);
            if (reply_immediately) {
                write_reply_content();
            }
        } else if (!result) {
            reply_ = Reply::stock_reply(Reply::bad_request);
            write_reply_content();
        } else {
            read_some_to_buffer();
        }
    } else {
        // BOOST_LOG_SEV(logger(), debug) << "Connection<SocketT>::handle_read(): failed to read data. Reason: " << e.message();
    }

    // If an error occurs then no new asynchronous operations are started. This
    // means that all shared_ptr references to the connection object will
    // disappear and the object will be destroyed automatically after this
    // handler returns. The Connection class's destructor closes the socket.
}

template <typename SocketT>
void Connection<SocketT>::handle_write(const boost::system::error_code& e)
{
    if (!e) {
        // Initiate graceful connection closure.
        boost::system::error_code ignored_ec;
        socket_.shutdown(SocketT::shutdown_both, ignored_ec);
    }

    // No new asynchronous operations are started. This means that all shared_ptr
    // references to the connection object will disappear and the object will be
    // destroyed automatically after this handler returns. The Connection class's
    // destructor closes the socket.
}

template <typename SocketT>
void CometDelayedConnection<SocketT>::sendResponse(DelayedResponseSender_ptr comet_http_response_sender)
{
    BOOST_LOG_SEV(logger(), debug) << "CometDelayedConnection::sendResponse to " << connection_->socket().remote_endpoint();
    boost::asio::async_write( connection_->socket(),
                              comet_http_response_sender->get_reply().to_buffers(),
                              connection_->strand_.wrap(boost::bind(&CometDelayedConnection<SocketT>::handle_write,
                                                                    shared_from_this(),
                                                                    comet_http_response_sender,
                                                                    boost::asio::placeholders::error
                                                                    )
                                                         )
                             );
}

template <typename SocketT>
void CometDelayedConnection<SocketT>::handle_write(DelayedResponseSender_ptr comet_http_response_sender, const boost::system::error_code& e)
{
    if (!e) {
        BOOST_LOG_SEV(logger(), debug) << "CometDelayedConnection::success sending response to " << connection_->socket().remote_endpoint();
        // Initiate graceful connection closure.
        boost::system::error_code ignored_ec;
        connection_->socket().shutdown(SocketT::shutdown_both, ignored_ec);
    } else {
        BOOST_LOG_SEV(logger(), debug) << "CometDelayedConnection::fail to send response to "
                                       << connection_->socket_.remote_endpoint()
                                       << ". Reason: " << e.message();
    }

    // No new asynchronous operations are started. This means that all shared_ptr
    // references to the CometDelayedConnection object will disappear and the object will be
    // destroyed automatically after this handler returns. The Connection class's
    // destructor closes the socket.
}

} // namespace Http
