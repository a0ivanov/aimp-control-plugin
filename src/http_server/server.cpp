// Copyright (c) 2003-2010 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include "stdafx.h"
#include "http_server/server.h"
#include "plugin/logger.h"
#include "utils/string_encoding.h"
#include "utils/util.h"
#include <boost/bind.hpp>

#include "connection.cpp"

const wchar_t * const NAME_SECURE = L"AimpControlPlugin"; // L"BluetoothChatSecure";
const GUID BLUETOOTH_SERVICE_UUID = { 0xfa87c0d0,
                                      0xafac,
                                      0x11de, 
                                      {0x8a, 0x39, 0x08, 0x00, 0x20, 0x0c, 0x9a, 0x66} 
                                     };

namespace {
using namespace ControlPlugin::PluginLogger;
ModuleLoggerType& logger()
    { return getLogManager().getModuleLogger<Http::Server>(); }
}

namespace Http
{

using ControlPlugin::PluginLogger::LogManager;
using namespace Utilities;

Server::Server( boost::asio::io_service& io_service,
                const std::string& address,
                const std::string& port,
                RequestHandler& request_handler
              )
    :
    io_service_(io_service),
    request_handler_(request_handler)
{
    try {
        open_specified_socket(address, port);
    } catch(std::exception& e) {
        using namespace Utilities;
        throw std::runtime_error( MakeString() << "Error in "__FUNCTION__": Failed to start server on '"
                                               << address << "':" << port << "' interface. Reason: " << e.what()
                                 );
    }
    
    try {
        open_localhost_socket(port);
    } catch (std::exception& e) {
        BOOST_LOG_SEV(logger(), warning) << "Failed to start server on localhost:" << port << ". Reason: " << e.what();
        // do nothing, it means that we already opened socket on localhost in open_specified_socket.
    }

    try {
        open_bluetooth_socket();
    } catch(std::exception& e) {
        BOOST_LOG_SEV(logger(), error) << "Bluetooth control is not available: failed to start bluetooth service. Reason: " << e.what();
        // proceed work of ip::tcp server.
    }
}

Server::~Server()
{
}

void Server::open_specified_socket(const std::string& address, const std::string& port)
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
        throw std::runtime_error( MakeString() << "Error in "__FUNCTION__": Failed to resolve endpoint for ip '"
                                               << address << "' and port '" << port << "'. Reason: " << e.what()
                                 );
    }

    ip::tcp::endpoint endpoint = *endpoint_iter_begin;
    try {
        start_accept_connections_on(endpoint);
    } catch(std::exception& e) { // this also handles boost::system::system_error exceptions from acceptor_ methods.
        throw std::runtime_error( MakeString() << "Error in "__FUNCTION__": Failed to start server on " << endpoint << " interface. Reason: " << e.what() );
    } catch(...) {
        throw std::runtime_error(MakeString() << "Error in "__FUNCTION__": Failed to start server on " << endpoint << " interface. Reason: unknown");
    }
}

void Server::open_localhost_socket(const std::string& port)
{
    using namespace boost::asio;

    ip::tcp::resolver resolver(io_service_);
    ip::tcp::resolver::query query(ip::tcp::v4(),
                                    "localhost",
                                    port,
                                    ip::tcp::resolver::query::passive
                                    );
    ip::tcp::resolver::iterator endpoint_iter = resolver.resolve(query);

    if ( endpoint_iter == ip::tcp::resolver::iterator() ) {
        throw std::exception("no endpoints were resolved");
    }
 
    start_accept_connections_on(*endpoint_iter);
}

void Server::start_accept_connections_on(boost::asio::ip::tcp::endpoint endpoint)
{
    using namespace boost::asio;

    // Acceptors used to listen for incoming connections over ip::tcp.
    IpTcpConnectionAcceptor_ptr acceptor( new IpTcpConnectionAcceptor(io_service_) );

    // Open the acceptor with the option to reuse the address (i.e. SO_REUSEADDR).
    acceptor->open( endpoint.protocol() );
    acceptor->set_option( ip::tcp::acceptor::reuse_address(true) );
    acceptor->bind(endpoint);
    acceptor->listen();

    // The next connection to be accepted.
    ConnectionIpTcp_ptr next_connection( new ConnectionIpTcp(io_service_, request_handler_) );
    acceptor->async_accept( next_connection->socket(),
                            boost::bind(&Server::handle_accept,
                                        this,
                                        acceptor,
                                        next_connection,
                                        placeholders::error)
                           );
}

