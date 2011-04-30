// Copyright (c) 2003-2010 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include "stdafx.h"
#include "http_server/server.h"
#include "plugin/logger.h"
#include "utils/string_encoding.h"
#include <boost/bind.hpp>

namespace {
using namespace AIMPControlPlugin::PluginLogger;
ModuleLoggerType& logger()
    { return getLogManager().getModuleLogger<Http::Server>(); }
}

namespace Http
{

using AIMPControlPlugin::PluginLogger::LogManager;

Server::Server( boost::asio::io_service& io_service,
                const std::string& address,
                const std::string& port,
                RequestHandler& request_handler
              )
    :
    io_service_(io_service),
    acceptor_(io_service_),
    request_handler_(request_handler),
    new_connection_( new Connection(io_service_, request_handler_) )
{
    using namespace boost::asio;

    ip::tcp::resolver::iterator endpoint_iter_begin;

    try {
        ip::tcp::resolver resolver(io_service_);
        ip::tcp::resolver::query query(ip::tcp::v4(),
                                       address,
                                       port,
                                       ip::tcp::resolver::query::passive
                                       );
        ip::tcp::resolver::iterator endpoint_iter = resolver.resolve(query),
                                    endpoint_iter_end;
        endpoint_iter_begin = endpoint_iter;

        if (endpoint_iter_begin == endpoint_iter_end) {
            throw std::exception("no endpoints were resolved");
        }

        BOOST_LOG_SEV(logger(), debug) << "Resolved endpoints for '" << address << ":" << port << "':";
        for (; endpoint_iter != endpoint_iter_end; ++endpoint_iter) {
            BOOST_LOG_SEV(logger(), debug) << ip::tcp::endpoint(*endpoint_iter);
        }
    } catch(std::exception& e) {
        std::ostringstream msg;
        msg << "Error in "__FUNCTION__": Failed to resolve endpoint for ip '" << address << "' and port '" << port << "'. Reason: " << e.what();
        throw std::runtime_error( msg.str() );
    }

    ip::tcp::endpoint endpoint;

    try {
        endpoint = *endpoint_iter_begin;

        // Open the acceptor with the option to reuse the address (i.e. SO_REUSEADDR).
        acceptor_.open( endpoint.protocol() );
        acceptor_.set_option( ip::tcp::acceptor::reuse_address(true) );
        acceptor_.bind(endpoint);
        acceptor_.listen();
        acceptor_.async_accept( new_connection_->socket(),
                                boost::bind(&Server::handle_accept,
                                            this,
                                            placeholders::error)
                              );
    } catch(std::exception& e) { // this also handles boost::system::system_error exceptions from acceptor_ methods.
        std::ostringstream msg;
        msg << "Error in "__FUNCTION__": Failed to start server on " << endpoint << " interface. Reason: " << e.what();
        throw std::runtime_error( msg.str() );
    } catch(...) {
        std::ostringstream msg;
        msg << "Error in "__FUNCTION__": Failed to start server on " << endpoint << " interface. Reason: unknown";
        throw std::runtime_error( msg.str() );
    }
}

Server::~Server()
{}

void Server::handle_accept(const boost::system::error_code& e)
{
    const boost::asio::ip::tcp::socket::endpoint_type& endpoint = new_connection_->socket().remote_endpoint();
    BOOST_LOG_SEV(logger(), info) << "Connection accepted from remote host " << endpoint;

    if (!e) {
        new_connection_->start();
        BOOST_LOG_SEV(logger(), info) << "Client connection started";
        new_connection_.reset( new Connection(io_service_, request_handler_) );
        acceptor_.async_accept( new_connection_->socket(),
                                boost::bind(&Server::handle_accept,
                                            this,
                                            boost::asio::placeholders::error)
                              );
        BOOST_LOG_SEV(logger(), info) << "Continue to waiting client connection";
    } else {
        BOOST_LOG_SEV(logger(), error) << "Error Server::handle_accept():" << e;
    }
}

} // namespace Http
