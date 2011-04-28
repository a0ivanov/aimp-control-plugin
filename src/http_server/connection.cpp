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

namespace {
using namespace AIMPControlPlugin::PluginLogger;
ModuleLoggerType& logger()
    { return getLogManager().getModuleLogger<Http::Server>(); }
}

namespace Http {

Connection::Connection(boost::asio::io_service& io_service, RequestHandler& handler)
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

Connection::~Connection()
{
    try {
        BOOST_LOG_SEV(logger(), info) << "Destroying connection to host " << socket().remote_endpoint();
    } catch (boost::system::system_error&) {
        BOOST_LOG_SEV(logger(), info) << "Destroying connection.";
    }

}

boost::asio::ip::tcp::socket& Connection::socket()
{
    return socket_;
}

void Connection::start()
{
    socket_.async_read_some(boost::asio::buffer(buffer_),
                            strand_.wrap(boost::bind(&Connection::handle_read,
                                                     shared_from_this(),
                                                     boost::asio::placeholders::error,
                                                     boost::asio::placeholders::bytes_transferred
                                                     )
                                         )
                            );
}

void Connection::handle_read(const boost::system::error_code& e,
                             std::size_t bytes_transferred)
{
    if (!e) {
        boost::tribool result;
        boost::tie(result, boost::tuples::ignore) = request_parser_.parse(request_,
                                                                          buffer_.data(),
                                                                          buffer_.data() + bytes_transferred
                                                                         );
        if (result) {
            CometDelayedConnection_ptr comet_connection( new CometDelayedConnection( shared_from_this() ) );
            bool reply_immediately = request_handler_.handle_request(request_, reply_, comet_connection);
            if (reply_immediately) {
                boost::asio::async_write(socket_,
                                         reply_.to_buffers(),
                                         strand_.wrap(boost::bind(&Connection::handle_write,
                                                                  shared_from_this(),
                                                                  boost::asio::placeholders::error
                                                                  )
                                                      )
                                         );
            }
        } else if (!result) {
            reply_ = Reply::stock_reply(Reply::bad_request);
            boost::asio::async_write(socket_,
                                     reply_.to_buffers(),
                                     strand_.wrap(boost::bind(&Connection::handle_write,
                                                              shared_from_this(),
                                                              boost::asio::placeholders::error
                                                              )
                                                  )
                                     );
        } else {
            socket_.async_read_some(boost::asio::buffer(buffer_),
                                    strand_.wrap(boost::bind(&Connection::handle_read,
                                                             shared_from_this(),
                                                             boost::asio::placeholders::error,
                                                             boost::asio::placeholders::bytes_transferred
                                                             )
                                                 )
                                    );
        }
    }

    // If an error occurs then no new asynchronous operations are started. This
    // means that all shared_ptr references to the connection object will
    // disappear and the object will be destroyed automatically after this
    // handler returns. The Connection class's destructor closes the socket.
}

void Connection::handle_write(const boost::system::error_code& e)
{
    if (!e) {
        // Initiate graceful connection closure.
        boost::system::error_code ignored_ec;
        socket_.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ignored_ec);
    }

    // No new asynchronous operations are started. This means that all shared_ptr
    // references to the connection object will disappear and the object will be
    // destroyed automatically after this handler returns. The Connection class's
    // destructor closes the socket.
}

void CometDelayedConnection::sendResponse(DelayedResponseSender_ptr comet_http_response_sender, const Reply& reply)
{
    BOOST_LOG_SEV(logger(), debug) << "CometDelayedConnection::sendResponse to " << connection_->socket_.remote_endpoint();
    boost::asio::async_write( connection_->socket_,
                              reply.to_buffers(),
                              connection_->strand_.wrap(boost::bind(&CometDelayedConnection::handle_write,
                                                                    shared_from_this(),
                                                                    comet_http_response_sender,
                                                                    boost::asio::placeholders::error
                                                                    )
                                                         )
                             );
}

void CometDelayedConnection::handle_write(DelayedResponseSender_ptr comet_http_response_sender, const boost::system::error_code& e)
{
    if (!e) {
        BOOST_LOG_SEV(logger(), debug) << "CometDelayedConnection::success sending response to " << connection_->socket_.remote_endpoint();
        // Initiate graceful connection closure.
        boost::system::error_code ignored_ec;
        connection_->socket_.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ignored_ec);
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

std::string CometDelayedConnection::getRemoteIP() const
{
    return connection_->socket_.remote_endpoint().address().to_string();
}

} // namespace Http