void Server::open_bluetooth_socket()
{
    using namespace boost::asio;
    using namespace boost::asio::bluetooth;

    rfcomm::endpoint endpoint;

        // Acceptors used to listen for incoming connections over bluetooth::rfcomm.
    BluetoothConnectionAcceptor_ptr acceptor(new BluetoothConnectionAcceptor(io_service_));
    acceptor->open( endpoint.protocol() );
    acceptor->set_option( rfcomm::acceptor::reuse_address(true) );
    acceptor->bind(endpoint);
    acceptor->listen();
    
    ConnectionBluetoothRfcomm_ptr new_connection( new ConnectionBluetoothRfcomm(io_service_, request_handler_) );
    acceptor->async_accept( new_connection->socket(),
                            boost::bind(&Server::handle_accept_bluetooth,
                                        this,
                                        acceptor,
                                        new_connection,
                                        boost::asio::placeholders::error)
                          );

    register_bluetooth_service(acceptor);
}

void Server::handle_accept_bluetooth(BluetoothConnectionAcceptor_ptr acceptor,
                                     ConnectionBluetoothRfcomm_ptr accepted_connection,
                                     const boost::system::error_code& e)
{
    const boost::asio::bluetooth::rfcomm::socket::endpoint_type& endpoint = accepted_connection->socket().remote_endpoint();
    BOOST_LOG_SEV(logger(), info) << "Connection accepted from remote host " << endpoint;

    if (!e) {
        accepted_connection->start();
        BOOST_LOG_SEV(logger(), debug) << "Client connection started";
        ConnectionBluetoothRfcomm_ptr new_connection( new ConnectionBluetoothRfcomm(io_service_, request_handler_) );
        acceptor->async_accept( new_connection->socket(),
                                boost::bind(&Server::handle_accept_bluetooth,
                                            this,
                                            acceptor,
                                            new_connection,
                                            boost::asio::placeholders::error)
                              );
        BOOST_LOG_SEV(logger(), debug) << "Continue to waiting client connection";
    } else {
        BOOST_LOG_SEV(logger(), error) << "Error Server::handle_accept():" << e;
    }
}

void Server::register_bluetooth_service(BluetoothConnectionAcceptor_ptr acceptor)
{
    using namespace boost::asio::bluetooth;

    rfcomm::endpoint endpoint = acceptor->local_endpoint();
    
    WSAQUERYSET service = {0};
    service.dwSize = sizeof(service);
    service.lpszServiceInstanceName = const_cast<LPWSTR>(NAME_SECURE);
    service.lpszComment = L"Remote control of AIMP player";

    GUID serviceID = BLUETOOTH_SERVICE_UUID;
    service.lpServiceClassId = &serviceID;

    service.dwNumberOfCsAddrs = 1;
    service.dwNameSpace = NS_BTH;

    CSADDR_INFO csAddr = {0};
    csAddr.LocalAddr.iSockaddrLength = endpoint.size();
    csAddr.LocalAddr.lpSockaddr = endpoint.data();
    csAddr.iSocketType = SOCK_STREAM;
    csAddr.iProtocol = BTHPROTO_RFCOMM;
    service.lpcsaBuffer = &csAddr;

    if ( 0 != WSASetService(&service, RNRSERVICE_REGISTER, 0) ) {
        throw std::runtime_error( MakeString() << "Service registration failed. Last error code: %d" << GetLastError() ); // GetLastErrorMessage
    } else {
        BOOST_LOG_SEV(logger(), debug) << "Bluetooth service registration successful";
        BOOST_LOG_SEV(logger(), info) << "Bluetooth service UUID: " << service.lpServiceClassId;
    }
}

void Server::handle_accept(IpTcpConnectionAcceptor_ptr acceptor,
                           ConnectionIpTcp_ptr accepted_connection,
                           const boost::system::error_code& e)
{
    const boost::asio::ip::tcp::socket::endpoint_type& endpoint = accepted_connection->socket().remote_endpoint();
    BOOST_LOG_SEV(logger(), info) << "Connection accepted from remote host " << endpoint;

    if (!e) {
        accepted_connection->start();
        BOOST_LOG_SEV(logger(), debug) << "Client connection started";
        ConnectionIpTcp_ptr new_connection( new ConnectionIpTcp(io_service_, request_handler_) );
        acceptor->async_accept(new_connection->socket(),
                               boost::bind(&Server::handle_accept,
                                           this,
                                           acceptor,
                                           new_connection,
                                           boost::asio::placeholders::error)
                              );
        BOOST_LOG_SEV(logger(), debug) << "Continue to waiting client connection";
    } else {
        BOOST_LOG_SEV(logger(), error) << "Error Server::handle_accept():" << e;
    }
}

} // namespace Http
