// Copyright (c) 2003-2010 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include "stdafx.h"
#include "http_server/server.h"
#include "plugin/logger.h"
#include "plugin/settings.h"
#include "plugin/control_plugin.h"
#include "utils/string_encoding.h"
#include "utils/scope_guard.h"
#include "utils/util.h"
#include <boost/bind.hpp>
#include <iterator>

#include "connection.cpp"

#include <winsock2.h>
#include <iphlpapi.h>
#include <stdio.h>
#include <stdlib.h>
#pragma comment(lib, "IPHLPAPI.lib")

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
typedef ControlPlugin::PluginSettings::Settings::HttpServer::NetworkInterface NetworkInterface;

struct Endpoint
{
    std::string host;
    std::string port;

    Endpoint(std::string host, std::string port) : host(host), port(port) {}

    bool operator<(const Endpoint& rhs) const {
        return std::tie(host, port) < std::tie(rhs.host, rhs.port);
    }
};

std::set<Endpoint> getEndpointsFromSettings();

Server::Server( boost::asio::io_service& io_service, RequestHandler& request_handler)
    :
    io_service_(io_service),
    request_handler_(request_handler)
{
    std::set<Endpoint> endpoints = getEndpointsFromSettings();
    for (auto endpoint : endpoints) {
        try {
            open_specified_socket(endpoint.host, endpoint.port);
        } catch(std::exception& e) {
            using namespace Utilities;
            throw std::runtime_error( MakeString() << "Error in "__FUNCTION__": Failed to start server on '"
                                                   << endpoint.host << "':" << endpoint.port << "' interface. Reason: " << e.what()
                                     );
        }
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

namespace
{
    void* MALLOC(size_t size) {
        return HeapAlloc(GetProcessHeap(), 0, size);
    }
    void FREE(void* p) {
        HeapFree(GetProcessHeap(), 0, p);
    }
}

std::set<NetworkInterface> getIPaddressesOfAdaptersSpecifiedByMAC(std::set<NetworkInterface> interfaces_mac)
{
    PIP_ADAPTER_INFO pAdapter = NULL;
    ULONG ulOutBufLen = sizeof (IP_ADAPTER_INFO);
    PIP_ADAPTER_INFO pAdapterInfo = (IP_ADAPTER_INFO *) MALLOC(sizeof (IP_ADAPTER_INFO));
    if (pAdapterInfo == NULL) {
        throw std::runtime_error("Error allocating memory needed to call GetAdaptersinfo\n");
    }
    
    // Make an initial call to GetAdaptersInfo to get
    // the necessary size into the ulOutBufLen variable
    if (GetAdaptersInfo(pAdapterInfo, &ulOutBufLen) == ERROR_BUFFER_OVERFLOW) {
        FREE(pAdapterInfo);
        pAdapterInfo = (IP_ADAPTER_INFO *) MALLOC(ulOutBufLen);
        if (pAdapterInfo == NULL) {
            throw std::runtime_error("Error allocating memory needed to call GetAdaptersinfo\n");
        }
    }
    ON_BLOCK_EXIT(&FREE, pAdapterInfo);

    const std::string invalidIP("0.0.0.0");

    std::set<NetworkInterface> interfaces_ip;
    std::string mac;

    DWORD dwRetVal = 0;
    if ((dwRetVal = GetAdaptersInfo(pAdapterInfo, &ulOutBufLen)) == NO_ERROR) {
        pAdapter = pAdapterInfo;
        while (pAdapter) {
            switch (pAdapter->Type) {
            case MIB_IF_TYPE_ETHERNET:
            case IF_TYPE_IEEE80211:
                break;
            default:
                pAdapter = pAdapter->Next;
                continue;
            }

            // MAC address
            mac.clear();
            char byte_str[4];
            for (UINT i = 0; i < pAdapter->AddressLength; i++) {
                sprintf_s(byte_str, "%.2X-", (int) pAdapter->Address[i]);
                mac += byte_str;
            }
            mac.pop_back(); // remove '-'.
            boost::to_lower(mac);

            auto interface_mac_it = std::find_if( interfaces_mac.begin(), interfaces_mac.end(),
                                                  [mac](const NetworkInterface& i) {
                                                      return boost::to_lower_copy(i.mac) == mac;
                                                  }
                                                 );
            if (interface_mac_it == interfaces_mac.end()) {
                pAdapter = pAdapter->Next;
                continue;
            }

            // IP addresses
            IP_ADDR_STRING* ial = &pAdapter->IpAddressList;
            while (ial) {
                if (invalidIP != ial->IpAddress.String) {
                    NetworkInterface i("", ial->IpAddress.String, interface_mac_it->port);
                    interfaces_ip.insert(i);
                    ial->IpAddress.String;
                }
                ial = ial->Next;
            } 

            pAdapter = pAdapter->Next;
        }
    } else {
         BOOST_LOG_SEV(logger(), warning) << "GetAdaptersInfo failed with error: " << dwRetVal;
    }

    return interfaces_ip;
}

std::set<Endpoint> getEndpointsFromSettings()
{
    // Extract IP addresses of specified MAC addresses.
    const auto& settings = ControlPlugin::AIMPControlPlugin::settings();
    const std::set<NetworkInterface>& interfaces = settings.http_server.interfaces;
    std::set<NetworkInterface> interfaces_mac;
    std::copy_if(interfaces.begin(), interfaces.end(), std::inserter(interfaces_mac, interfaces_mac.end()),
                 [](const NetworkInterface& i) {
                     return !i.mac.empty();
                 }
                 );
    std::set<NetworkInterface> interfaces_ip_by_mac = getIPaddressesOfAdaptersSpecifiedByMAC(interfaces_mac);

    std::set<NetworkInterface> interfaces_ip_from_settings;
    std::copy_if(interfaces.begin(), interfaces.end(), std::inserter(interfaces_ip_from_settings, interfaces_ip_from_settings.end()),
                 [](const NetworkInterface& i) {
                     return i.mac.empty() && !i.ip.empty();
                 }
                 );


    std::set<NetworkInterface> interfaces_ip;
    std::set_union( interfaces_ip_from_settings.begin(), interfaces_ip_from_settings.end(),
                    interfaces_ip_by_mac.begin(), interfaces_ip_by_mac.end(),
                    std::inserter(interfaces_ip, interfaces_ip.end()));

    // check if all interfaces are requested in settings.
    std::set<Endpoint> endpoints;

    if (settings.http_server.all_interfaces.enabled()) {
        // remove all ip interfaces which have the same port.
        for ( auto it = interfaces_ip.begin(); it != interfaces_ip.end(); ) {
            if ( it->port == settings.http_server.all_interfaces.port ) {
                it = interfaces_ip.erase(it);
            } else {
                ++it;
            }
        }

        endpoints.emplace("", settings.http_server.all_interfaces.port);
    }

    for (auto i : interfaces_ip) {
        endpoints.emplace(i.ip, i.port);
        endpoints.emplace("localhost", i.port); // append localhost with correspond port for all interfaces.
    }

    return endpoints;
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

} // namespace Http
